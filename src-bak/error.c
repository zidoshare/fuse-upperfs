#include "error.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void
error(char* msg)
{
  perror(msg);
  exit(errno);
}
