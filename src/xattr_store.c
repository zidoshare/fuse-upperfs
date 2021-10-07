#include <errno.h>
#include <leveldb/c.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>

static char DB_PARENT_DIR[PATH_MAX];
static char MOUNT_BASE_DIR[PATH_MAX];

bool
file_exists(char* filename)
{
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

void
local_xattr_db_init(const char* db_parent_dir, const char* mount_base_dir)
{
  strcpy(DB_PARENT_DIR, db_parent_dir);
  strcpy(MOUNT_BASE_DIR, mount_base_dir);
}

void
get_db_path(const char* path, char* dest_path)
{
  strcpy(dest_path, DB_PARENT_DIR);

  strcat(dest_path, path + strlen(MOUNT_BASE_DIR));

  // 去除结尾斜杠
  size_t i = strlen(dest_path) - 1;
  while (dest_path[i] == '/') {
    dest_path[i] = '\0';
  }
}

leveldb_t*
get_db_by_path(const char* path)
{
  char dest_path[PATH_MAX];
  get_db_path(path, dest_path);

  if (file_exists(dest_path)) {
    leveldb_t* db;
    leveldb_options_t* options;
    char* err = NULL;

    options = leveldb_options_create();
    leveldb_options_set_create_if_missing(options, 1);
    db = leveldb_open(options, dest_path, &err);

    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      return NULL;
    }
    leveldb_free(err);
    err = NULL;
    return db;
  }
  return NULL;
}

leveldb_t*
create_db_by_path(const char* path)
{
  char dest_path[PATH_MAX];
  get_db_path(path, dest_path);

  char* err = NULL;
  leveldb_options_t* options = leveldb_options_create();
  leveldb_options_set_create_if_missing(options, 1);
  leveldb_t* db = leveldb_open(options, dest_path, &err);
  if (err != NULL) {
    fprintf(stderr, "Open fail.\n");
    return NULL;
  }
  return db;
}

int
local_set_xattr(const char* path,
                const char* name,
                const char* value,
                size_t size,
                int flags)
{
  leveldb_t* db = get_db_by_path(path);
  char* original_value = NULL;
  char* err = NULL;

  if (db != NULL) {
    leveldb_readoptions_t* roptions = leveldb_readoptions_create();

    size_t vallen = 0;
    original_value =
      leveldb_get(db, roptions, name, strlen(name), &vallen, &err);
    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      // TODO error handle
      return -1;
    }
    leveldb_free(err);
    err = NULL;
    leveldb_free(roptions);
  } else {
    db = create_db_by_path(path);
  }

  if (flags == 0 || (original_value == NULL && flags == XATTR_CREATE) ||
      (original_value != NULL && flags == XATTR_REPLACE)) {
    leveldb_writeoptions_t* woptions = leveldb_writeoptions_create();
    leveldb_put(db, woptions, name, strlen(name), value, size, &err);
    leveldb_free(original_value);
    leveldb_free(woptions);
    leveldb_free(db);
    return 0;
  }

  if (original_value == NULL && flags == XATTR_REPLACE)
    errno = ENODATA;
  else if (original_value != NULL && flags == XATTR_CREATE) {
    errno = EEXIST;
    leveldb_free(original_value);
  }
  leveldb_free(db);
  return -1;
}
int
local_get_xattr(const char* path, const char* name, char* value, size_t size)
{
  leveldb_t* db = get_db_by_path(path);
  if (db == NULL) {
    errno = ENODATA;
    return -1;
  }
  char* err = NULL;
  leveldb_readoptions_t* roptions = leveldb_readoptions_create();

  size_t vallen = 0;
  char* db_store_value =
    leveldb_get(db, roptions, name, strlen(name), &vallen, &err);
  if (err != NULL) {
    fprintf(stderr, "Open fail.\n");
    // TODO error handle
    return -1;
  }
  leveldb_free(err);

  if (db_store_value == NULL) {
    errno = ENODATA;
    return -1;
  }

  if (vallen <= size) {
    strcpy(value, db_store_value);
  } else {
    errno = ERANGE;
    return -1;
  }

  return vallen;
}
int
local_list_xattr(const char* path, char* list, size_t size)
{
  leveldb_t* db = get_db_by_path(path);
  leveldb_readoptions_t* roptions = leveldb_readoptions_create();

  leveldb_iterator_t* iter = leveldb_create_iterator(db, roptions);

  char* head = list;
  size_t len = 0;
  for (leveldb_iter_seek_to_first(iter); leveldb_iter_valid(iter);
       leveldb_iter_next(iter)) {
    if (size < len) {
      errno = ERANGE;
      return -1;
    }
    size_t write_size = size - 0;
    leveldb_iter_seek(iter, head, write_size);
    len += strlen(head);
    head += len;
  }

  return 0;
}
int
local_remove_xattr(const char* path, const char* name)
{
  leveldb_t* db = get_db_by_path(path);

  leveldb_readoptions_t* roptions = leveldb_readoptions_create();
  char* err = NULL;

  size_t vallen = 0;
  char* db_store_value =
    leveldb_get(db, roptions, name, strlen(name), &vallen, &err);
  if (err != NULL) {
    fprintf(stderr, "Open fail.\n");
    // TODO error handle
    return -1;
  }
  leveldb_free(err);
  err = NULL;
  if (db_store_value == NULL) {
    errno = ENODATA;
    return -1;
  }

  leveldb_writeoptions_t* woptions = leveldb_writeoptions_create();

  leveldb_delete(db, woptions, name, strlen(name), &err);
  if (err != NULL) {
    fprintf(stderr, "Open fail.\n");
    // TODO error handle
    return -1;
  }
  leveldb_free(err);
  err = NULL;

  return 0;
}
