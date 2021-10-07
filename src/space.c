#define _XOPEN_SOURCE 500

#include "space.h"
#include "error.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <unistd.h>

static char global_path[4096] = "";
static unsigned long global_size = -1;

unsigned long
directory_size(const char* path)
{
  struct statfs sfs;
  if (statfs(path, &sfs) != 0) {
    if (errno == EACCES)
      return 0;

    error("directory_size.statfs");
  } else if (sfs.f_type == 0x9fa0)
    return 0;

  if (chdir(path) != 0) {
    if (errno == EACCES)
      return 0;

    error("From directory_size.chdir");
  }

  DIR* dir = opendir(".");

  if (dir == NULL) {
    if (errno == EACCES)
      return 0;

    error("directory_size.opendir");
  }

  struct dirent* ent = NULL;
  unsigned long size = 0;

  while ((ent = readdir(dir)) != NULL) {
    if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0))
      continue;

    size += entry_size(ent->d_name);
  }

  if (chdir("..") != 0)
    error("directory_size.chdir");

  if (closedir(dir) != 0)
    error("directory_size.closedir");

  return size;
}

long
entry_size(const char* path)
{
  struct stat buf;
  if (lstat(path, &buf) != 0)
    return -1;

  if (S_ISDIR(buf.st_mode))
    return buf.st_size + directory_size(path);
  else
    return buf.st_size;
}

long
incr_size(long s)
{
  if (global_size == (unsigned long)-1)
    space(global_path);
  if (global_size == (unsigned long)-1)
    return -1;
  if ((long)global_size + s < 0)
    global_size = 0;
  else
    global_size += s;
  return global_size;
}

unsigned long
space(const char* path)
{
  int len = strlen(global_path);
  if (len)
    return global_size;

  char fpath[PATH_MAX];
  if (realpath(path, fpath) == NULL)
    error("main.realpath");
  strcpy(global_path, fpath);
  global_size = entry_size(fpath);

  return global_size;
}

int
initialized(__attribute__((unused)) const char* path)
{
  return strlen(global_path);
}
