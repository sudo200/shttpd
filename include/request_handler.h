#ifndef ___SHTTP_REQUEST_HANDLER_H___
#define ___SHTTP_REQUEST_HANDLER_H___

#include <errno.h>

#include <shttp/statuses.h>
#include <shttp/http_request_parser.h>

#include <sutil/buffer.h>

typedef struct {
  /**
   * Writes the status and header lines out to the client.
   *
   * @param code  The status code to respond with.
   * @param headers The headers to send, or defaults if NULL.
   * @return 0 on success, a negative value on error.
   */
  int (*write_head)(http_status_t code, http_headers_t headers);

  /**
   * Writes a buffer as body to the client.
   *
   * @param buf Address of the buffer.
   * @param len Length of the buffer.
   * @return 0 on soccess, a negative value on error.
   */
  int (*write_buffer)(const void *buf, size_t len);
} response_functions_t;

extern void (*handle_request)(http_request_t *req, buffer_t *req_buf, const response_functions_t *res);

#endif  //___SHTTP_REQUEST_HANDLER_H___

