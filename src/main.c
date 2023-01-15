#include "malloc_override.h"
#include "request_handler.h"
#include "server.h"
#include "worker.h"

#include <dlfcn.h>
#include <pthread.h>

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

#include "config.h"

#define constructor __attribute__((constructor))
#define destructor  __attribute__((destructor))
#define noreturn    __attribute__((noreturn))

#define die(msg)  do { logger_fatal_fm(server.log, m, "%s: %m", msg); exit(EXIT_FAILURE); } while(0)

static volatile bool run = true;
static volatile int exit_code;
static jmp_buf jump;

static noreturn void server_exit(int code) {
  run = false;
  exit_code = code;
  longjmp(jump, 1);
}

http_server_t server = {
  .log = NULL,
  .exit = server_exit,
};

static marker *m;

static constructor void constr(void) {
  ualloc = malloc;
  urealloc = realloc;
  ufree = free;

  server.log = logger_new(NULL, NULL, false);
  m = marker_new("MAIN");
}

static destructor void destr(void) {
  marker_destroy(m);
  logger_destroy(server.log);
}


void sighandler(int sig) {
  switch (sig) {
    case SIGSEGV: {
      logger_fatal_m(server.log, m, "Segmentation fault detected!");
      logger_fatal_m(server.log, m, "Cannot continue!");
      abort();
    }
    break;

    case SIGINT:
    case SIGTERM:
    case SIGHUP: {
      logger_notice_m(server.log, m, "Signal received!");
      server.exit(EXIT_SUCCESS);
    }
    break;
  }
}

noreturn int main(int argc, char **argv)
{
  worker_param_t *params;
  socklen_t addr_len;
  void *module = NULL;

  { // Init
    set_config(argc, argv);

    if(!config.interactive) {
      pid_t p = fork();
      if(p < 0)
        die("Could not fork into background");

      if(p > 0) { // Parent
        sleep(1);
        _exit(EXIT_SUCCESS);
      } else
        setsid();
    }

    loggerlevel = TRACE;

    logger_notice_m(server.log, m, "Starting shttpd...");
    logger_notice_fm(server.log, m, "PID: %d", getpid());

    signal(SIGALRM, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setsignal(SIGINT, sighandler);
    setsignal(SIGTERM, sighandler);
    setsignal(SIGHUP, sighandler);
    setsignal(SIGSEGV, sighandler);

    uint8_t dummy[0xFF];
    addr_len = sizeof(dummy);
    getsockname(config.sock_fd, (struct sockaddr *)dummy, &addr_len);

    do {
      if(config.module_path != NULL) {
        if((module = dlopen(config.module_path, RTLD_LAZY | RTLD_LOCAL)) == NULL) {
          logger_error_fm(server.log, m, "Error loading module: %s", dlerror());
          break;
        }

        void *func = dlsym(module, "handle_request");
        if(func == NULL) {
          logger_error_fm(server.log, m, "Error resolving handler function from module: %s", dlerror());
        } else
          *(void **)&handle_request = func;
      }
    } while(0);

    logger_notice_m(server.log, m, "shttp started!");
  }
  
  setjmp(jump);
  while (run) {
    params = (worker_param_t *)ualloc(sizeof(*params));
    params->addrlen = addr_len;
    params->buffer_size = 4095;
    params->con = accept(config.sock_fd, &params->addr, &params->addrlen);

    if(params->con < 0) {
      ufree(params);
      logger_error_fm(server.log, m, "Error handling connection: %m", NULL);
      continue;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, connection_worker, params);
    pthread_detach(thread);
  }

  
  { // Clean up
    logger_notice_m(server.log, m, "Shutting down!");
    shutdown(config.sock_fd, SHUT_RDWR);
    ufree(params);

    close(config.sock_fd);
    if(config.module_opts.arr != NULL)
      ufree(*config.module_opts.arr);
    ufree(config.module_opts.arr);
    if(module != NULL)
      dlclose(module);
    logger_notice_fm(server.log, m, "Exiting with code '%d'!", exit_code);
  }

  exit(exit_code);
}

