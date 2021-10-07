#ifndef FUSE_UTIME_H
#define FUSE_UTIME_H
#include <sys/types.h>

int
fuse_utimesat(int _fd,
              const char* _path,
              const struct timespec _times[2],
              int _flags);

#endif
