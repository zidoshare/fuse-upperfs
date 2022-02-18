#define FUSE_USE_VERSION 35

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fuse_lowlevel.h>
#include <stddef.h>
#include <pthread.h>
#include <limits.h>
#include <sys/resource.h>
#include <err.h>
#include <errno.h>
#include <error.h>
#include <signal.h>
#include <stdlib.h>

struct hash_map;

struct lo_inode
{
    struct lo_inode *parent;
    struct hash_map *children;
    size_t name_hash;
    int fd;
    ino_t ino;
    dev_t dev;
    int lookups; // ref count
};

enum
{
    CACHE_NEVER,
    CACHE_NORMAL,
    CACHE_ALWAYS,
};

struct lo_data
{
    pthread_mutex_t mutex;
    int debug;
    int writeback;
    int flock;
    int xattr;
    const char *source;
    double timeout;
    int cache;
    int timeout_set;
    struct hash_map *inodes;
};

static const struct fuse_opt lo_opts[] = {
    {"writeback",
     offsetof(struct lo_data, writeback), 1},
    {"no_writeback",
     offsetof(struct lo_data, writeback), 0},
    {"source=%s",
     offsetof(struct lo_data, source), 0},
    {"flock",
     offsetof(struct lo_data, flock), 1},
    {"no_flock",
     offsetof(struct lo_data, flock), 0},
    {"xattr",
     offsetof(struct lo_data, xattr), 1},
    {"no_xattr",
     offsetof(struct lo_data, xattr), 0},
    {"timeout=%lf",
     offsetof(struct lo_data, timeout), 0},
    {"timeout=",
     offsetof(struct lo_data, timeout_set), 1},
    {"cache=never",
     offsetof(struct lo_data, cache), CACHE_NEVER},
    {"cache=auto",
     offsetof(struct lo_data, cache), CACHE_NORMAL},
    {"cache=always",
     offsetof(struct lo_data, cache), CACHE_ALWAYS},

    FUSE_OPT_END};

static size_t node_inode_hasher (const void *p, size_t s)
{
  struct lo_inode *n = (struct lo_inode *)p;

  return (n->ino ^ n->dev) % s;
}

void maximize_fd_limit()
{
    struct rlimit lim;
    int res = getrlimit(RLIMIT_NOFILE, &lim);
    if (res < 0)
        error( EXIT_FAILURE, errno, "read nofile rlimit failed");
    lim.rlim_cur = lim.rlim_max;
    res = setrlimit(RLIMIT_NOFILE, &lim);
    if (res != 0)
        error( EXIT_FAILURE, errno, "write nofile rlimit failed");
}

static struct fuse_lowlevel_ops up_oper =
{

};

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;

    struct lo_data lo = {
        .debug = 0,
        .writeback = 0};

    int ret = -1;

    umask(0);

    pthread_mutex_init(&lo.mutex, NULL);

    se = fuse_session_new(&args,&up_oper,sizeof (up_oper), &lo);
    if (se == NULL)
    {
      error(0, errno, "cannot create FUSE session");
      goto err_out1;
    }

    if( fuse_set_signal_handlers(se) != 0)
    {
      error(0, errno, "cannot set signal handler");
      goto err_out2;
    }

    if(fuse_session_mount(se, argv[2]) != 0)
    {
      error(0, errno, "mount failed");
      exit(EXIT_FAILURE);
    }

    fuse_daemonize(1);

    struct fuse_loop_config loop_config;
    loop_config.clone_fd = 0;
    loop_config.max_idle_threads = 10;
    ret = fuse_session_loop_mt(se, &loop_config);

    fuse_session_unmount(se);

err_out3:
    fuse_remove_signal_handlers(se);
err_out2:
    fuse_session_destroy(se);
err_out1:
    fuse_opt_free_args(&args);

    return ret ? 1: 0;
}

