#include "malloc_override.h"
#include "server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

#include <sutil/dmem.h>
#include <sutil/logger.h>
#include <sutil/util.h>


static bool run = true;
static int exit_code;
static jmp_buf exit_buf;

__attribute__((noreturn)) static void server_exit(int code) {
  run = false;
  exit_code = code;
  longjmp(exit_buf, 0);
}

http_server_t server = {
  .log = NULL,
  .exit = server_exit,
};

__attribute__((constructor)) static void constructor(void) {
  ualloc = malloc;
  urealloc = realloc;
  ufree = free;
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

int main(int argc, char **argv)
{
  server.log = logger_new(NULL, NULL, false);

  logger_notice(server.log, "Starting shttpd...");

  setsignal(SIGINT, sighandler);
  setsignal(SIGTERM, sighandler);
  setsignal(SIGHUP, sighandler);
  setsignal(SIGSEGV, sighandler);

  logger_notice(server.log, "shttp started!");

  setjmp(exit_buf);
  while (run) {
    sleep(1);
  }

  logger_notice(server.log, "Shutting down!");

  sleep(2);

  logger_info_f(server.log, "Exitting with code '%d'! Goodbye!\n", exit_code);
  logger_destroy(server.log);
  return exit_code;
}

