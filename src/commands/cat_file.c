
#include <stdio.h>
#include <string.h>

#include "../include/common.h"
#include "../include/core.h"

static int cmd_cat_file(int opt, const char *exp_type, git_object_t *obj) {
  switch (opt) {
    case 't':
      printf("%s\n", obj->type);
      return 0;

    case 's':
      printf("%zu\n", obj->size);
      return 0;

    case 'e':
      return 0;

    case 'p':
      fwrite(obj->data, 1, obj->size, stdout);
      return 0;

    case 0:
      if (strcmp(obj->type, exp_type) != 0) {
        fprintf(stderr, "fatal: expected %s, got %s\n", exp_type, obj->type);
        return 1;
      }
      fwrite(obj->data, 1, obj->size, stdout);
      return 0;
  }
  return 1;
}

int handle_cat_file(int argc, char *argv[]) {
  int opt = 0;
  int opt_e = 0;
  int result = 1; /* default: failure */
  git_object_t obj = {0};
  const char *obj_hash = NULL;
  const char *exp_type = NULL;

  if (argc != 3) {
    fprintf(stderr,
            "usage: cgit cat-file <type> <object>\n"
            "   or: cgit cat-file (-e | -p | -t | -s) <object>\n");
    goto cleanup;
  }

  /*
   * Here I am detecting the mode, it could be:
   * - FORM A: cgit cat-file <type> <object>
   * - FORM B: cgit cat-file <option> <object>
   */
  if (argv[1][0] == '-' && argv[1][1] != '\0' && argv[1][2] == '\0') {
    opt = argv[1][1];

    if (opt != 'p' && opt != 't' && opt != 's' && opt != 'e') {
      fprintf(stderr, "Invalid option '-%c' (-t | -s | -e | -p)\n", opt);
      goto cleanup;
    }
  }

  if (!opt) exp_type = argv[1];
  obj_hash = argv[2];
  if (opt == 'e') opt_e = 1;

  cgit_error_t err = read_object(obj_hash, &obj, opt_e);
  if (err != CGIT_OK) {
    fprintf(stderr, "Failed to read object %s\n", obj_hash);
    goto cleanup;
  }

  result = cmd_cat_file(opt, exp_type, &obj);

cleanup:
  free_object(&obj);
  return result;
}
