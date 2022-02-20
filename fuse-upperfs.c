#define _GNU_SOURCE
#define FUSE_USE_VERSION 35
#define _FILE_OFFSET_BITS 64

#include <err.h>
#include <errno.h>
#include <error.h>
#include <fuse_lowlevel.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <dirent.h>

void u_log(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

struct hash_map;

struct lo_inode {
  struct lo_inode *next;
  struct lo_inode *prev;
  size_t name_hash;
  int fd;
  ino_t ino;
  dev_t dev;
  int refcount;  // ref count
};


enum {
  CACHE_NEVER,
  CACHE_NORMAL,
  CACHE_ALWAYS,
};

struct lo_data {
  pthread_mutex_t mutex;
  int debug;
  int writeback;
  int flock;
  int xattr;
  const char* source;
  const char* cache_source;
  double timeout;
  int cache;
  int timeout_set;
  struct hash_map* inodes;
  struct lo_inode root;
};

static struct lo_data *lo_data(fuse_req_t req)
{
  return (struct lo_data *) fuse_req_userdata(req);
}


static struct lo_inode *lo_inode(fuse_req_t req, fuse_ino_t ino)
{
  if (ino == FUSE_ROOT_ID)
  {
    return &lo_data(req)->root;
  }
  else
    return (struct lo_inode *)(uintptr_t)ino;
}

static int lo_fd(fuse_req_t req, fuse_ino_t ino)
{
  return lo_inode(req, ino)->fd;
}

static const struct fuse_opt lo_opts[] = {
    {"writeback", offsetof(struct lo_data, writeback), 1},
    {"no_writeback", offsetof(struct lo_data, writeback), 0},
    {"source=%s", offsetof(struct lo_data, source), 0},
    {"cache_source=%s", offsetof(struct lo_data, cache_source), 0},
    {"flock", offsetof(struct lo_data, flock), 1},
    {"no_flock", offsetof(struct lo_data, flock), 0},
    {"xattr", offsetof(struct lo_data, xattr), 1},
    {"no_xattr", offsetof(struct lo_data, xattr), 0},
    {"timeout=%lf", offsetof(struct lo_data, timeout), 0},
    {"timeout=", offsetof(struct lo_data, timeout_set), 1},
    {"cache=never", offsetof(struct lo_data, cache), CACHE_NEVER},
    {"cache=auto", offsetof(struct lo_data, cache), CACHE_NORMAL},
    {"cache=always", offsetof(struct lo_data, cache), CACHE_ALWAYS},

    FUSE_OPT_END};


void maximize_fd_limit() {
  struct rlimit lim;
  int res = getrlimit(RLIMIT_NOFILE, &lim);
  if (res < 0) error(EXIT_FAILURE, errno, "read nofile rlimit failed");
  lim.rlim_cur = lim.rlim_max;
  res = setrlimit(RLIMIT_NOFILE, &lim);
  if (res != 0) error(EXIT_FAILURE, errno, "write nofile rlimit failed");
}

static struct lo_inode *lo_find(struct lo_data *lo, struct stat *st)
{
  struct lo_inode *p;
  struct lo_inode *ret = NULL;

  pthread_mutex_lock(&lo->mutex);

  for (p = lo->root.next; p != &lo->root; p = p->next)
    if (p->ino == st->st_ino && p->dev == st->st_dev)
    {
      ret = p;
      ret->refcount++;
      break;
    }
  pthread_mutex_unlock(&lo->mutex);
  return ret;
}

static int lo_do_lookup(fuse_req_t req, fuse_ino_t parent, const char *name, struct fuse_entry_param *e)
{
  struct lo_data *lo = lo_data(req);
  struct lo_inode *inode;
  int saverr;

  memset(e, 0, sizeof(*e));
  e->attr_timeout = lo->timeout;
  e->entry_timeout = lo->timeout;

  int newfd = openat(lo_fd(req, parent), name, O_PATH | O_NOFOLLOW);
  if (newfd == -1)
    goto out_err;

  int res = fstatat(newfd, "", &e->attr, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);
  if (res == -1)
    goto out_err;

  inode = lo_find(lo_data(req), &e->attr);
  if (inode)
  {
    close(newfd);
    newfd = -1;
  }
  else
  {
    struct lo_inode *prev, *next;

    saverr = ENOMEM;
    inode = calloc(1, sizeof(struct lo_inode));
    if (!inode)
      goto out_err;

    inode->refcount = 1;
    inode->fd = newfd;
    inode->ino = e->attr.st_ino;
    inode->dev = e->attr.st_dev;

    pthread_mutex_lock(&lo->mutex);

    prev = &lo->root;
    next = prev->next;
    next->prev = inode;

    inode->next = next;
    inode->prev = prev;
    prev->next = inode;
    pthread_mutex_unlock(&lo->mutex);
  }
  e->ino = (uintptr_t)inode;

  if (lo->debug)
    u_log(" %lli/%s -> %lli\n", (unsigned long long) parent, name, (unsigned long long) e->ino);
  return 0;
out_err:
  saverr = errno;
  if (newfd != -1)
    close(newfd);
  return saverr;
}

//void up_init(void* userdata, struct fuse_conn_info* conn) {}
//void up_destroy(void *userdata){}
void up_lookup(fuse_req_t req, fuse_ino_t parent, const char *name){
  struct fuse_entry_param e;
  struct lo_data *lo = lo_data(req);

  if (lo->debug)
    u_log("lo_lookup(parent=%" PRIu64 ", name=%s)\n", parent, name);

  int err = lo_do_lookup(req, parent, name, &e);
  if (err)
    fuse_reply_err(req, err);
  else
    fuse_reply_entry(req, &e);
}
//void up_statfs(fuse_req_t req, fuse_ino_t ino){}
//void up_access(fuse_req_t req, fuse_ino_t ino, int mask){}
//void up_getxattr(fuse_req_t req, fuse_ino_t ino, const char *name, size_t size){}
//void up_removexattr(fuse_req_t req, fuse_ino_t ino, const char *name){}
//void up_setxattr(fuse_req_t req, fuse_ino_t ino, const char *name, const char *value, size_t size, int flags){}
//void up_listxattr(fuse_req_t req, fuse_ino_t ino,size_t size){}
//void up_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup){}
//void up_forget_multi(fuse_req_t req,  size_t count, struct fuse_forget_data *forgets){}
void up_getattr(fuse_req_t req, fuse_ino_t ino, __attribute__((unused))struct fuse_file_info *fi){
  struct lo_data *lo = lo_data(req);

  struct stat buf;
  int res = fstatat(lo_fd(req, ino), "", &buf, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);
  if (res == -1)
    return (void) fuse_reply_err(req, errno);

  fuse_reply_attr(req, &buf, lo->timeout);
}
//void up_readlink(fuse_req_t req, fuse_ino_t ino){}
//
struct lo_dirp {
  DIR *dp;
  struct dirent *entry;
  off_t offset;
};

void up_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
  int error = ENOMEM;

  struct lo_dirp *d = calloc(1, sizeof(struct lo_dirp));
  if (d == NULL)
    goto out_err;

  int fd = openat(lo_fd(req, ino), ".", O_RDONLY);
  if (fd == -1)
    goto out_errno;

  d->dp = fdopendir(fd);
  if (d->dp == NULL)
    goto out_errno;

  d->offset = 0;
  d->entry = NULL;
  fi->fh = (uintptr_t) d;
  fuse_reply_open(req, fi);
  return;
out_errno:
  error = errno;
out_err:
  if (d)
  {
    if (fd != -1)
      close(fd);
    free(d);
  }
  fuse_reply_err(req, error);
}
//void up_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi){}
//void up_readdirplus(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi){}
//void up_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){}
//void up_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi){}
void up_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
  int fd;
  char buf[64];

  struct lo_data *lo = lo_data(req);

  if (lo->debug != 0)
    u_log("lo_open(ino=%" PRIu64 ", flags=%d)\n", ino, fi->flags);

  if (lo->writeback && (fi->flags & O_ACCMODE) == O_WRONLY)
  {
    fi->flags &= ~O_ACCMODE;
    fi->flags |= O_RDWR;
  }

  if (lo->writeback && (fi-> flags & O_APPEND))
    fi->flags &= ~O_APPEND;

  sprintf(buf, "/proc/self/fd/%i", lo_fd(req, ino));
  fd = open(buf, fi->flags & ~O_NOFOLLOW);
  if (fd == -1)
    return (void) fuse_reply_err(req, errno);

  fi->fh = fd;

  if (lo->cache == CACHE_ALWAYS)
    fi->keep_cache = 1;
  fuse_reply_open(req,fi);
}
//void up_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){}
//void up_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi){}
//void up_write_buf(fuse_req_t req, fuse_ino_t ino, struct fuse_bufvec *bufv, off_t off, struct fuse_file_info *fi){}
//void up_unlink(fuse_req_t req, fuse_ino_t ino, const char *name){}
//void up_rmdir(fuse_req_t req, fuse_ino_t ino, const char *name){}
//void up_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi){}
//void up_symlink(fuse_req_t req, const char *link, fuse_ino_t parent, const char *name){}
//void up_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname, unsigned int flags){}
//void up_mkdir(fuse_req_t req, fuse_ino_t parent, const char*name, mode_t mode){}
//void up_mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, dev_t rdev){}
//void up_link(fuse_req_t req, fuse_ino_t ino,fuse_ino_t newparent, const char *newname){}
//void up_fsync(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi){}
//void up_fsyncdir(fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info *fi){}
//void up_ioctl(fuse_req_t req, fuse_ino_t ino, unsigned int cmd, void *arg, struct fuse_file_info *fi, unsigned flags, const void *in_buf, size_t in_bufsz, size_t outbufsz){}
//void up_fallocate(fuse_req_t req, fuse_ino_t ino, int mode, off_t offset, off_t length, struct fuse_file_info *fi){}
//#ifdef HAVE_COPY_FILE_RANGE
//
//static void up_copy_file_range(fuse_req_t req, fuse_ino_t ino_in, off_t off_in, struct fuse_file_info *fi_in, fuse_ino_t ino_out, off_t off_out, struct fuse_file_info *fi_out, size_t len, int flags)
//{
//
//}
//
//
//#endif


