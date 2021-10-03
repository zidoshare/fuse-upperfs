#include <leveldb/c.h>
#include <limits.h>
#include <stdio.h>

void init(){
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
    char *err = NULL;
    char *read;
    size_t read_len;

    options = leveldb_options_create();
    leveldb_options_set_create_if_missing(options, 1);
    db = leveldb_open(options, "testdb", &err);

    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      return(1);
    }
    leveldb_free(err); err = NULL;
}

int local_set_xattr(const char *path, const char *name, const char *value, size_t size, int flags)
{

}
int local_get_xattr(const char *path, const char *name, char *value, size_t size);
int local_list_xattr(const char *path, char *list, size_t size);
int local_remove_xattr(const char *path, const char *name);