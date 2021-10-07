#ifndef XATTR_STORE_H
#define XATTR_STORE_H

#include <string.h>

// 初始化
void
local_xattr_db_init(const char* db_parent_path,const char* mount_base_dir);

int
local_set_xattr(const char* path,
                const char* name,
                const char* value,
                size_t size,
                int flags);
int
local_get_xattr(const char* path, const char* name, char* value, size_t size);
int
local_list_xattr(const char* path, char* list, size_t size);
int
local_remove_xattr(const char* path, const char* name);

#endif
