#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/common.h"

cgit_error_t build_object_path(const char *hash, char *path_out,
                               size_t path_size) {
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

cgit_error_t is_valid_hash(const char *hash) {
  cgit_error_t result = CGIT_OK;
  size_t objlen = strlen(hash);

  if (objlen != CGIT_HASH_HEX_LEN) {
    fprintf(stderr,
            "error: invalid hash name '%s': expected 40 hexadecimal "
            "characters\n",
            hash);
    result = CGIT_ERROR_INVALID_ARGS;
  }

  for (size_t i = 0; i < CGIT_HASH_HEX_LEN; i++) {
    if (!isxdigit((unsigned char)hash[i])) {
      fprintf(stderr,
              "error: invalid hash name '%s': non-hexadecimal character\n",
              hash);
      result = CGIT_ERROR_INVALID_ARGS;
    }
  }
  return result;
}

void buffer_free(buffer_t *buf) {
  free(buf->data);

  buf->data = NULL;
  buf->size = 0;
  buf->capacity = 0;
}

cgit_error_t build_object_header(const unsigned char *data, size_t file_size,
                                 const char *type, buffer_t *output) {
  size_t header_len = snprintf(NULL, 0, "%s %zu", type, file_size);

  if (file_size > SIZE_MAX - header_len - 1) {
    fprintf(stderr, "unable to represent total size\n");
    return CGIT_ERROR_MEMORY;
  }
  size_t total_size = header_len + 1 + file_size;

  output->data = malloc(total_size);
  if (!output->data) {
    fprintf(stderr, "error: %s\n", strerror(errno));
    return CGIT_ERROR_MEMORY;
  }

  snprintf((char *)output->data, header_len + 1, "%s %zu", type, file_size);
  memcpy(output->data + header_len + 1, data, file_size);
  output->size = total_size;
  output->capacity = total_size;

  return CGIT_OK;
}

cgit_error_t parse_object_header(const unsigned char *buf, size_t buf_len,
                                 char *type, size_t type_len,
                                 size_t *content_size, size_t *payload_offset) {
  /* Parse type: scan until space separator */
  size_t i = 0;
  while (i < buf_len && buf[i] != ' ') i++;
  if (i >= buf_len) {
    fprintf(stderr, "error: invalid object header (no space after type)\n");
    return CGIT_ERROR_INVALID_OBJECT;
  }

  if (i + 1 > type_len) {
    fprintf(stderr, "error: object type too long\n");
    return CGIT_ERROR_INVALID_OBJECT;
  }
  memcpy(type, buf, i);
  type[i] = '\0';

  /* Parse decimal size until NUL */
  i++; /* Skip space */
  if (i >= buf_len) {
    fprintf(stderr, "error: invalid object header (truncated after type)\n");
    return CGIT_ERROR_INVALID_OBJECT;
  }

  size_t size_val = 0;
  int saw_digit = 0;

  while (i < buf_len && buf[i] != '\0') {
    unsigned char c = buf[i];

    if (c < '0' || c > '9') {
      fprintf(stderr, "error: invalid object header (bad size)\n");
      return CGIT_ERROR_INVALID_OBJECT;
    }
    saw_digit = 1;
    size_t digit = (size_t)(c - '0');
    if (size_val > (SIZE_MAX - digit) / 10) {
      fprintf(stderr, "error: object size too large to represent\n");
      return CGIT_ERROR_INVALID_OBJECT;
    }

    size_val = size_val * 10 + digit;
    i++;
  }

  if (!saw_digit || i == buf_len) {
    fprintf(stderr, "error: invalid object header (no NUL)\n");
    return CGIT_ERROR_INVALID_OBJECT;
  }

  i++; /* Skip NUL terminator */

  *content_size = size_val;
  *payload_offset = i;

  return CGIT_OK;
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
