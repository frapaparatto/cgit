

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "core.h"

int handle_ls_tree(int argc, char *argv[]) {
  int opt_name_only = 0;
  int result = 1;
  const char *obj_hash = NULL;

  size_t count = 0;
  tree_entry_t *entries = NULL;
  git_object_t obj = {0};

  if (argc < 2) {
    fprintf(stderr, "usage: cgit ls-tree [--name-only] <object>\n");
    goto cleanup;
  }

  if (argc == 3) {
    if (strcmp(argv[1], "--name-only") != 0) {
      fprintf(stderr, "invalid option: %s\n", argv[1]);
      goto cleanup;
    }
    opt_name_only = 1;
    obj_hash = argv[2];

  } else {
    /* Assuming that argc is exactly 2 */
    obj_hash = argv[1];
  }

  cgit_error_t err_reading = read_object(obj_hash, &obj);
  if (err_reading != CGIT_OK) {
    fprintf(stderr, "Failed to read object %s\n", obj_hash);
    goto cleanup;
  }

  if (strcmp(obj.type, "tree") != 0) {
    fprintf(stderr, "error: not a tree object\n");
    goto cleanup;
  }

  cgit_error_t err_parsing = parse_tree(obj.data, obj.size, &entries, &count);
  if (err_parsing != CGIT_OK) {
    fprintf(stderr, "Failed to read tree object %s\n", obj_hash);
    goto cleanup;
  }

  if (opt_name_only) {
    for (size_t i = 0; i < count; i++) {
      printf("%s\n", entries[i].name);
    }
  } else {
    for (size_t i = 0; i < count; i++) {
      printf("%06o %s %s\t%s\n", entries[i].mode, entries[i].type,
             entries[i].hash, entries[i].name);
    }
  }

  result = 0;

cleanup:
  free_object(&obj);
  free_tree_entries(entries, count);
  return result;
}
