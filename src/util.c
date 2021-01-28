#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../inc/util.h"

_Noreturn void err( const char* msg, ... ) {
  va_list args;
  va_start (args, msg);
  vfprintf(stderr, msg, args);
  va_end (args);
  abort();
}

inline size_t size_max( size_t x, size_t y ) { return (x >= y)? x : y ; }
