
#include <stdio.h>

#include "../include/common.h"
#include "../include/core.h"

int handle_write_tree(int argc, char *argv[]) {
  int result = 1;
  tree_entry_t *entries = NULL;
  size_t count = 0;
  char *curr_dir_path = ".";
  buffer_t out = {0};
  int persist = 1;
  char hash_out[CGIT_HASH_HEX_LEN + 1];

  cgit_error_t err = write_tree_recursive(curr_dir_path, &entries, &count);
  if (err != CGIT_OK) {
    fprintf(stderr, "Failed to create tree object\n");
    goto cleanup;
  }

  cgit_error_t err_serialize = serialize_tree(entries, count, &out);
  if (err_serialize != CGIT_OK) {
    fprintf(stderr, "Failed to serialize tree object\n");
    goto cleanup;
  }

  cgit_error_t err_writing =
      write_object(out.data, out.size, "tree", hash_out, persist);
  if (err_writing != CGIT_OK) {
    fprintf(stderr, "Failed to write tree object\n");
    goto cleanup;
  }

  result = 0;
cleanup:
  buffer_free(&out);
  free_tree_entries(entries, count);
  return result;
}
