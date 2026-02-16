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

  /* Parse header */
  char type_buf[CGIT_MAX_TYPE_LEN];
  size_t content_size = 0;
  size_t payload_offset = 0;

  result = parse_object_header(out_buf.data, out_buf.size, type_buf,
                               sizeof(type_buf), &content_size,
                               &payload_offset);
  if (result != CGIT_OK) {
    goto cleanup;
  }

  /* Validate payload size matches header */
  size_t payload_len = out_buf.size - payload_offset;
  if (payload_len != content_size) {
    fprintf(stderr, "error: invalid object (size mismatch)\n");
    result = CGIT_ERROR_INVALID_OBJECT;
    goto cleanup;
  }

  /* Populate object struct */
  obj->type = strdup(type_buf);
  if (!obj->type) {
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }

  obj->size = content_size;
  obj->data = malloc(payload_len + 1);
  if (!obj->data) {
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }

  memcpy(obj->data, out_buf.data + payload_offset, payload_len);
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
