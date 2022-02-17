# 手动编译

```shell
gcc -Wall -std=c11 `pkg-config fuse3 --cflags --libs`  -o fuseupperfs error.c fuse_ops.c fuseupperfs.c quota.c space.c
```