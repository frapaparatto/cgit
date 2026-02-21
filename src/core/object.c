#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../include/common.h"
#include "../include/core.h"

static cgit_error_t buffer_append_fmt(buffer_t *buf, const char *fmt, ...) {
  cgit_error_t result = CGIT_OK;

  if (!buf->data) {
    buf->data = malloc(CGIT_READ_BUFFER_SIZE);
    if (!buf->data) {
      fprintf(stderr,
              "error: buffer_append_fmt: failed to allocate output buffer\n");
      return CGIT_ERROR_MEMORY;
    }
    buf->capacity = CGIT_READ_BUFFER_SIZE;
    buf->size = 0;
  }

  va_list args;
  va_start(args, fmt);
  size_t len_line = vsnprintf(NULL, 0, fmt, args);
  va_end(args);

  if (len_line > buf->capacity - buf->size) {
    size_t new_cap = buf->capacity;

    while (len_line > new_cap - buf->size) {
      if (new_cap > SIZE_MAX / 2) {
        fprintf(stderr, "out of memory");
        result = CGIT_ERROR_MEMORY;
        goto cleanup;
      }
      new_cap *= 2;
    }

    unsigned char *tmp_buf = realloc(buf->data, new_cap);
    if (!tmp_buf) {
      fprintf(stderr,
              "error: serialize_tree: realloc failed growing buffer to %zu "
              "bytes\n",
              new_cap);
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }

    buf->data = tmp_buf;
    buf->capacity = new_cap;
  }
  va_start(args, fmt);
  vsnprintf((char *)buf->data + buf->size, len_line + 1, fmt, args);
  va_end(args);

  buf->size += len_line;

cleanup:
  return result;
}

cgit_error_t build_commit_content(const char *tree_hash,
                                  const char *parent_hash, const char *author,
                                  const char *email, const char *message,
                                  buffer_t *output) {
  cgit_error_t result = CGIT_OK;
  time_t timestamp;
  time(&timestamp);
  struct tm lt;
  localtime_r(&timestamp, &lt);

  int offset_seconds = lt.tm_gmtoff;
  char sign = offset_seconds >= 0 ? '+' : '-';
  int abs_offset = abs(offset_seconds);
  int hours = abs_offset / 3600;
  int minutes = (abs_offset % 3600) / 60;

  result = buffer_append_fmt(output, "tree %s\n", tree_hash);
  if (result != CGIT_OK) goto cleanup;

  if (parent_hash) {
    result = buffer_append_fmt(output, "parent %s\n", parent_hash);
    if (result != CGIT_OK) goto cleanup;
  }

  result = buffer_append_fmt(output, "author %s <%s> %ld %c%02d%02d\n",
                             author, email, (long)timestamp, sign, hours,
                             minutes);
  if (result != CGIT_OK) goto cleanup;

  result = buffer_append_fmt(output, "committer %s <%s> %ld %c%02d%02d\n",
                             author, email, (long)timestamp, sign, hours,
                             minutes);
  if (result != CGIT_OK) goto cleanup;

  result = buffer_append_fmt(output, "\n%s\n", message);
  if (result != CGIT_OK) goto cleanup;

  return result;

cleanup:
  buffer_free(output);
  return result;
}

static int cmp_entry(const void *a, const void *b) {
  return strcmp(((tree_entry_t *)a)->name, ((tree_entry_t *)b)->name);
}

cgit_error_t serialize_tree(tree_entry_t *entries, size_t count,
                            buffer_t *out) {
  cgit_error_t result = CGIT_OK;
  qsort(entries, count, sizeof(tree_entry_t), cmp_entry);

  out->data = malloc(CGIT_READ_BUFFER_SIZE);
  if (!out->data) {
    fprintf(stderr,
            "error: serialize_tree: failed to allocate output buffer\n");
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }
  out->capacity = CGIT_READ_BUFFER_SIZE;
  out->size = 0;

  for (size_t i = 0; i < count; i++) {
    char tmp[CGIT_READ_BUFFER_SIZE] = {0};
    char entry_byte_hash[CGIT_HASH_RAW_LEN];

    result = hex_to_bytes_hash((const unsigned char *)entries[i].hash,
                               entry_byte_hash);
    if (result != CGIT_OK) goto cleanup;

    size_t len =
        snprintf(tmp, sizeof(tmp), "%u %s", entries[i].mode, entries[i].name);

    memcpy(tmp + len + 1, entry_byte_hash, CGIT_HASH_RAW_LEN);

    size_t total_len = len + 1 + CGIT_HASH_RAW_LEN;

    if (out->size > SIZE_MAX - total_len) {
      fprintf(stderr, "error: serialize_tree: size overflow on entry '%s'\n",
              entries[i].name);
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }

    if (total_len > out->capacity - out->size) {
      size_t new_cap = out->capacity;

      while (total_len > new_cap - out->size) {
        if (new_cap > SIZE_MAX / 2) {
          fprintf(stderr,
                  "error: serialize_tree: buffer too large to grow for entry "
                  "'%s'\n",
                  entries[i].name);
          result = CGIT_ERROR_MEMORY;
          goto cleanup;
        }
        new_cap *= 2;
      }

      unsigned char *tmp_buf = realloc(out->data, new_cap);
      if (!tmp_buf) {
        fprintf(stderr,
                "error: serialize_tree: realloc failed growing buffer to %zu "
                "bytes\n",
                new_cap);
        result = CGIT_ERROR_MEMORY;
        goto cleanup;
      }

      out->data = tmp_buf;
      out->capacity = new_cap;
    }

    memcpy(out->data + out->size, tmp, total_len);
    out->size += total_len;
  }

  return result;

cleanup:
  buffer_free(out);
  return result;
}

