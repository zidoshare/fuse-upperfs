AM_CPPFLAGS = $(fuse_CFLAGS)

bin_PROGRAMS = fuseupperfs
fuseupperfs_SOURCES = fuseupperfs.c fuse_ops.c fuse_ops.h quota.c quota.h space.c space.h error.c error.h

fuseupperfs_LDADD = $(fuse_LIBS)
