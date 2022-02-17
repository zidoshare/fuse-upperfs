#define _XOPEN_SOURCE 500

#ifdef FUSE3
#define FUSE_USE_VERSION 31
#else
#define FUSE_USE_VERSION 26
#endif

#define MAX_FUSE_ARGC 20

#include <fuse.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "fuse_ops.h"
#include "quota.h"
#include "xattr_store.h"
#include "log.h"

char base[PATH_MAX];

#ifdef FUSE3
struct fuse_operations fuse_ops = {
  .getattr = fuse_getattr,
  .readlink = fuse_readlink,
  .mknod = fuse_mknod,
  .mkdir = fuse_mkdir,
  .unlink = fuse_unlink,
  .rmdir = fuse_rmdir,
  .symlink = fuse_symlink,
  .rename = fuse_rename,
  .link = fuse_link,
  .chmod = fuse_chmod,
  .chown = fuse_chown,
  .truncate = fuse_truncate,
  .utimens = fuse_utimens,
  .open = fuse_open,
  .read = fuse_read,
  .write = fuse_write,
  .statfs = fuse_statfs,
  .release = fuse_release,
  .fsync = fuse_fsync,
  .setxattr = fuse_setxattr,
  .getxattr = fuse_getxattr,
  .listxattr = fuse_listxattr,
  .removexattr = fuse_removexattr,
  .opendir = fuse_opendir,
  .readdir = fuse_readdir,
  .releasedir = fuse_releasedir,
  .access = fuse_access,
  .init = fuse_init,
  .destroy = local_fuse_destroy,
};
#else
struct fuse_operations fuse_ops = {
  .getattr = fuse_getattr,
  .readlink = fuse_readlink,
  .mknod = fuse_mknod,
  .mkdir = fuse_mkdir,
  .unlink = fuse_unlink,
  .rmdir = fuse_rmdir,
  .symlink = fuse_symlink,
  .rename = fuse_rename,
  .link = fuse_link,
  .chmod = fuse_chmod,
  .chown = fuse_chown,
  .truncate = fuse_truncate,
  .utime = fuse_utime,
  .open = fuse_open,
  .read = fuse_read,
  .write = fuse_write,
  .statfs = fuse_statfs,
  .release = fuse_release,
  .fsync = fuse_fsync,
  .setxattr = fuse_setxattr,
  .getxattr = fuse_getxattr,
  .listxattr = fuse_listxattr,
  .removexattr = fuse_removexattr,
  .opendir = fuse_opendir,
  .readdir = fuse_readdir,
  .releasedir = fuse_releasedir,
  .access = fuse_access,
  .init = fuse_init,
  .destroy = local_fuse_destroy,
};
#endif

static struct options {
  const char *base;
  const char *quota_path;
  const char *quota_size;
  const char *xattr_store;
  int show_help;
} options;

#define OPTION(t,p) \
{ t, offsetof(struct options, p), 1}
static const struct fuse_opt option_spec[] = {
  OPTION("--base=%s",filename),
}

void
usage()
{
  printf("fuseupperfs mount <basedir> <mountpoint> [-p<quota path> [-s<size>] [-u<B|K|M|G|T>]]"
         "[-x <xattr db_path>] [-d] [-- <fuse options>]\n");

  exit(0);
}

int
main(int argc, char* argv[])
{
  if (argc < 3)
    usage();

  char* command = argv[1];
  char* path = argv[2];

  char fpath[PATH_MAX];
  if (realpath(path, fpath) == NULL)
    error("main_realpath");

  if (strcmp(command, "mount") == 0) {
    if (argc < 4)
      usage();

    if (realpath(argv[2], base) == NULL)
      error("main_realpath");
    unsigned long size = -1;
    enum units unit = BYTES;

    int flag;
    char db_parent_dir[PATH_MAX] = "";
    char quota_path[PATH_MAX] = "/";
    int debug_enable = 0;
    while ((flag = getopt(argc, argv, "p:s:u:x:d")) != -1) {
      // getopt 会导致位置切换
      switch (flag) {
        case 's':
          size = (unsigned long)strtol(optarg, NULL, 0);
          break;
        case 'u':
          unit = char_to_units(optarg[0]);
          break;
        case 'x':
          strcpy(db_parent_dir, optarg);
          break;
        case 'd':
          debug_enable = 1;
          up_log_enable();
          break;
        case 'p':
          strcpy(quota_path,optarg);
          if(strstr(quota_path,"..")) {
              error("cannot use relative path");
          }
          break;
        default:
          break;
      }
    }
    if (db_parent_dir[0] == '\0') {
      strcpy(db_parent_dir, base);
      strcat(db_parent_dir, "/.xattr");
      if (access(db_parent_dir, F_OK) != 0) {
        if (mkdir(db_parent_dir, 0744) != 0) {
          error("failed to create xattr db dir");
          exit(1);
        }
      }
      local_xattr_db_init(db_parent_dir, base);
    } else {
      char real_parent_dir[PATH_MAX];
      if (realpath(db_parent_dir, real_parent_dir) == NULL)
        error("cannot get real path for db parent dir");
      local_xattr_db_init(real_parent_dir, base);
    }
    char tmp_quota_path[PATH_MAX] = "";
    strcpy(tmp_quota_path, base);
    strcat(tmp_quota_path, quota_path);
    char real_quota_path[PATH_MAX] = "";
    if (realpath(tmp_quota_path,real_quota_path) == NULL)
      error("cannot get real path for quota path");
    quota_set(real_quota_path, size, unit);

    int i = 1;
    for (; optind + i + 1 < argc; i++)
      argv[i] = argv[optind + i + 1];
    argc = i;
    if(debug_enable) {
      argv[argc++] = "-d";
    }

    for(int i = 0; i < argc; i++)
      printf("%s ",argv[i]);
    printf("\n");
    int ret = fuse_main(argc, argv, &fuse_ops, base);

    if (ret < 0)
      error("fuse_main");

    return ret;
  } else
    usage();

  return 0;
}