#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>

static void *(*_malloc)(size_t) = malloc;
static void (*_free)(void *) = free;


#include "malloc_override.h"

static size_t mmap_threshold = 1024 * 1024;
static bool malloc_called = false;

void set_mmap_threshold(size_t sz) {
  if(malloc_called) {
    fputs("Tried resizing after malloc called!\n", stderr);
    abort();
  }

  mmap_threshold = sz;
}

void *malloc(size_t sz) {
  malloc_called = true;

  if(sz >= mmap_threshold) {
    size_t *ptr = (size_t *)mmap(
        NULL,
        sz + sizeof(size_t),
        PROT_READ | PROT_WRITE,
        MAP_ANON | MAP_PRIVATE,
        -1,
        0
    );
    if(ptr == MAP_FAILED) {
      errno = ENOMEM;
      return NULL;
    }

    *ptr++ = sz;
    return (void *)ptr;
  } else {
    size_t *ptr = (size_t *)_malloc(sz + sizeof(size_t));
    if(ptr == NULL) {
      errno = ENOMEM;
      return NULL;
    }

    *ptr++ = sz;
    return (void *)ptr;
  }
}

void free(void *ptr) {
  if(ptr == NULL)
    return;

  ptr = (void *)(((size_t *)ptr) - 1);

  if(*(size_t *)ptr >= mmap_threshold) {
    munmap(ptr, *(size_t *)ptr);
  } else
    _free(ptr);
}

void *realloc(void *ptr, size_t sz) {
  if(ptr == NULL)
    return malloc(sz);

  void *newptr = malloc(sz);
  if(newptr == NULL)
    return NULL;

  memcpy(newptr, ptr, *((size_t *)ptr - 1));
  free(ptr);
  return newptr;
}

void *calloc(size_t n, size_t sz) {
  void *ptr = malloc(n * sz);
  if(ptr == NULL)
    return NULL;
  memset(ptr, 0, n * sz);
  return ptr;
}