//TODO add more operations
static struct fuse_lowlevel_ops up_oper = {
   /*.statfs = up_statfs,*/
   /*.access = up_access,*/
   /*.getxattr = up_getxattr,*/
   /*.removexattr = up_removexattr,*/
   /*.setxattr = up_setxattr,*/
   /*.listxattr = up_listxattr,*/
   /*.init = up_init,*/
   .lookup = up_lookup,
   /*.forget = up_forget,*/
   /*.forget_multi = up_forget_multi,*/
   .getattr = up_getattr,
   /*.readlink = up_readlink,*/
   .opendir = up_opendir,
   /*.readdir = up_readdir,*/
   /*.readdirplus = up_readdirplus,*/
   /*.releasedir = up_releasedir,*/
   /*.create = up_create,*/
   /*.open = up_open,*/
   /*.release = up_release,*/
   /*.read = up_read,*/
   /*.write_buf = up_write_buf,*/
   /*.unlink = up_unlink,*/
   /*.rmdir = up_rmdir,*/
   /*.setattr = up_setattr,*/
   /*.symlink = up_symlink,*/
   /*.rename = up_rename,*/
   /*.mkdir = up_mkdir,*/
   /*.mknod = up_mknod,*/
   /*.link = up_link,*/
   /*.fsync = up_fsync,*/
   /*.fsyncdir = up_fsyncdir,*/
   /*.ioctl = up_ioctl,*/
   /*.fallocate = up_fallocate,*/
#if HAVE_COPY_FILE_RANGE && HAVE_FUSE_COPY_FILE_RANGE
   /*.copy_file_range = up_copy_file_range,*/
#endif
};

