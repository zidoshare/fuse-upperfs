#ifndef FUSE_H
#define FUSE_H

#define FUSE_USE_VERSION 31

#define _XOPEN_SOURCE 500

#include <fuse.h>
#include <limits.h>

int
fuse_getattr(const char* path, struct stat* buf, struct fuse_file_info* fi);

int
fuse_readlink(const char* path, char* target, size_t size);

int
fuse_mknod(const char* path, mode_t mode, dev_t dev);

int
fuse_mkdir(const char* path, mode_t mode);

int
fuse_unlink(const char* path);

int
fuse_rmdir(const char* path);

int
fuse_symlink(const char* path, const char* link);

int
fuse_rename(const char* src, const char* dst, unsigned int flags);

int
fuse_link(const char* src, const char* dst);

int
fuse_chmod(const char* path, mode_t mode, struct fuse_file_info* fi);

int
fuse_chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi);

int
fuse_truncate(const char* path, off_t off, struct fuse_file_info* fi);

int
fuse_open(const char* path, struct fuse_file_info* fi);

int
fuse_read(const char* path,
          char* buf,
          size_t size,
          off_t off,
          struct fuse_file_info* fi);

int
fuse_write(const char* path,
           const char* buf,
           size_t size,
           off_t off,
           struct fuse_file_info* fi);

int
fuse_statfs(const char* path, struct statvfs* buf);

// int (*flush) (const char *, struct fuse_file_info *);

int
fuse_release(const char* path, struct fuse_file_info* fi);

int
fuse_fsync(const char* path, int datasync, struct fuse_file_info* fi);

int
fuse_setxattr(const char* path,
              const char* name,
              const char* value,
              size_t size,
              int flags);

int
fuse_getxattr(const char* path, const char* name, char* value, size_t size);

int
fuse_listxattr(const char* path, char* list, size_t size);

int
fuse_removexattr(const char* path, const char* name);

int
fuse_opendir(const char* path, struct fuse_file_info* fi);

int
fuse_readdir(const char* path,
             void* buf,
             fuse_fill_dir_t fill,
             off_t off,
             struct fuse_file_info* fi,
             enum fuse_readdir_flags flags);

int
fuse_releasedir(const char* path, struct fuse_file_info* fi);

// int (*fsyncdir) (const char *, int, struct fuse_file_info *);

void*
fuse_init(struct fuse_conn_info* conn, struct fuse_config* cfg);

// void (*destroy) (void *private_data);

int
fuse_access(const char* path, int mode);

int
fuse_utimens(const char* path,
             const struct timespec tv[2],
             struct fuse_file_info* fi);

#endif
