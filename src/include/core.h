#ifndef CGIT_CORE_H
#define CGIT_CORE_H

#include "common.h"

typedef struct {
  unsigned int mode;
  char *type;
  char *name;
  char hash[CGIT_HASH_HEX_LEN + 1];
} tree_entry_t;

typedef struct {
  char *type;
  size_t size;
  unsigned char *data;
} git_object_t;

cgit_error_t parse_tree(const unsigned char *data, size_t len,
                        tree_entry_t **entries_out, size_t *count_out);

void free_tree_entries(tree_entry_t *entries, size_t count);

cgit_error_t parse_object_header(const unsigned char *buf, size_t buf_len,
                                 char *type, size_t type_len,
                                 size_t *content_size, size_t *payload_offset);
cgit_error_t build_object_header(const unsigned char *data, size_t file_size,
                                 const char *type, buffer_t *output);
cgit_error_t object_exists(const char *hash);
cgit_error_t read_object(const char *hash, git_object_t *obj);
cgit_error_t write_object(const unsigned char *data, size_t len,
                          const char *type, char *hash_out, int persist);
void free_object(git_object_t *obj);

cgit_error_t compress_data(const unsigned char *input, size_t input_len,
                           buffer_t *output);
cgit_error_t decompress_data(const unsigned char *input, size_t input_len,
                             buffer_t *output);

cgit_error_t compute_sha1(const unsigned char *header, size_t len,
                          char *hex_out);

cgit_error_t build_object_path(const char *hash, char *path_out,
                               size_t path_size);
cgit_error_t read_file(const char *path, buffer_t *output);
cgit_error_t is_valid_hash(const char *hash);
void buffer_free(buffer_t *buf);

#endif
