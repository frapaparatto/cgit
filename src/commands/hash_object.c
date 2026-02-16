#include <stdio.h>

#include "../include/common.h"
#include "../include/core.h"

/*
 * hash-object command:
 * 1. validate the command itself (responsibility of this module)
 * 2. build the header <type> <size>\0<content> (not responsibility of this
 * module)
 * 3. compute the sha-1 (not responsibility)
 * 4. compress data (not responsibility)
 * 5. if -w option -> stdout: hash and write object in the cgit/objects (not
 * responsibility)
 * 6. if not -w option -> stdout: hash (responsibility)
 * 7. free allocated resources (responsibility)
 *
 */

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

  /* TODO: check if in the case of the option passed, if argv is 3 because the
   * user can also forgtet filename (e.g. cgit hash-object -w)*/
  if (!opt) {
    f = argv[1];
  } else {
    f = argv[2];
    persist = 1;
  }

  /*
   * Usage git hash-object [-w] path
   *
   * I have to understand two things:
   * - first if used with/without -w option
   * - common operations to both case
   *
   * The only operation that is not common to the case without flag, is the
   * write_object, that is an external operation to our command (layer 3)
   */

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
