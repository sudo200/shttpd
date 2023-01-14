#include <string.h>

#include <sutil/hash.h>
#include <sutil/hashmap.h>

#include "request_handler.h"

#define stra(str) (void *)str, strlen((const char *)str)

void handle_request(http_request_t *req, buffer_t *req_buf, const response_functions_t *res) {
  if(strcmp(req->status.url, "/") == 0) {
    http_headers_t headers = hashmap_new(fnv1a);

    hashmap_put(headers, stra("Content-Type"), "text/plain");
    hashmap_put(headers, stra("Content-Length"), "13");

    res->write_head(OK, headers);
    hashmap_destroy(headers);

    res->write_buffer(stra("Hello there!\n"));
  }
  else {
    res->write_head(NOT_FOUND, NULL);
  }
}

