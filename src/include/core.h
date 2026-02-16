#ifndef CGIT_CORE_H
#define CGIT_CORE_H

#include "common.h"

typedef struct {
  char *type;
  size_t size;
  unsigned char *data;
} git_object_t;

cgit_error_t parse_object_header(const unsigned char *buf, size_t buf_len,
                                 char *type, size_t type_len,
                                 size_t *content_size, size_t *payload_offset);

cgit_error_t read_object(const char *hash, git_object_t *obj);
/* TODO: evaluate what is the cleanest way to write the persist parameter in
 * terms of type used (bool, int) or maybe creating an enum... */
cgit_error_t write_object(const unsigned char *data, size_t len,
                          const char *type, char *hash_out, int persist);
void free_object(git_object_t *obj);

cgit_error_t compress_data(const unsigned char *input, size_t input_len,
                           buffer_t *output);
cgit_error_t decompress_data(const unsigned char *input, size_t input_len,
                             buffer_t *output);

cgit_error_t compute_sha1(const unsigned char *data, size_t len, char *hex_out);

cgit_error_t build_object_path(const char *hash, char *path_out,
                               size_t path_size);
cgit_error_t read_file(const char *path, buffer_t *output);
int is_valid_hash(const char *hash);
void buffer_free(buffer_t *buf);

#endif
