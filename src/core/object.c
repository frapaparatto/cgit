#include "../include/core.h"

cgit_error_t read_object(const char *hash, git_object_t *obj);
cgit_error_t write_object(const unsigned char *data, size_t len,
                          const char *type, char *hash_out, int persist);
void free_object(git_object_t *obj);
