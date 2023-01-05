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
      ret = snprintf((char *)buffer_get(buffer), buffer_size(buffer),
            "HTTP/1.0 400\r\n"
            "Connection: close\r\n"
            "Server: shttpd\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<h1>400 Bad Request</h1>"
          );
      goto _send;
    }

    http_response_t res = {
      .headers = hashmap_new(fnv1a)
    };

    hashmap_put(res.headers, "Server", 6UL, "shttpd");
    hashmap_put(res.headers, "Connection", 10UL, "keep-alive");
    hashmap_put(res.headers, "Keep-Alive", 10UL, "timeout=10, max=1000");

    buffer_t *req_buffer = buffer_new_from_pointer(
        (byte *)buffer_get(buffer) + body_offset,
        buffer_size(buffer) - body_offset
        );
    buffer_t *res_buffer = NULL;

    handle_request(&req, &res, req_buffer, &res_buffer);

    buffer_destroy(req_buffer);
    hashmap_destroy(req.headers);

    size_t status_offset, headers_offset;
    response_status_to_string(&res.status, buffer_get(buffer), buffer_size(buffer), &status_offset);
    headers_to_string(&res.headers, (byte *)buffer_get(buffer) + status_offset, buffer_size(buffer) - status_offset, &headers_offset);
   
    hashmap_destroy(res.headers);

    size_t send_size = 0UL, sent = 0UL;
    if(res_buffer != NULL) {
      memcpy(
          (byte *)buffer_get(buffer) + status_offset + headers_offset - 2,
          buffer_get(res_buffer),
          send_size = buffer_size(res_buffer)
          );
      buffer_destroy(res_buffer);
    }
    send_size += status_offset + headers_offset - 2;

_send:
    while (send_size > sent) {
    ret = send(
        params->con,
        (byte *)buffer_get(buffer) + sent,
        send_size - sent,
        MSG_NOSIGNAL
    );
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
    sent += ret;
    }
  }
 
_break:
  close(params->con);
  logger_info_m(server.log, m, "Exiting...");
  marker_destroy(m);
  buffer_destroy(buffer);
  ufree(p);
  return NULL;
}

