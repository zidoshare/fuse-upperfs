#define _XOPEN_SOURCE 500

#include "fuse_ops.h"
#include "space.h"
#include "quota.h"

#include <stdio.h>

#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <sys/xattr.h>
#include <sys/types.h>
#include <pthread.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/**
 * Appends the path of the root filesystem to the given path, returning
 * the result in buf.
 */
void fullpath(const char *path, char *buf)
{
  char *basedir = (char *)fuse_get_context()->private_data;

  strcpy(buf, basedir);
  strcat(buf, path);
}

/* The following functions describe FUSE operations. Each operation appends
   the path of the root filesystem to the given path in order to give the
   mirrored path. All quota checking takes place in fuse_write. */

int fuse_getattr(const char *path, struct stat *buf)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return lstat(fpath, buf) ? -errno : 0;
}

int fuse_readlink(const char *path, char *target, size_t size)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return readlink(fpath, target, size) < 0 ? -errno : 0;
}

int fuse_mknod(const char *path, mode_t mode, dev_t dev)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return mknod(fpath, mode, dev) ? -errno : 0;
}

int fuse_mkdir(const char *path, mode_t mode)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return mkdir(fpath, mode | S_IFDIR) ? -errno : 0;
}

int fuse_unlink(const char *path)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  pthread_mutex_lock(&mutex);
  long original_size = space(fpath);
  incr_size(-original_size);
  int result = unlink(fpath) ? -errno : 0;
  pthread_mutex_unlock(&mutex);

  return result;
}

int fuse_rmdir(const char *path)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);
  return rmdir(fpath) ? -errno : 0;
}

int fuse_symlink(const char *path, const char *link)
{
  char flink[PATH_MAX];
  fullpath(link, flink);

  return symlink(path, flink) ? -errno : 0;
}

int fuse_rename(const char *src, const char *dst)
{
  char fsrc[PATH_MAX];
  fullpath(src, fsrc);

  char fdst[PATH_MAX];
  fullpath(dst, fdst);

  return rename(fsrc, fdst) ? -errno : 0;
}

int fuse_link(const char *src, const char *dst)
{
  char fsrc[PATH_MAX];
  fullpath(src, fsrc);

  char fdst[PATH_MAX];
  fullpath(dst, fdst);

  return link(fsrc, fdst) ? -errno : 0;
}

int fuse_chmod(const char *path, mode_t mode)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return chmod(fpath, mode) ? -errno : 0;
}

int fuse_chown(const char *path, uid_t uid, gid_t gid)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return chown(fpath, uid, gid) ? -errno : 0;
}

int fuse_truncate(const char *path, off_t off)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  pthread_mutex_lock(&mutex);
  long original_size = entry_size(fpath);
  long diff = (long)off - original_size;
  incr_size(diff);

  int result = truncate(fpath, off) ? -errno : 0;
  pthread_mutex_unlock(&mutex);
  return result;
}

int fuse_utime(const char *path, struct utimbuf *buf)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return utime(fpath, buf) ? -errno : 0;
}

int fuse_open(const char *path, struct fuse_file_info *fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  int fh = open(fpath, fi->flags);
  if (fh < 0)
    return -errno;

  fi->fh = fh;

  return 0;
}

int fuse_read(const char *path, char *buf, size_t size, off_t off,
              struct fuse_file_info *fi)
{
  return pread(fi->fh, buf, size, off) < 0 ? -errno : size;
}

int fuse_write(const char *path, const char *buf, size_t size, off_t off,
               struct fuse_file_info *fi)
{
  pthread_mutex_lock(&mutex);
  printf("fuse write call begin\n");
  if (quota_exceeded() != 0)
  {
    pthread_mutex_unlock(&mutex);
    return -ENOSPC;
  }

  int result = pwrite(fi->fh, buf, size, off) < 0 ? -errno : size;
  if (result > 0)
    incr_size(size);

  pthread_mutex_unlock(&mutex);
  printf("fuse write result no: %d\n", result);
  printf("fuse write call end\n");
  return result;
}

int fuse_statfs(const char *path, struct statvfs *buf)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return statvfs(fpath, buf) ? -errno : 0;
}

int fuse_release(const char *path, struct fuse_file_info *fi)
{
  return close(fi->fh) ? -errno : 0;
}

int fuse_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
  if (datasync)
    return fdatasync(fi->fh) ? -errno : 0;
  else
    return fsync(fi->fh) ? -errno : 0;
}

int fuse_setxattr(const char *path, const char *name, const char *value,
                  size_t size, int flags)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return lsetxattr(fpath, name, value, size, flags) ? -errno : 0;
}

int fuse_getxattr(const char *path, const char *name, char *value, size_t size)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  ssize_t s = lgetxattr(fpath, name, value, size);
  return s < 0 ? -errno : s;
}

int fuse_listxattr(const char *path, char *list, size_t size)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return llistxattr(fpath, list, size) < 0 ? -errno : 0;
}

int fuse_removexattr(const char *path, const char *name)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return lremovexattr(fpath, name) ? -errno : 0;
}

int fuse_opendir(const char *path, struct fuse_file_info *fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  DIR *dir = opendir(fpath);
  if (dir == NULL)
    return -errno;

  fi->fh = (uint64_t)dir;

  return 0;
}

int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t fill, off_t off,
                 struct fuse_file_info *fi)
{
  struct dirent *de = NULL;

  while ((de = readdir((DIR *)fi->fh)) != NULL)
  {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;

    if (fill(buf, de->d_name, &st, 0))
      break;
  }

  return 0;
}

int fuse_releasedir(const char *path, struct fuse_file_info *fi)
{
  return closedir((DIR *)fi->fh) ? -errno : 0;
}

int fuse_access(const char *path, int mode)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return access(fpath, mode) ? -errno : 0;
}

void *
fuse_init(struct fuse_conn_info *conn)
{
  return (fuse_get_context())->private_data;
}
