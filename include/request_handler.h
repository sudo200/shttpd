#ifndef ___SHTTP_REQUEST_HANDLER_H___
#define ___SHTTP_REQUEST_HANDLER_H___

#include <errno.h>

#include <shttp/http_request_parser.h>
#include <shttp/http_response_parser.h>

#include <sutil/buffer.h>

typedef struct {
  int (*write_head)(http_status_t code, http_headers_t headers);

  int (*write_buffer)(const void *buf, size_t len);
} response_functions_t;

void handle_request(http_request_t *req, buffer_t *req_buf, const response_functions_t *res);

#endif  //___SHTTP_REQUEST_HANDLER_H___

