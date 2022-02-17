#define _XOPEN_SOURCE 600

#include "quota.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <stdatomic.h>

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

// 当前文件夹资源剩余大小
static atomic_long global_quota = 0;
char global_path[PATH_MAX];

unsigned long
min(unsigned long l1, unsigned long l2)
{
  return l1 < l2 ? l1 : l2;
}

void
quota_set(const char* path, unsigned long size, enum units unit)
{
  strcpy(global_path, path);
  long space_of_path = space(path);
  up_logf("space of %s: %ld\n",path,space_of_path);
  global_quota = (long)(size * unit) - space_of_path;
  up_logf("space initialzed!the remain size is [%ld]\n",global_quota);
}

long double
quota_get(const char* path, enum units unit)
{
  if(limited(path))
    return global_quota / unit;
  return -1;
}

/**
 * Determines if a write can succeed under the quota restrictions.
 */
long
quota_exceeded(const char* path)
{
  if(limited(path))
    return global_quota;
  return 1;
}

long
incr_size(const char* path, long s)
{
  if(limited(path)) {
    long original_quota,result_quota;
    do{
      original_quota = global_quota;
      result_quota = 0;
      if(original_quota > s)
        result_quota = original_quota - s;
    }while(!atomic_compare_exchange_weak(&global_quota, &original_quota, result_quota));

    up_logf("the oringinal quota is %ld, incr size is %ld,the result quota is %ld\n", original_quota , s, global_quota);
    return global_quota;
  }
  return LONG_MAX;
}


void
quota_unset(const char* path)
{
  if(limited(path)){
    global_quota = LONG_MAX;
    strcpy(global_path, "");
  }
}

int limited(const char* path){
  up_logf("global path is %s,target path is %s\n",global_path,path);
  if(strncmp(path, global_path, strlen(global_path)) == 0)
    return 1;
  return 0;
}
