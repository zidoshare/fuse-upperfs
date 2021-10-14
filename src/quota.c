#define _XOPEN_SOURCE 600

#include "quota.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "error.h"
#include "space.h"
#include "log.h"

enum units
char_to_units(const char c)
{
  switch (c) {
    case 'B':
      return BYTES;
    case 'K':
      return KILOBYTES;
    case 'M':
      return MEGABYTES;
    case 'G':
      return GIGABYTES;
    case 'T':
      return TERABYTES;
    default:
      return BYTES;
  }
}

long global_quota = 0;
char global_path[PATH_MAX];

unsigned long
min(unsigned long l1, unsigned long l2)
{
  return l1 < l2 ? l1 : l2;
}

void
quota_set(const char* path, unsigned long size, enum units unit)
{
  global_quota = (long)(size * unit);
  strcpy(global_path, path);
  if (!initialized())
    space(path);
}

long double
quota_get(enum units unit)
{
  return global_quota / unit;
}

/**
 * Determines if a write can succeed under the quota restrictions.
 */
int
quota_exceeded()
{
  unsigned long quota = quota_get(BYTES);
  up_logf("current quota: %ld\n", quota);

  if (quota == 0)
    return 0;

  unsigned long size = space(global_path);
  up_logf("current size: %ld\n", size);
  if (size >= quota)
    return -1;

  return 0;
}

void
quota_unset(__attribute__((unused)) const char* path)
{
  global_quota = 0;
  strcpy(global_path, "");
}
