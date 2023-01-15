#include <sutil/dmem.h>
#include <sutil/hash.h>
#include <sutil/hashmap.h>
#include <sutil/mstring.h>

#include "request_handler.h"

static void __default_handler(http_request_t *req, buffer_t *req_buf, const response_functions_t *res) {
  http_headers_t headers = hashmap_new(fnv1a);
    
  static const char body[] = "<!DOCTYPE html>"
    "<html lang=\"en\">"
    "<head>"
    "<title>&#9824; Thank you for using shttpd!</title>"
    "<style>"
    ":root {"
    "color: whitesmoke;"
    "background-color:  black;"
    "font-family: sans-serif;"
    "}"
    "</style>"
    "</head>"
    "<body>"
    "<h1>&#9824; Thank you for using <em>shttpd</em>!</h1>"
    "<p>To get started, specify a shttp-module using the -m flag</p>"
    "</body>"
    "</html>";
  static const size_t body_size = sizeof(body) - 1;
  char *len;
  
  msprintf(&len, "%lu", body_size);

  hashmap_put(headers, "Content-Type", 12UL, "text/html; charset=ascii");
  hashmap_put(headers, "Content-Length", 14UL, len);

  res->write_head(OK, headers);
  hashmap_destroy(headers);
  ufree(len);

  res->write_buffer(body, body_size);
}

void (*handle_request)(http_request_t *req, buffer_t *req_buf, const response_functions_t *res) = __default_handler;

