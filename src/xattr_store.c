#include <errno.h>
#include <leveldb/c.h>
#include <limits.h>
#include <malloc.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

static char GLOBAL_DB_PATH[PATH_MAX];
static char MOUNT_BASE_DIR[PATH_MAX];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static leveldb_t* DB = NULL;

bool
file_exists(char* filename)
{
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

int
mkdir_all(char* folder_path)
{
  if (!access(folder_path, F_OK)) {
    return 1;
  }

  char path[PATH_MAX];
  char* path_buf;
  char temp_path[PATH_MAX];
  char* temp;
  int temp_len;

  memset(path, 0, sizeof(path));
  memset(temp_path, 0, sizeof(temp_path));
  strcat(path, folder_path);
  path_buf = path;

  while ((temp = strsep(&path_buf, "/")) != NULL) {
    temp_len = strlen(temp);
    if (0 == temp_len) {
      continue;
    }
    strcat(temp_path, "/");
    strcat(temp_path, temp);
    printf("temp_path = %s\n", temp_path);
    if (-1 == access(temp_path, F_OK)) {
      if (-1 == mkdir(temp_path, 0744)) {
        return 2;
      }
    }
  }
  return 1;
}

void
local_xattr_db_init(const char* db_parent_dir, const char* mount_base_dir)
{
  strcpy(GLOBAL_DB_PATH, db_parent_dir);
  strcpy(MOUNT_BASE_DIR, mount_base_dir);
}

void
local_xattr_db_release()
{
  pthread_mutex_lock(&mutex);
  if (DB != NULL) {
    leveldb_close(DB);
  }
  pthread_mutex_unlock(&mutex);
}

leveldb_t*
create_db()
{
  if (!file_exists(GLOBAL_DB_PATH)) {
    mkdir_all(GLOBAL_DB_PATH);
  }
  printf("GLOBAL_DB_PATH is %s",GLOBAL_DB_PATH);
  leveldb_t* db;
  leveldb_options_t* options;
  char* err = NULL;

  options = leveldb_options_create();
  leveldb_options_set_create_if_missing(options, 1);
  db = leveldb_open(options, GLOBAL_DB_PATH, &err);

  if (err != NULL) {
    fprintf(stderr, "Get DB fail: %s\n", err);
    return NULL;
  }
  leveldb_free(err);
  err = NULL;
  return db;
}

leveldb_t*
get_db()
{
  if (DB != NULL) {
    return DB;
  }
  pthread_mutex_lock(&mutex);
  if (DB != NULL) {
    return DB;
  }
  DB = create_db();
  pthread_mutex_unlock(&mutex);
  return DB;
}

char*
get_db_key(const char* path, const char* name, size_t* result_len)
{
  size_t prefix_len = strlen(path);
  *result_len = (prefix_len + strlen(name));
  char* buf = (char*)malloc(sizeof(char) * ((*result_len) + 2));
  strcpy(buf, path);
  strcat(buf, name);
  return buf;
}

char*
unwrap_db_key(const char* path,
              size_t path_len,
              char* key,
              size_t key_len,
              size_t* result_len)
{
  *result_len = key_len - path_len;
  return key + path_len;
}

int
local_set_xattr(const char* path,
                const char* name,
                const char* value,
                size_t size,
                int flags)
{
  leveldb_t* db = get_db();
  char* original_value = NULL;
  char* err = NULL;

  size_t name_len;
  char* wrappered_name = get_db_key(path, name, &name_len);

  if (db != NULL) {
    leveldb_readoptions_t* roptions = leveldb_readoptions_create();

    size_t vallen = 0;
    original_value =
      leveldb_get(db, roptions, wrappered_name, name_len, &vallen, &err);
    if (err != NULL) {
      fprintf(stderr, "Get attr fail: %s\n", err);
      errno = ENOTSUP;
      free(wrappered_name);
      return -1;
    }
    leveldb_free(err);
    err = NULL;
  }

  if (flags == 0 || (original_value == NULL && flags == XATTR_CREATE) ||
      (original_value != NULL && flags == XATTR_REPLACE)) {
    leveldb_writeoptions_t* woptions = leveldb_writeoptions_create();
    leveldb_put(db, woptions, wrappered_name, name_len, value, size, &err);
    leveldb_free(original_value);
    free(wrappered_name);
    return 0;
  }

  if (original_value == NULL && flags == XATTR_REPLACE)
    errno = ENODATA;
  else if (original_value != NULL && flags == XATTR_CREATE)
    errno = EEXIST;
  free(wrappered_name);
  return -1;
}
int
local_get_xattr(const char* path, const char* name, char* value, size_t size)
{
  leveldb_t* db = get_db();
  if (db == NULL) {
    errno = ENODATA;
    return -1;
  }
  char* err = NULL;
  size_t name_len;
  char* wrappered_name = get_db_key(path, name, &name_len);
  leveldb_readoptions_t* roptions = leveldb_readoptions_create();

  size_t vallen;
  char* db_store_value =
    leveldb_get(db, roptions, wrappered_name, name_len, &vallen, &err);
  if (err != NULL) {
    fprintf(stderr, "Get attr fail: %s\n", err);
    errno = ENOTSUP;
    return -1;
  }
  leveldb_free(err);

  if (db_store_value == NULL) {
    errno = ENODATA;
    return -1;
  }
  printf("value = %s,size = %ld\n", value, size);

  if (size != 0) {
    if (vallen <= size) {
      strcpy(value, db_store_value);
      leveldb_free(db_store_value);
    } else {
      errno = ERANGE;
      return -1;
    }
  }

  return (int)vallen;
}
int
local_list_xattr(const char* path, char* list, size_t size)
{
  leveldb_t* db = get_db();
  if (db == NULL) {
    return 0;
  }
  leveldb_readoptions_t* roptions = leveldb_readoptions_create();

  leveldb_iterator_t* iter = leveldb_create_iterator(db, roptions);
  if (size == 0) {
    size_t len = 0;
    for (leveldb_iter_seek_to_first(iter); leveldb_iter_valid(iter);
         leveldb_iter_next(iter)) {
      size_t vallen;
      const char* key = leveldb_iter_key(iter, &vallen);
      size_t key_len;
      unwrap_db_key(path, strlen(path), key, vallen, &key_len);
      len += key_len + 1;
    }
    leveldb_iter_destroy(iter);
    printf("len = %d\n", (int)len);
    return (int)len;
  }
  size_t len = 0;
  for (leveldb_iter_seek_to_first(iter); leveldb_iter_valid(iter);
       leveldb_iter_next(iter)) {
    if (size < len) {
      errno = ERANGE;
      return -1;
    }
    size_t vallen;
    const char* key = leveldb_iter_key(iter, &vallen);
    size_t key_len;
    char* result_key = unwrap_db_key(path, strlen(path), key, vallen, &key_len);
    memcpy(list + len, result_key, key_len);
    len += key_len + 1;
    *(list + len - 1) = '\0';
  }
  leveldb_iter_destroy(iter);

  return (int)len;
}
int
local_remove_xattr(const char* path, const char* name)
{
  leveldb_t* db = get_db();
  if (db == NULL) {
    errno = ENODATA;
    return -1;
  }

  leveldb_readoptions_t* roptions = leveldb_readoptions_create();
  char* err = NULL;

  size_t name_len;
  char* wrappered_name = get_db_key(path, name, &name_len);
  size_t vallen = 0;
  char* db_store_value =
    leveldb_get(db, roptions, wrappered_name, name_len, &vallen, &err);
  if (err != NULL) {
    fprintf(stderr, "Get attr fail: %s\n", err);
    errno = ENOTSUP;
    free(wrappered_name);
    return -1;
  }
  leveldb_free(err);
  err = NULL;
  if (db_store_value == NULL) {
    free(wrappered_name);
    errno = ENODATA;
    return -1;
  }

  leveldb_writeoptions_t* woptions = leveldb_writeoptions_create();

  leveldb_delete(db, woptions, wrappered_name, name_len, &err);
  if (err != NULL) {
    fprintf(stderr, "Delete attr fail: %s\n", err);
    errno = ENOTSUP;
    free(wrappered_name);
    return -1;
  }
  leveldb_free(err);
  err = NULL;
  free(wrappered_name);
  return 0;
}
