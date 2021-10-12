#define _XOPEN_SOURCE 500

#include "fuse_ops.h"
#include "quota.h"
#include "space.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef FUSE3
#include "fuse_utime.h"
//#define __USE_ATFILE 1
//#include <fcntl.h>
//#else
//#include <utime.h>
#endif

#include "xattr_store.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void
fullpath(const char* path, char* buf)
{
  char* basedir = (char*)fuse_get_context()->private_data;

  strcpy(buf, basedir);
  strcat(buf, path);
}

#ifdef FUSE3
int
fuse_getattr(const char* path,
             struct stat* buf,
             __attribute__((unused)) struct fuse_file_info* fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return lstat(fpath, buf) ? -errno : 0;
}

int
fuse_rename(const char* src,
            const char* dst,
            __attribute__((unused)) unsigned int flags)
{
  char fsrc[PATH_MAX];
  fullpath(src, fsrc);

  char fdst[PATH_MAX];
  fullpath(dst, fdst);

  return rename(fsrc, fdst) ? -errno : 0;
}

int
fuse_chmod(const char* path,
           mode_t mode,
           __attribute__((unused)) struct fuse_file_info* fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return chmod(fpath, mode) ? -errno : 0;
}

int
fuse_chown(const char* path,
           uid_t uid,
           gid_t gid,
           __attribute__((unused)) struct fuse_file_info* fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return chown(fpath, uid, gid) ? -errno : 0;
}

int
fuse_truncate(const char* path,
              off_t off,
              __attribute__((unused)) struct fuse_file_info* fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  pthread_mutex_lock(&mutex);
  long original_size = entry_size(fpath);
  long diff = (long)off - original_size;
  printf("call truncate %s\n",fpath);
  incr_size(diff);

  int result = truncate(fpath, off) ? -errno : 0;
  pthread_mutex_unlock(&mutex);
  return result;
}

int
fuse_utimens(const char* path,
             const struct timespec tv[2],
             __attribute__((unused)) struct fuse_file_info* fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);
  // 这里贼神奇，在我的电脑上直接调用不行，非要封装一层。。。 不知道是为什么
  // 另外 这里 fd 应该传递 fcntl.h 里的 AT_FDCWD。但是因为 `__USE_ATFILE`
  // 宏未定义，导致无法引用这个参数，也不知道怎么定义，目前初步测试通过。
  return fuse_utimesat(-100, fpath, tv, 0) ? -errno : 0;
}

int
fuse_readdir(__attribute__((unused)) const char* path,
             __attribute__((unused)) void* buf,
             __attribute__((unused)) fuse_fill_dir_t fill,
             __attribute__((unused)) off_t off,
             struct fuse_file_info* fi,
             __attribute__((unused)) enum fuse_readdir_flags flags)
{
  struct dirent* de = NULL;

  while ((de = readdir((DIR*)fi->fh)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;

    // TODO 最后一个参数不知道填啥
    if (fill(buf, de->d_name, &st, 0, 0))
      break;
  }

  return 0;
}

void*
fuse_init(__attribute__((unused)) struct fuse_conn_info* conn,
          __attribute__((unused)) struct fuse_config* cfg)
{
  return (fuse_get_context())->private_data;
}

#else
int
fuse_getattr(const char* path, struct stat* buf)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return lstat(fpath, buf) ? -errno : 0;
}

int
fuse_rename(const char* src, const char* dst)
{
  char fsrc[PATH_MAX];
  fullpath(src, fsrc);

  char fdst[PATH_MAX];
  fullpath(dst, fdst);

  return rename(fsrc, fdst) ? -errno : 0;
}

int
fuse_chmod(const char* path, mode_t mode)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return chmod(fpath, mode) ? -errno : 0;
}

int
fuse_chown(const char* path, uid_t uid, gid_t gid)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return chown(fpath, uid, gid) ? -errno : 0;
}

int
fuse_truncate(const char* path, off_t off)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  pthread_mutex_lock(&mutex);
  long original_size = entry_size(fpath);
  long diff = (long)off - original_size;
  printf("call truncate %s\n",fpath);
  incr_size(diff);

  int result = truncate(fpath, off) ? -errno : 0;
  pthread_mutex_unlock(&mutex);
  return result;
}

int
fuse_utime(const char* path, struct utimbuf* buf)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return utime(fpath, buf) ? -errno : 0;
}

int
fuse_readdir(const char* path,
             void* buf,
             fuse_fill_dir_t fill,
             off_t off,
             struct fuse_file_info* fi)
{
  struct dirent* de = NULL;

