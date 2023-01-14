#include <errno.h>
#include <string.h>

#include <pthread.h>

#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <poll.h>

#include <shttp/http_request_parser.h>
#include <shttp/http_response_parser.h>

#include <sutil/buffer.h>
#include <sutil/dmem.h>
#include <sutil/hash.h>
#include <sutil/hashmap.h>
#include <sutil/logger.h>
#include <sutil/mstring.h>
#include <sutil/util.h>

#include "request_handler.h"
#include "server.h"
#include "worker.h"

#define thread_local  __thread

__attribute__((always_inline)) static void set_nonblocking(fd_t fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

thread_local struct {
  size_t buffer_size;
  http_version_t version;
  http_headers_t headers;
  fd_t fd;
} tls;

static void cp_headers(void *key, size_t keylen, void *value, void *pipe) {
  hashmap_put((hashmap_t *)pipe, key, keylen, value);
}

static int write_head(http_status_t code, http_headers_t headers) {
  http_response_t res = {
    .status = (http_response_status_t) {
      tls.version,
      code,
    },
    tls.headers
  };

  if(headers != NULL)
    hashmap_foreach(headers, cp_headers, res.headers);
  
  buffer_t *buf = buffer_new(tls.buffer_size);
  if(buf == NULL)
    return -1;

  http_response_to_string(&res, buffer_get(buf), buffer_size(buf), NULL);
  const size_t to_send = strlen((const char *)buffer_get(buf));
  size_t sent = 0UL;
  const void *buffer = buffer_get(buf);

  while(to_send > sent) {
    ssize_t s = send(tls.fd, (uint8_t *)buffer + sent, to_send - sent, MSG_NOSIGNAL);
    if(s < 0) {
      buffer_destroy(buf);
      return -1;
    }

    sent += s;
  }

  buffer_destroy(buf);
  return 0;
}

static int write_buffer(const void *buf, size_t to_send) {
  size_t sent = 0UL;

  while(to_send > sent) {
    ssize_t s = send(tls.fd, (uint8_t *)buf + sent, to_send - sent, MSG_NOSIGNAL);
    if(s < 0)
      return -1;
    sent += s;
  }
  return 0;
}


void *connection_worker(void *p) {
  worker_param_t *params = (worker_param_t *)p;
  pthread_t self = pthread_self();
  char *name;
  msprintf(&name, "WORKER-%lu", *(unsigned long *)&self);
  marker *m = marker_new(name);
  ufree(name);

  tls.buffer_size = params->buffer_size;
  tls.fd = params->con;

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

  buffer_t *buffer = buffer_new(params->buffer_size);
  if(buffer == NULL) {
    logger_error_fm(server.log, m, "Could not allocate buffer: %m", NULL);
    goto _break;
  }

  const char *const close_msg = "Client unexpectedly closed connection";

  set_nonblocking(params->con);

  while(true) {
    struct pollfd fds = {
      .fd = params->con,
      .events = POLLIN
    };
    if(poll(&fds, 1, 10000) == 0)
      goto _break;

    ssize_t ret = recv(params->con, buffer_get(buffer), buffer_size(buffer), 0);
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
    if(ret == 0)
      goto _break;

    http_request_t req;
    size_t body_offset;
    parse_status_t status = parse_request(&req, buffer_get(buffer), ret, &body_offset);

    if(status != PARSE_OK) { // Bad request
      logger_error_m(server.log, m, "Error parsing request!");
      goto _break;
    }

    static const response_functions_t res = {
      write_head,
      write_buffer
    };

    buffer_t *req_buffer = buffer_new_from_pointer(
        (byte *)buffer_get(buffer) + body_offset,
        buffer_size(buffer) - body_offset
        );

    tls.headers = hashmap_new(fnv1a);
    hashmap_put(tls.headers, "Connection", 10UL, "keep-alive");
    hashmap_put(tls.headers, "Content-Length", 14UL, "0");
    hashmap_put(tls.headers, "Keep-Alive", 10UL, "timeout=10");
    hashmap_put(tls.headers, "Server", 6UL, "shttpd");
    tls.version = req.status.version;

    handle_request(&req, req_buffer, &res);

    hashmap_destroy(tls.headers);
    hashmap_destroy(req.headers);
    buffer_destroy(req_buffer);
  }
 
_break:
  close(params->con);
  logger_info_m(server.log, m, "Exiting...");
  marker_destroy(m);
  buffer_destroy(buffer);
  ufree(p);
  return NULL;
}

