#ifndef ___SHTTPD_MALLOC_OVERRIDE_H___
#define ___SHTTPD_MALLOC_OVERRIDE_H___

#include <sutil/types.h>

void set_mmap_threshold(size_t sz);

void *malloc(size_t sz);
void *calloc(size_t n, size_t sz);
void *realloc(void *ptr, size_t sz);
void free(void *ptr);

#endif  //___SHTTPD_MALLOC_OVERRIDE_H___