  while ((de = readdir((DIR*)fi->fh)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;

    if (fill(buf, de->d_name, &st, 0))
      break;
  }

  return 0;
}

void*
fuse_init(struct fuse_conn_info* conn)
{
  return (fuse_get_context())->private_data;
}

#endif

int
fuse_readlink(const char* path, char* target, size_t size)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return readlink(fpath, target, size) < 0 ? -errno : 0;
}

int
fuse_mknod(const char* path, mode_t mode, dev_t dev)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return mknod(fpath, mode, dev) ? -errno : 0;
}

int
fuse_mkdir(const char* path, mode_t mode)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return mkdir(fpath, mode | S_IFDIR) ? -errno : 0;
}

int
fuse_unlink(const char* path)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  pthread_mutex_lock(&mutex);
  printf("call unlink %s\n",fpath);
  long original_size = entry_size(fpath);
  incr_size(-original_size);
  int result = unlink(fpath) ? -errno : 0;
  pthread_mutex_unlock(&mutex);

  return result;
}

int
fuse_rmdir(const char* path)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);
  return rmdir(fpath) ? -errno : 0;
}

int
fuse_symlink(const char* path, const char* link)
{
  char flink[PATH_MAX];
  fullpath(link, flink);

  return symlink(path, flink) ? -errno : 0;
}

int
fuse_link(const char* src, const char* dst)
{
  char fsrc[PATH_MAX];
  fullpath(src, fsrc);

  char fdst[PATH_MAX];
  fullpath(dst, fdst);

  return link(fsrc, fdst) ? -errno : 0;
}

int
fuse_open(const char* path, struct fuse_file_info* fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  int fh = open(fpath, fi->flags);
  if (fh < 0)
    return -errno;

  fi->fh = fh;

  return 0;
}

int
fuse_read(__attribute__((unused)) const char* path,
          char* buf,
          size_t size,
          off_t off,
          struct fuse_file_info* fi)
{
  return pread(fi->fh, buf, size, off) < 0 ? -errno : (int)size;
}

int
fuse_write(__attribute__((unused)) const char* path,
           const char* buf,
           size_t size,
           off_t off,
           struct fuse_file_info* fi)
{
  pthread_mutex_lock(&mutex);
  printf("fuse write call begin\n");
  if (quota_exceeded() != 0) {
    pthread_mutex_unlock(&mutex);
    return -ENOSPC;
  }

  int result = pwrite(fi->fh, buf, size, off) < 0 ? -errno : (int)size;
  if (result > 0)
    incr_size(size);

  pthread_mutex_unlock(&mutex);
  printf("fuse write result no: %d\n", result);
  printf("fuse write call end\n");
  return result;
}

int
fuse_statfs(const char* path, struct statvfs* buf)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return statvfs(fpath, buf) ? -errno : 0;
}

int
fuse_release(__attribute__((unused)) const char* path,
             struct fuse_file_info* fi)
{
  return close(fi->fh) ? -errno : 0;
}

int
fuse_fsync(__attribute__((unused)) const char* path,
           int datasync,
           struct fuse_file_info* fi)
{
  if (datasync)
    return fdatasync(fi->fh) ? -errno : 0;
  else
    return fsync(fi->fh) ? -errno : 0;
}

int
fuse_setxattr(const char* path,
              const char* name,
              const char* value,
              size_t size,
              int flags)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return local_set_xattr(fpath, name, value, size, flags) ? -errno : 0;
}

int
fuse_getxattr(const char* path, const char* name, char* value, size_t size)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  ssize_t s = local_get_xattr(fpath, name, value, size);
  return s < 0 ? -errno : (int)s;
}

int
fuse_listxattr(const char* path, char* list, size_t size)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  size_t len = local_list_xattr(fpath, list, size);
  int result = len < 0 ? -errno : (int)len;
  printf("result is %d\n", result);
  if (size != 0) {
    printf("current list is (size %ld,len %ld):", size, len);
    for (int i = 0; i < len; i++) {
      if (*(list + i) == '\0')
        printf("\\0");
      else
        printf("%c", *(list + i));
    }
    printf("\n");
  }
  return result;
}

int
fuse_removexattr(const char* path, const char* name)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return local_remove_xattr(fpath, name) ? -errno : 0;
}

int
fuse_opendir(const char* path, struct fuse_file_info* fi)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  DIR* dir = opendir(fpath);
  if (dir == NULL)
    return -errno;

  fi->fh = (uint64_t)dir;

  return 0;
}

int
fuse_releasedir(__attribute__((unused)) const char* path,
                struct fuse_file_info* fi)
{
  return closedir((DIR*)fi->fh) ? -errno : 0;
}

int
fuse_access(const char* path, int mode)
{
  char fpath[PATH_MAX];
  fullpath(path, fpath);

  return access(fpath, mode) ? -errno : 0;
}

void
local_fuse_destroy(void* private_data)
{
  local_xattr_db_release();
}