cgit_error_t write_tree_recursive(const char *path, tree_entry_t **entries_out,
                                  size_t *count_out) {
  cgit_error_t result = CGIT_OK;
  tree_entry_t *entries = NULL;
  size_t count = 0;
  int persist = 1;
  buffer_t buf = {0};
  tree_entry_t *sub_entries = NULL;
  size_t sub_count = 0;

  DIR *dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "failed to open directory\n");
    result = CGIT_ERROR_FILE_NOT_FOUND;
    goto cleanup;
  }

  struct dirent *dir_entry;
  while ((dir_entry = readdir(dir)) != NULL) {
    if (strcmp(dir_entry->d_name, ".cgit") == 0 ||
        strcmp(dir_entry->d_name, ".") == 0 ||
        strcmp(dir_entry->d_name, "..") == 0)
      continue;

    char sub_path[CGIT_MAX_PATH_LENGTH];
    snprintf(sub_path, sizeof(sub_path), "%s/%s", path, dir_entry->d_name);

    struct stat st;
    if (stat(sub_path, &st)) goto cleanup;
    unsigned int mode;
    char *type;

    switch (st.st_mode & S_IFMT) {
      case S_IFDIR:
        mode = 40000;
        type = "tree";
        break;
      case S_IFREG:
        mode = (st.st_mode & S_IXUSR) ? 100755 : 100644;
        type = "blob";
        break;
      case S_IFLNK:
        type = "blob";
        mode = 120000;
        break;
      default:
        fprintf(stderr, "invalid mode\n");
        result = CGIT_ERROR_INVALID_OBJECT;
        goto cleanup;
    }

    tree_entry_t *tmp = realloc(entries, (count + 1) * sizeof(tree_entry_t));
    if (!tmp) {
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }
    entries = tmp;
    tree_entry_t *entry = &entries[count];

    entry->type = strdup(type);
    if (!entry->type) {
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }
    entry->mode = mode;
    entry->name = malloc(strlen(dir_entry->d_name) + 1);

    if (!entry->name) {
      free(entry->type);
      result = CGIT_ERROR_MEMORY;
      goto cleanup;
    }

    memcpy(entry->name, dir_entry->d_name, strlen(dir_entry->d_name));
    entry->name[strlen(dir_entry->d_name)] = '\0';

    if (strcmp(entry->type, "blob") == 0) {
      result = read_file(sub_path, &buf);
      if (result != CGIT_OK) {
        fprintf(stderr, "Failed to read file '%s'\n", sub_path);
        goto cleanup;
      }

      result = write_object(buf.data, buf.size, type, entry->hash, persist);
      if (result != CGIT_OK) {
        fprintf(stderr, "Failed to create the object\n");
        goto cleanup;
      }
    } else if (strcmp(entry->type, "tree") == 0) {
      result = write_tree_recursive(sub_path, &sub_entries, &sub_count);
      if (result != CGIT_OK) goto cleanup;

      result = serialize_tree(sub_entries, sub_count, &buf);
      if (result != CGIT_OK) goto cleanup;

      result = write_object(buf.data, buf.size, "tree", entry->hash, persist);
      if (result != CGIT_OK) goto cleanup;

      free_tree_entries(sub_entries, sub_count);
      sub_entries = NULL;
      sub_count = 0;
    }

    buffer_free(&buf);
    count++;
  }

  *entries_out = entries;
  *count_out = count;

  return result;

cleanup:
  buffer_free(&buf);
  free_tree_entries(sub_entries, sub_count);
  free_tree_entries(entries, count);
  if (dir) closedir(dir);
  return result;
}

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
