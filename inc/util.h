#ifndef _UTIL_H_
#define _UTIL_H_

#include <stddef.h>

_Noreturn void err( const char* msg, ... );

extern inline size_t size_max( size_t x, size_t y );

#endif
