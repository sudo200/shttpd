#include <string.h>

#include <sutil/hashmap.h>

#include "request_handler.h"

#define stra(str) (void *)str, strlen((const char *)str)

void handle_request(http_request_t *req, http_response_t *res, buffer_t *req_buf, buffer_t **res_buf) {
  res->status.version = HTTP_1_1;
  res->status.status = OK;

  hashmap_put(res->headers, stra("Content-Length"), "13");
  hashmap_put(res->headers, stra("Content-Type"), "text/plain");
  hashmap_put(res->headers, stra("Connection"), "keep-alive");

  *res_buf = buffer_new_from_string("Hello there!\n");
}

