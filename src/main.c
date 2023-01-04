#include "malloc_override.h"
#include "server.h"

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sutil/dmem.h>
#include <sutil/logger.h>
#include <sutil/util.h>

#define die(msg)  do { logger_fatal_f(server.log, "%s: %m", msg); exit(EXIT_FAILURE); } while(0)

static volatile bool run = true;
static volatile int exit_code;
static jmp_buf jump;

__attribute__((noreturn)) static void server_exit(int code) {
  run = false;
  exit_code = code;
  longjmp(jump, 1);
}

http_server_t server = {
  .log = NULL,
  .exit = server_exit,
};

__attribute__((constructor)) static void constructor(void) {
  ualloc = malloc;
  urealloc = realloc;
  ufree = free;

  server.log = logger_new(NULL, NULL, false);
}

__attribute__((destructor)) static void destructor(void) {
  logger_destroy(server.log);
}


void sighandler(int sig) {
  switch (sig) {
    case SIGSEGV: {
      logger_fatal(server.log, "Segmentation fault detected!");
      logger_fatal(server.log, "Cannot continue!");
      abort();
    }
    break;

    case SIGINT:
    case SIGTERM:
    case SIGHUP: {
      logger_notice(server.log, "Signal received!");
      server.exit(EXIT_SUCCESS);
    }
    break;
  }
}

__attribute__((noreturn)) int main(int argc, char **argv)
{
  fd_t srv_sock;
  struct sockaddr addr = {
    .sa_family = AF_INET
  };
  socklen_t addrlen;

  switch(addr.sa_family) {
    case AF_INET: {
        struct sockaddr_in *in_addr = (struct sockaddr_in *)&addr;
        inet_aton("0.0.0.0", &in_addr->sin_addr);
        in_addr->sin_port = htons(8080);
        addrlen = sizeof(*in_addr);
      }
      break;

    case AF_UNIX: {
        struct sockaddr_un *un_addr = (struct sockaddr_un *)&addr;
        strncpy(un_addr->sun_path, "socket.sock", 108);
        unlink(un_addr->sun_path);
        addrlen = sizeof(*un_addr);
      }
      break;

    default: {
      logger_fatal(server.log, "Address family not supported!");
      exit(EXIT_FAILURE);
    }
  }

  { // Init
    logger_notice(server.log, "Starting shttpd...");

    setsignal(SIGINT, sighandler);
    setsignal(SIGTERM, sighandler);
    setsignal(SIGHUP, sighandler);
    setsignal(SIGSEGV, sighandler);

    if((srv_sock = socket(addr.sa_family, SOCK_STREAM, 0)) < 0)
      die("socket");

    const int tru = true;
    if(setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &tru, sizeof(tru)) < 0)
      die("setsockopt");

    if(bind(srv_sock, &addr, addrlen) < 0)
      die("bind");

    if(listen(srv_sock, 0xFF) < 0)
      die("listen");

    logger_notice(server.log, "shttp started!");
  }
  
  setjmp(jump);
  while (run) {
  }

  
  { // Clean up
    logger_notice(server.log, "Shutting down!");
    shutdown(srv_sock, SHUT_RDWR);

    close(srv_sock);
    logger_notice_f(server.log, "Exiting with code '%d'!", exit_code);
  }

  exit(exit_code);
}

