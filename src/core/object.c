#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../include/core.h"
#include "common.h"

static const char *type_from_mode(unsigned int mode) {
  switch (mode) {
    case 0100644:
    case 0100755:
    case 0120000:
      return "blob";
    case 040000:
      return "tree";
    default:
      return NULL;
  }
}

cgit_error_t parse_tree(const unsigned char *data, size_t len,
                        tree_entry_t **entries_out, size_t *count_out) {
  cgit_error_t result = CGIT_OK;
  tree_entry_t *entries = NULL;
  size_t count = 0;
  size_t i = 0;

  while (i < len) {
    /* Parse mode: read until space */
    size_t mode_start = i;
    while (i < len && data[i] != ' ') i++;

    if (i >= len) {
      fprintf(stderr, "error: invalid tree content\n");
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }

    char mode_str[CGIT_MAX_MODE_LEN];
    size_t mode_len = i - mode_start;
    if (mode_len >= sizeof(mode_str)) {
      fprintf(stderr, "error: invalid tree content\n");
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }
    memcpy(mode_str, data + mode_start, mode_len);
    mode_str[mode_len] = '\0';

    char *endptr;
    unsigned int mode = (unsigned int)strtol(mode_str, &endptr, 8);
    if (*endptr != '\0') {
      fprintf(stderr, "error: invalid mode in tree entry\n");
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }

    const char *type = type_from_mode(mode);
    if (!type) {
      fprintf(stderr, "fatal: invalid mode %o\n", mode);
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }

    i++; /* skip space */

    /* Parse name: read until null terminator */
    size_t name_start = i;
    while (i < len && data[i] != '\0') i++;

    if (i >= len) {
      fprintf(stderr, "error: invalid tree content\n");
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }

    size_t name_len = i - name_start;
    i++; /* skip null terminator */

    /* Parse raw SHA1 hash */
    if (i + CGIT_HASH_RAW_LEN > len) {
      fprintf(stderr, "error: invalid tree content\n");
      result = CGIT_ERROR_INVALID_OBJECT;
      goto cleanup;
    }

    /* Grow entries array */
    tree_entry_t *tmp = realloc(entries, (count + 1) * sizeof(tree_entry_t));
    if (!tmp) {
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }
    entries = tmp;

    /* Populate entry */
    tree_entry_t *entry = &entries[count];
    entry->mode = mode;

    entry->type = strdup(type);
    if (!entry->type) {
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }

    entry->name = malloc(name_len + 1);
    if (!entry->name) {
      free(entry->type);
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }
    memcpy(entry->name, data + name_start, name_len);
    entry->name[name_len] = '\0';

    for (size_t j = 0; j < CGIT_HASH_RAW_LEN; j++) {
      sprintf(entry->hash + 2 * j, "%02x", data[i + j]);
    }
    entry->hash[CGIT_HASH_HEX_LEN] = '\0';

    i += CGIT_HASH_RAW_LEN;
    count++;
  }

  *entries_out = entries;
  *count_out = count;
  return CGIT_OK;

cleanup:
  free_tree_entries(entries, count);
  return result;
}

void free_tree_entries(tree_entry_t *entries, size_t count) {
  for (size_t i = 0; i < count; i++) {
    free(entries[i].type);
    free(entries[i].name);
  }
  free(entries);
}

cgit_error_t object_exists(const char *hash) {
  char path[CGIT_MAX_PATH_LENGTH];
  cgit_error_t result = CGIT_OK;

  result = is_valid_hash(hash);
  if (result != CGIT_OK) return result;

  result = build_object_path(hash, path, CGIT_MAX_PATH_LENGTH);
  if (result != CGIT_OK) return result;

  if (access(path, F_OK) != 0) return CGIT_ERROR_FILE_NOT_FOUND;

  return CGIT_OK;
}

cgit_error_t read_object(const char *hash, git_object_t *obj) {
  char path[CGIT_MAX_PATH_LENGTH];
  cgit_error_t result = CGIT_OK;
  buffer_t buf = {0};
  buffer_t out_buf = {0};

  result = is_valid_hash(hash);
  if (result != CGIT_OK) {
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

  result =
      parse_object_header(out_buf.data, out_buf.size, type_buf,
                          sizeof(type_buf), &content_size, &payload_offset);
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
  buffer_t header = {0};
  buffer_t output_buf = {0};
  char path[CGIT_MAX_PATH_LENGTH];
  FILE *file = NULL;

  result = build_object_header(data, len, type, &header);
  if (result != CGIT_OK) goto cleanup;

  result = compute_sha1(header.data, header.size, hash_out);
  if (result != CGIT_OK) goto cleanup;
  if (!persist) goto cleanup;

  /* if persistance, compression, path, writing */
  result = compress_data(header.data, header.size, &output_buf);
  if (result != CGIT_OK) goto cleanup;

  result = build_object_path(hash_out, path, sizeof(path));
  if (result != CGIT_OK) goto cleanup;

  /* Skip if object already exists */
  struct stat st;
  if (stat(path, &st) == 0) goto cleanup;

  /* Create the object subdirectory (e.g. .cgit/objects/ab) */
  char dir[sizeof(CGIT_OBJECTS_DIR) + 1 + CGIT_DIR_BUF_SIZE];
  snprintf(dir, sizeof(dir), CGIT_OBJECTS_DIR "/%.2s", hash_out);

  if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "error: cannot create directory '%s': %s\n", dir,
            strerror(errno));
    result = CGIT_ERROR_IO;
    goto cleanup;
  }

  /* Write compressed object to disk */
  file = fopen(path, "wb");
  if (!file) {
    fprintf(stderr, "error: cannot open '%s': %s\n", path, strerror(errno));
    result = CGIT_ERROR_IO;
    goto cleanup;
  }

  size_t written = fwrite(output_buf.data, 1, output_buf.size, file);
  if (written != output_buf.size) {
    fprintf(stderr, "error: short write on '%s'\n", path);
    result = CGIT_ERROR_IO;
    goto cleanup;
  }

cleanup:
  if (file) fclose(file);
  buffer_free(&header);
  buffer_free(&output_buf);
  return result;
}

void free_object(git_object_t *obj) {
  free(obj->type);
  free(obj->data);
  obj->type = NULL;
  obj->data = NULL;
  obj->size = 0;
}
