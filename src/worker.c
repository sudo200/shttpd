#include <errno.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/select.h>

#include <shttp/http_request_parser.h>
#include <shttp/http_response_parser.h>

#include <sutil/dmem.h>
#include <sutil/hash.h>
#include <sutil/hashmap.h>
#include <sutil/logger.h>
#include <sutil/mstring.h>
#include <sutil/util.h>

#include "server.h"
#include "worker.h"

__attribute__((always_inline)) static bool isopen(fd_t fd) {
  char buf;
  return recv(fd, &buf, 1, MSG_PEEK | MSG_DONTWAIT) != 0;
}

__attribute__((always_inline)) static void set_nonblocking(fd_t fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void *connection_worker(void *p) {
  worker_param_t *params = (worker_param_t *)p;
  pthread_t self = pthread_self();
  char *name;
  msprintf(&name, "WORKER-%lu", *(unsigned long *)&self);
  marker *m = marker_new(name);
  ufree(name);

  switch (params->addr.sa_family) {
    case AF_INET: {
      struct sockaddr_in *inet_addr = (struct sockaddr_in *)&params->addr;

      logger_info_fm(server.log, m, "Incoming connection from %s:%hu", inet_ntoa(inet_addr->sin_addr), ntohs(inet_addr->sin_port));
    }
    break;

    case AF_UNIX: {
      logger_info_m(server.log, m, "Incoming connection");
    }
    break;
  }

  void *buffer = ualloc(params->buffer_size);
  if(buffer == NULL) {
    logger_error_fm(server.log, m, "Could not allocate buffer: %m", NULL);
    goto _break;
  }

  const char *const close_msg = "Client unexpectedly closed connection";

  set_nonblocking(params->con);

  while(true) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(params->con, &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    if(select(params->con + 1, &read_fds, NULL, NULL, &timeout) == 0 || !isopen(params->con))
      goto _break;

    ssize_t ret = recv(params->con, buffer, params->buffer_size, 0);
    if(ret < 0)
      switch(errno) {
        case EPIPE:
        case ECONNRESET:
          logger_info_m(server.log, m, close_msg);
          goto _break;

        default:
          logger_error_fm(server.log, m, "Error while receiving: %m", NULL);
          continue;
      }

    http_request_t req;
    size_t body_offset;
    parse_status_t status = parse_request(&req, buffer, ret, &body_offset);

    if(status != PARSE_OK) { // Panic
      goto _break;
    }

    // Handle request

    ret = sprintf((char *)buffer,
        "HTTP/1.1 200\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello there!\n"
    );

    hashmap_destroy(req.headers);

    ret = send(params->con, buffer, ret, MSG_NOSIGNAL);
    if(ret < 0)
      switch(errno) {
        case EPIPE:
        case ECONNRESET:
          logger_info_m(server.log, m, close_msg);
          goto _break;

        default:
          logger_error_fm(server.log, m, "Error while sending: %m", NULL);
          continue;
      }
  }
 
_break:
  close(params->con);
  logger_info_m(server.log, m, "Exiting...");
  marker_destroy(m);
  ufree(buffer);
  ufree(p);
  return NULL;
}

