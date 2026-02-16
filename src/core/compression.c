#include <stddef.h>

#include "../include/common.h"

cgit_error_t compress_data(const unsigned char *input, size_t input_len,
                           buffer_t *output);

cgit_error_t decompress_data(const unsigned char *input, size_t input_len,
                             buffer_t *output);