int main(int argc, char* argv[]) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  struct fuse_session* se;
  struct fuse_cmdline_opts opts;

  struct lo_data lo = {.debug = 0, .writeback = 0};

  int ret = -1;

  umask(0);

  pthread_mutex_init(&lo.mutex, NULL);

  if (fuse_parse_cmdline(&args, &opts) != 0)
    return 1;

  if (opts.show_help)
  {
    printf("usage: %s [options] <mountpoint> \n\n", argv[0]);
    fuse_cmdline_help();
    fuse_lowlevel_help();

    ret = 0;

    goto err_out1;
  }
  else if (opts.show_version)
  {
    printf("FUSE library version %s\n", fuse_pkgversion());
    fuse_lowlevel_version();
    ret = 0;

    goto err_out1;
  }

  if(opts.mountpoint == NULL)
  {
    printf("usage: %s [options] <mountpoint>\n", argv[0]);
    printf("       %s --help\n", argv[0]);

    ret = 1;

    goto err_out1;
  }

  if( fuse_opt_parse(&args, &lo, lo_opts, NULL) == -1)
    return -1;

  lo.debug = opts.debug;
  lo.root.refcount = 2;

  if (lo.source)
  {
    struct stat stat;

    if (lstat(lo.source, &stat) == -1)
    {
      error(1, errno, "failed to stat source (\"%s\"): %m\n", lo.source);
      ret = 1;
      goto err_out1;
    }

    if (!S_ISDIR(stat.st_mode))
    {
      printf("source is not a directory\n");
      ret = 1;
      goto err_out1;
    }
  }
  else
  {
    printf("source cannot be empty\n");
    ret = 1;
    goto err_out1;
  }

  if (lo.cache_source)
  {
    struct stat stat;

    if (lstat(lo.cache_source, &stat) == -1)
    {
      printf("failed to stat cache source (\"%s\"): %m\n", lo.source);
      ret = 1;
      goto err_out1;
    }

    if (!S_ISDIR(stat.st_mode))
    {
      printf("cache source is not a directory\n");
      ret = 1;
      goto err_out1;
    }
  }else
  {
    char tmp_source[PATH_MAX];
    sprintf(tmp_source, "/tmp/ufs/%s", lo.source);
    lo.cache_source = strdup(tmp_source);
  }

  lo.timeout = 0.0;
  lo.root.fd = open(lo.source, O_PATH);

  if (lo.root.fd == -1)
  {
    u_log("open(\"%s\", O_PATH): %m\n", lo.source);
    ret = 1;
    goto err_out1;
  }

  se = fuse_session_new(&args, &up_oper, sizeof(up_oper), &lo);
  if (se == NULL)
  {
    error(0, errno, "cannot create FUSE session");
    goto err_out1;
  }

  if (fuse_set_signal_handlers(se) != 0)
  {
    error(0, errno, "cannot set signal handler");
    goto err_out2;
  }

  if (fuse_session_mount(se, opts.mountpoint) != 0)
    goto err_out3;

  fuse_daemonize(opts.foreground);

  struct fuse_loop_config loop_config;
  loop_config.clone_fd = opts.clone_fd;
  loop_config.max_idle_threads = 10;
  ret = fuse_session_loop_mt(se, &loop_config);

  fuse_session_unmount(se);

err_out3:
  fuse_remove_signal_handlers(se);
err_out2:
  fuse_session_destroy(se);
err_out1:
  fuse_opt_free_args(&args);

  return ret ? 1 : 0;
}

