#define _XOPEN_SOURCE 500

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "fuse_ops.h"
#include "quota.h"

char base[PATH_MAX];

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
};

void
usage()
{
  printf("fusequota exceeded <path>\n");
  printf("fusequota unset <path>\n");

  printf("fusequota mount <basedir> <mountpoint> [<size> [-u<B|K|M|G|T>]]\n");

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

  if (strcmp(command, "get") == 0) {
    int c = getopt(argc, argv, "u:");
    enum units unit = (c < 0) ? BYTES : char_to_units(optarg[0]);

    long double size = quota_get(unit);

    printf("%Lf\n", size);
  } else if (strcmp(command, "exceeded") == 0) {
    if (quota_exceeded() == 0)
      printf("NOT ");
    printf("EXCEEDED\n");
  } else if (strcmp(command, "unset") == 0)
    quota_unset(fpath);
  else if (strcmp(command, "mount") == 0) {
    if (argc < 4)
      usage();

    if (realpath(argv[2], base) == NULL)
      error("main_realpath");
    unsigned long size = -1;
    enum units unit = BYTES;
    int ignoreIndex = 2;
    if (argc >= 5 && argv[4][0] != '-') {
      ignoreIndex += 1;
      size = (unsigned long)atol(argv[4]);
      int c = getopt(argc, argv, "u:");
      if (c >= 0) {
        unit = char_to_units(optarg[0]);
        ignoreIndex += 2;
      }
    }
    argv[1] = argv[3];
    int i = 2;
    for (; i < argc; i++)
      argv[i] = argv[i + ignoreIndex];
    argc -= ignoreIndex;

    quota_set(base, size, unit);
    int ret = fuse_main(argc, argv, &fuse_ops, base);

    if (ret < 0)
      error("fuse_main");

    return ret;
  } else
    usage();

  return 0;
}
