#ifndef ___SHTTP_REQUEST_HANDLER_H___
#define ___SHTTP_REQUEST_HANDLER_H___

#include <shttp/http_request_parser.h>
#include <shttp/http_response_parser.h>

#include <sutil/buffer.h>

void handle_request(http_request_t *req, http_response_t *res, buffer_t *req_buf, buffer_t **res_buf);

#endif  //___SHTTP_REQUEST_HANDLER_H___

