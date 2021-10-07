#include "fuse_utime.h"
#include <sys/stat.h>

int
fuse_utimesat(int _fd,
              const char* _path,
              const struct timespec _times[2],
              int _flags)
{
  return utimensat(_fd, _path, _times, _flags);
}
