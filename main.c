#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {

  if (argc < 2) {
    fprintf(stderr, "Usage: ./cgit <command> [<args>]\n");
    return 1;
  }

  const char *command = argv[1];

  if (strcmp(command, "init") == 0)
    fprintf(stdout, "This prints out to stdout.\n");

  return 0;
}
