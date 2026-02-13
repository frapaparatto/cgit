#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/common.h"

cgit_error_t build_object_path(const char *hash, char *path_out,
                               size_t path_size) {
  /* This function is not responsible for validating hash or other things, this
   * is responsabilities of the caller */

  /* Since path_out and path_size are passed to the function, I assume they've
   * already declared with some macros*/

  char dir[CGIT_DIR_BUF_SIZE];
  char object[CGIT_OBJ_NAME_BUF_SIZE];

  memcpy(dir, hash, 2);
  memcpy(object, hash + 2, 38);

  dir[CGIT_DIR_BUF_SIZE - 1] = '\0';
  object[CGIT_OBJ_NAME_BUF_SIZE - 1] = '\0';

  int written =
      snprintf(path_out, path_size, CGIT_OBJECTS_DIR "/%s/%s", dir, object);

  if (written < 0 || (size_t)written >= path_size) {
    fprintf(stderr, "error: object path truncated\n");
    return CGIT_ERROR_IO;
  }

  return CGIT_OK;
}

int is_valid_hash(const char *hash) {
  size_t objlen = strlen(hash);
  if (objlen != CGIT_HASH_HEX_LEN) {
    fprintf(stderr,
            "error: invalid hash name '%s': expected 40 hexadecimal "
            "characters\n",
            hash);
    return 1;
  }

  for (size_t i = 0; i < CGIT_HASH_HEX_LEN; i++) {
    if (!isxdigit((unsigned char)hash[i])) {
      fprintf(stderr,
              "error: invalid hash name '%s': non-hexadecimal character\n",
              hash);
      return 1;
    }
  }
  return 0;
}

void buffer_free(buffer_t *buf) {
  free(buf->data);

  buf->data = NULL;
  buf->size = 0;
  buf->capacity = 0;
}

cgit_error_t read_file(const char *path, buffer_t *output) {
  FILE *file = NULL;
  cgit_error_t result = CGIT_OK;

  struct stat st;
  if (stat(path, &st) != 0) {
    fprintf(stderr, "stat: %s: %s\n", path, strerror(errno));
    result = CGIT_ERROR_FILE_NOT_FOUND;
    goto cleanup;
  }
  size_t file_size = (size_t)st.st_size;

  output->data = malloc(file_size);
  if (!output->data) {
    fprintf(stderr, "error: out of memory\n");
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }

  file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
    result = CGIT_ERROR_FILE_NOT_FOUND;
    goto cleanup;
  }

  size_t bytes_read = fread(output->data, 1, file_size, file);
  if (bytes_read != file_size) {
    fprintf(stderr, "error: short read on '%s'\n", path);
    result = CGIT_ERROR_IO;
    goto cleanup;
  }

  output->size = file_size;
  output->capacity = file_size;

cleanup:
  if (file) fclose(file);
  if (result != CGIT_OK) {
    buffer_free(output);
  }
  return result;
}
