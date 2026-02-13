#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/commands.h"
#include "../include/common.h"

static int ensure_dir(const char *path) {
  if (mkdir(path, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "Failed to create %s: %s\n", path, strerror(errno));
    return 1;
  }
  return 0;
}

int handle_init(int argc, char *argv[]) {
  int reinit = 0;

  if (mkdir(CGIT_DIR, 0755) == -1) {
    if (errno == EEXIST) {
      reinit = 1;
    } else {
      fprintf(stderr, "Failed to create %s: %s\n", CGIT_DIR, strerror(errno));
      return 1;
    }
  }

  if (ensure_dir(CGIT_OBJECTS_DIR) != 0 || ensure_dir(CGIT_REFS_DIR) != 0)
    return 1;

  if (!reinit) {
    FILE *headFile = fopen(CGIT_HEAD_FILE, "w");
    if (headFile == NULL) {
      fprintf(stderr, "Failed to create %s: %s\n", CGIT_HEAD_FILE,
              strerror(errno));
      return 1;
    }

    fprintf(headFile, "ref: refs/heads/main\n");
    fclose(headFile);
  }

  printf("%s %s directory\n", reinit ? "Reinitialized" : "Initialized",
         CGIT_DIR);
  return 0;
}
