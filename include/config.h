#ifndef ___SHTTPD_CONFIG_H___
#define ___SHTTPD_CONFIG_H___

#include <sys/socket.h>

#include <sutil/types.h>

typedef struct {
  bool interactive;
  fd_t sock_fd;
  char *module_path;
  string_array_t module_opts;
} shttpd_config_t;

extern shttpd_config_t config;

void set_config(int argc, char **argv);

#endif  //___SHTTPD_CONFIG_H___

