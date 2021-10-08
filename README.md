# fuse-upperfs

fuse-upperfs 是一个用户态文件系统，通过 mount 指令代理一个已有文件系统的文件夹，提供:

* 基本文件系统的所有支持
* 额外的强制文件夹大小限制
* 另支持 xattr 额外扩展属性，一般情况下，你不需要考虑底层文件系统是否实现 额外扩展属性。这主要是为解决类似 nfs 文件系统不支持 xattr 属性所带来的的问题。
* 支持 fuse3 和 fuse2

# 构建与安装

本项目使用以下依赖：

* libefuse3 / libfuse
* leveldb
* cmake

因此为了构建项目：

centos:

```shell
$ yum install libfuse3-devel libfuse-devel leveldb-devel cmake
$ cmake -DCMAKE_BUILD_TYPE=Release . # 使用 fuse2 
$ cmake -DCMAKE_BUILD_TYPE=Release -DFUSE3=1 . # 使用 fuse3
$ make
````


# 挂载文件系统

> 用户态文件系统意味着你可以使用非 root 执行相关指令。

通过以下指定挂载文件夹

```shell
$ fuseupperfs mount <basedir> <mountpoint> [-s<size>] [-u<B|K|M|G|T>] [-x <xattr db_path>] [-- <fuse options>]
```

* -s : 限制 size。
* -u : 限制 size 的单位，默认为 B 也就是 byte。
* -x ： xattr 数据库存储路径，默认为挂载目录下的 .xattr 目录

fuse options:

会透传到 fuse 的指令，例如携带 -d 指令，则进程运行在前台

接下来 basedir 将被代理，并且可以在 mount 时设置挂载点的大小限制。

取消挂载通过以下指令：

```shell
$ umount <mountpoint>
```
