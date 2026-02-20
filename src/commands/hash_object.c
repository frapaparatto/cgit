#include <stdio.h>

#include "../include/common.h"
#include "../include/core.h"

int handle_hash_object(int argc, char *argv[]) {
  int opt = 0;
  int result = 1;
  int persist = 0; /* Since -w is optional, the default is to avoid writing */
  char *f;
  char hash_out[CGIT_HASH_HEX_LEN + 1];
  char *type = CGIT_DEFAULT_OBJ_TYPE;
  buffer_t buf = {0};

  if (argc < 2) {
    fprintf(stderr, "usage: cgit hash-object [-w] <file>\n");
    goto cleanup;
  }

  if (argv[1][0] == '-' && argv[1][1] != '\0' && argv[1][2] == '\0') {
    opt = argv[1][1];

    if (opt != 'w') {
      fprintf(stderr, "Invalid option '-%c'\n", opt);
      goto cleanup;
    }
  }

  if (opt && argc < 3) {
    fprintf(stderr,
            "Missing file name\n"
            "usage: cgit hash-object [-w] <file>\n");
    goto cleanup;
  }

  if (!opt) {
    f = argv[1];
  } else {
    f = argv[2];
    persist = 1;
  }

  cgit_error_t err_read = read_file(f, &buf);
  if (err_read != CGIT_OK) {
    fprintf(stderr, "Failed to read file '%s'\n", f);
    goto cleanup;
  }

  cgit_error_t err_write =
      write_object(buf.data, buf.size, type, hash_out, persist);

  if (err_write != CGIT_OK) {
    fprintf(stderr, "Failed to create the object\n");
    goto cleanup;
  }

  printf("%s\n", hash_out);
  result = 0;

cleanup:
  buffer_free(&buf);
  return result;
}
