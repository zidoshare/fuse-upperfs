#include <stdarg.h>
#include <stdio.h>

#include "log.h"

static int loggable = 0;

void up_log_enable(){
  loggable = 1;
}

int up_logf(char* format, ...) {
  if( !loggable) {
    return -1;
  }
  va_list argp;
  va_start(argp,format);
  int ret = vprintf(format,argp);
  va_end(argp);
  return ret;
}
