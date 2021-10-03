/**
 * Project: fusequota
 * Author: August Sodora III <augsod@gmail.com>
 * File: quota.c
 * License: GPLv3
 *
 * fusequota is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fusequota is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fusequota. If not, see <http://www.gnu.org/licenses/>.
 */
#define _XOPEN_SOURCE 600

#include "quota.h"
#include "space.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <sys/types.h>
#include <sys/xattr.h>

enum units
char_to_units(const char c)
{
  switch (c)
  {
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

void quota_set(const char *path, unsigned long size, enum units unit)
{
  global_quota = (long)(size * unit);
  strcpy(global_path, path);
  if (!initialized(path))
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
int quota_exceeded()
{
  unsigned long quota = quota_get(BYTES);
  printf("current quota: %ld\n", quota);

  if (quota == 0)
    return 0;

  unsigned long size = space(global_path);
  printf("current size: %ld\n", size);
  if (size >= quota)
    return -1;

  return 0;
}

void quota_unset(const char *path)
{
  global_quota = 0;
  strcpy(global_path, "");
}
