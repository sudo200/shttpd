#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/un.h>

#include <sutil/dmem.h>
#include <sutil/mstring.h>

#include "config.h"
#include "help.h"
#include "server.h"

#define destructor  __attribute__((destructor))

// Default config ////////////////////////////////////

#define DEFAULT_DAEMONIZE false

#define DEFAULT_BIND_ADDR "0.0.0.0"
#define DEFAULT_BIND_PORT 8080

#define DEFAULT_MODULE_PATH NULL

///////////////////////////////////////////////////////

#define die(msg)  do { logger_fatal_fm(server.log, m, "%s: %m", msg); exit(EXIT_FAILURE); } while(0)
#define equals(x,y) (strcmp(x,y) == 0)

shttpd_config_t config;

static struct sockaddr_in mkaddr(const char *addr) {
  struct sockaddr_in inet_addr = {
    .sin_family = AF_INET
  };
  if(addr == NULL)
    addr = DEFAULT_BIND_ADDR;

  char *_addr = strdup(addr);
  
  char *port_str = strrchr(_addr, ':');
  if(port_str != NULL)
    *port_str++ = '\0';

  inet_aton(_addr, &inet_addr.sin_addr);
  inet_addr.sin_port = htons(
    port_str != NULL ?
    strtol(port_str, NULL, 10) :
    DEFAULT_BIND_PORT
  );
  free(_addr);

  return inet_addr;
}

static void set_sock(const char *address) {
  struct sockaddr *addr;
  socklen_t addr_len;

  if(address == NULL)
    address = DEFAULT_BIND_ADDR;

  if(startswith(address, "unix:")) {
    address += 5;
    struct sockaddr_un adr = {
      .sun_family = AF_UNIX
    };
    strncpy(adr.sun_path, address, sizeof(adr.sun_path));
    addr = (struct sockaddr *)&adr;
    addr_len = sizeof(adr);
    unlink(address);
  } else {
    struct sockaddr_in adr = mkaddr(address);
    addr = (struct sockaddr *)&adr;
    addr_len = sizeof(adr);
  }

  marker *m = marker_new("SOCKETFACTORY");

  if((config.sock_fd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0)
    die("socket");

  const int tru = 1;
  if(setsockopt(config.sock_fd, SOL_SOCKET, SO_REUSEADDR, &tru, sizeof(tru)))
    die("setsockopt");

  if(setsockopt(config.sock_fd, SOL_SOCKET, SO_KEEPALIVE, &tru, sizeof(tru)))
    die("setsockopt");

  if(bind(config.sock_fd, addr, addr_len) < 0)
    die("bind");

  if(listen(config.sock_fd, 0xFF) < 0)
    die("listen");

  marker_destroy(m);
}

void set_config(int argc, char **argv) {
  memset(&config, 0, sizeof(config));
  marker *m = marker_new("ARGPARSER");
  const char *address = NULL;

  for(int i = 1; i < argc; ++i) {
    if(equals(argv[i], "-h") || equals(argv[i], "--help"))
      help();
    else if(equals(argv[i], "-i") || equals(argv[i], "--interactive"))
      config.interactive = true;
    else if(equals(argv[i], "-a") || equals(argv[i], "--address"))
      address = argv[++i];
    else if(equals(argv[i], "-m") || equals(argv[i], "--module"))
      config.module_path = argv[++i];
    /*else if(equals(argv[i], "-o") || equals(argv[i], "--options"))
      strspl(&config.module_opts, argv[++i], " ");*/
  }

  set_sock(address);

  marker_destroy(m);
}

