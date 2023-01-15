#include <stdio.h>
#include <stdlib.h>

#include "help.h"

noreturn void help(void) {
  puts(
    "shttp - sudo http daemon\n"
    "========================\n"
    "-a,--address\tIPv4 address and port or unix socket to bind to.\n"
    "\t\tBinding to 0.0.0.0 binds to all interfaces.\n"
    "\t\tTo bind to a unix socket, prepend \"unix:\" to the path.\n"
    "\n"
    "-m,--module\tThe module to load.\n"
    "\t\tshttpd by itself is kindof useless, it needs a shttpd module\n"
    "\t\tto do useful work like serving files, executing CGI-programs, etc.\n"
    "\n"
    "-i,--interactive\tDo not fork into background\n"
    "\n"
    "sudo200 and contributors"
  );
  exit(EXIT_FAILURE);
}

