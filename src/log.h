#ifndef UPPER_FUSE_LOG_H
#define UPPER_FUSE_LOG_H

// 开启 log
void up_log_enable();
// 打印log， 在未开启的情况下，不输出
int up_logf(char* format, ...);

#endif
