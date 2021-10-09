cmake_minimum_required(VERSION 3.1)
project(fuseupperfs C)

set(CMAKE_C_STANDARD 11)

include_directories(src)
add_executable(${PROJECT_NAME}
        src/error.c
        src/error.h
        src/fuse_ops.c
        src/fuse_ops.h
        src/fuse_utime.c
        src/fuse_utime.h
        src/fuseupperfs.c
        src/quota.c
        src/quota.h
        src/space.c
        src/space.h
        src/xattr_store.c
        src/xattr_store.h)

if (DEFINED _FILE_OFFSET_BITS)
else ()
    ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)
endif ()

if (DEFINED FUSE3)
    include_directories(/usr/include/fuse3)
    target_link_libraries(${PROJECT_NAME} fuse3)
    ADD_DEFINITIONS(-DFUSE3)
else ()
    target_link_libraries(${PROJECT_NAME} fuse)
endif ()
target_link_libraries(${PROJECT_NAME} pthread leveldb)

set(ignoreMe "${FUSE3}")
# cmake -DCMAKE_BUILD_TYPE=Debug . 编译后可以用 gdb 调试
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
