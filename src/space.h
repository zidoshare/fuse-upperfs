#ifndef SPACE_H
#define SPACE_H

#include <stdbool.h>
#define BYTES_IN_KILOBYTE 1024.0L
#define BYTES_IN_MEGABYTE 1048576.0L
#define BYTES_IN_GIGABYTE 1073741824.0L
#define BYTES_IN_TERABYTE 1099511627776.0L

bool
initialized();
long
incr_size(long s);
long
entry_size(const char* path);
unsigned long
space(const char* path);

#endif
