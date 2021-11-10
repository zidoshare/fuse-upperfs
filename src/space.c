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

unsigned long
space(const char* path)
{
  char fpath[PATH_MAX];
  if (realpath(path, fpath) == NULL)
    error("main.realpath");
  return entry_size(fpath);
}
