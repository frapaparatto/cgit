#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/core.h"
#include "common.h"

cgit_error_t read_object(const char *hash, git_object_t *obj) {
  char path[CGIT_MAX_PATH_LENGTH];
  cgit_error_t result = CGIT_OK;
  buffer_t buf = {0};
  buffer_t out_buf = {0};

  result = is_valid_hash(hash);
  if (result != CGIT_OK) {
    /* Error is printed by the is_valid_hash() function */
    goto cleanup;
  }

  result = build_object_path(hash, path, CGIT_MAX_PATH_LENGTH);
  if (result != CGIT_OK) {
    goto cleanup;
  }

  result = read_file(path, &buf);
  if (result != CGIT_OK) {
    goto cleanup;
  }

  result = decompress_data(buf.data, buf.size, &out_buf);
  if (result != CGIT_OK) {
    goto cleanup;
  }

  /* Parse type: scan until space separator */
  size_t i = 0;
  while (i < out_buf.size && out_buf.data[i] != ' ') i++;
  if (i >= out_buf.size) {
    fprintf(stderr, "error: invalid object header (no space after type)\n");
    result = CGIT_ERROR_INVALID_OBJECT;
    goto cleanup;
  }
  size_t type_len = i;

  obj->type = malloc(type_len + 1);

  if (!obj->type) {
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }

  memcpy(obj->type, out_buf.data, type_len);
  obj->type[type_len] = '\0';

  /* Parse decimal size until NULL */
  i++; /* Skip space */
  if (i >= out_buf.size) {
    fprintf(stderr, "error: invalid object header (truncated after type)\n");
    result = CGIT_ERROR_INVALID_OBJECT;
    goto cleanup;
  }

  size_t size_val = 0;
  int saw_digit = 0;

  while (i < out_buf.size && out_buf.data[i] != '\0') {
    unsigned char c = out_buf.data[i];

    if (c < '0' || c > '9') {
      fprintf(stderr, "error: invalid object header (bad size)\n");
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }
    saw_digit = 1;
    size_t digit = (size_t)(c - '0');
    if (size_val > (SIZE_MAX - digit) / 10) {
      fprintf(stderr, "error: object size too large to represent\n");
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }

    size_val = size_val * 10 + digit;
    i++;
  }

  if (!saw_digit || i == out_buf.size) {
    fprintf(stderr, "error: invalid object header (no NUL)\n");
    result = CGIT_ERROR_INVALID_OBJECT;
    goto cleanup;
  }

  obj->size = size_val;
  i++;

  if (i > out_buf.size) {
    fprintf(stderr, "error: invalid object header (no data after NUL)\n");
    result = CGIT_ERROR_INVALID_OBJECT;
    goto cleanup;
  }

  size_t payload_len = out_buf.size - i;
  if (payload_len != obj->size) {
    fprintf(stderr, "error: invalid object (size mismatch)\n");
    result = CGIT_ERROR_INVALID_OBJECT;
    goto cleanup;
  }
  obj->data = malloc(payload_len + 1);

  if (!obj->data) {
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }

  memcpy(obj->data, out_buf.data + i, payload_len);
  obj->data[payload_len] = '\0';

cleanup:
  buffer_free(&buf);
  buffer_free(&out_buf);
  return result;
}

cgit_error_t write_object(const unsigned char *data, size_t len,
                          const char *type, char *hash_out, int persist) {
  cgit_error_t result = CGIT_OK;

  return result;
}

void free_object(git_object_t *obj) {
  free(obj->type);
  free(obj->data);
  obj->type = NULL;
  obj->data = NULL;
  obj->size = 0;
}
