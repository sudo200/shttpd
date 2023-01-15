#ifndef ___SHTTPD_SERVER_H___
#define ___SHTTPD_SERVER_H___

#include <sutil/logger.h>

#ifndef noreturn
#define noreturn  __attribute__((noreturn))
#endif  //noreturn

typedef struct {
  logger_t *log;
  noreturn void (*exit)(int code);
} http_server_t;

extern http_server_t server;

#endif  //___SHTTPD_SERVER_H___

