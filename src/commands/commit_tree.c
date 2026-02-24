#include <stdio.h>
#include <string.h>

#include "../include/common.h"
#include "../include/core.h"

int handle_commit_tree(int argc, char *argv[]) {
  int result = 1;
  buffer_t out_buf = {0};
  char hash_out[CGIT_HASH_HEX_LEN + 1];
  int persist = 1;
  const char *tree_hash = NULL;
  const char *parent_hash = NULL;
  const char *message = NULL;

  if (argc < 4) {
    fprintf(stderr,
            "usage: cgit commit-tree <tree-hash> [-p <parent-hash>] -m "
            "<commit-message>\n");
    goto cleanup;
  }

  tree_hash = argv[1];

  int i = 2;
  while (i < argc) {
    if (strcmp(argv[i], "-p") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: -p requires an argument\n");
        goto cleanup;
      }
      parent_hash = argv[i + 1];
      i += 2;
    } else if (strcmp(argv[i], "-m") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: -m requires an argument\n");
        goto cleanup;
      }
      message = argv[i + 1];
      i += 2;
    } else {
      fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
      goto cleanup;
    }
  }

  if (!message) {
    fprintf(stderr, "error: -m message is required\n");
    goto cleanup;
  }

  if (is_valid_hash(tree_hash) != CGIT_OK) {
    fprintf(stderr, "error: invalid tree hash '%s'\n", tree_hash);
    goto cleanup;
  }

  if (parent_hash && is_valid_hash(parent_hash) != CGIT_OK) {
    fprintf(stderr, "error: invalid parent hash '%s'\n", parent_hash);
    goto cleanup;
  }

  cgit_error_t err_build =
      build_commit_content(tree_hash, parent_hash, CGIT_AUTHOR_NAME,
                           CGIT_AUTHOR_EMAIL, message, &out_buf);
  if (err_build != CGIT_OK) goto cleanup;

  cgit_error_t err_writing =
      write_object(out_buf.data, out_buf.size, "commit", hash_out, persist);
  if (err_writing != CGIT_OK) goto cleanup;

  printf("%s\n", hash_out);

  result = 0;

cleanup:
  buffer_free(&out_buf);
  return result;
}
