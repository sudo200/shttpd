#ifndef ___SHTTPD_WORKER_H___
#define ___SHTTPD_WORKER_H___

#include <sys/socket.h>

#include <sutil/types.h>

typedef struct {
  size_t buffer_size;
  fd_t con;
  struct sockaddr addr;
  socklen_t addrlen;
} worker_param_t;

void *connection_worker(void *worker_param);

#endif  //___SHTTPD_WORKER_H___

