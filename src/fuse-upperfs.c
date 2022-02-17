#define FUSE_USE_VERSION 35

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include "hash.h"

#if defined(__GNUC__) && (__GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 6) && !defined __cplusplus
_Static_assert(sizeof(fuse_ino_t) >= sizeof(uintptr_t),
               "fuse_ino_t too small to hold uintptr_t values!");
#else
struct _uintptr_to_must_hold_fuse_ino_t_dummy_struct
{
    unsigned _uintptr_to_must_hold_fuse_ino_t : ((sizeof(fuse_ino_t) >= sizeof(uintptr_t)) ? 1 : -1);
};
#endif

struct lo_inode
{
    struct lo_inode *parent;
    struct hash_map children;
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
    struct hash_map inodes;
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
}
