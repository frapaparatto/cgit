#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "../include/common.h"
#include "../include/core.h"

cgit_error_t decompress_data(const unsigned char *input, size_t input_len,
                             buffer_t *output) {
  cgit_error_t result = CGIT_OK;
  int strm_initialized = 0;
  unsigned char tmp[CGIT_COMPRESSION_BUFFER_SIZE];
  output->capacity = CGIT_READ_BUFFER_SIZE;
  output->data = malloc(output->capacity);

  if (!output->data) {
    fprintf(stderr, "out of memory\n");
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));
  strm.next_in = (Bytef *)input;
  strm.avail_in = (uInt)input_len;

  int zret = inflateInit(&strm);
  if (zret != Z_OK) {
    fprintf(stderr, "error: inflateInit failed\n");
    result = CGIT_ERROR_COMPRESSION;
    goto cleanup;
  }
  strm_initialized = 1;

  for (;;) {
    // Reset output window every iteration
    strm.next_out = (Bytef *)tmp;
    strm.avail_out = (uInt)sizeof(tmp);

    zret = inflate(&strm, Z_NO_FLUSH);

    if (zret != Z_OK && zret != Z_STREAM_END) {
      fprintf(stderr, "error: inflate failed (corrupt object?)\n");
      result = CGIT_ERROR_COMPRESSION;
      goto cleanup;
    }

    size_t produced = sizeof(tmp) - (size_t)strm.avail_out;
    if (produced > 0) {
      if (output->size + produced > output->capacity) {
        size_t new_cap = output->capacity;
        while (new_cap < output->size + produced) new_cap *= 2;

        unsigned char *p = (unsigned char *)realloc(output->data, new_cap);
        if (!p) {
          fprintf(stderr, "out of memory\n");
          result = CGIT_ERROR_MEMORY;
          goto cleanup;
        }
        output->data = p;
        output->capacity = new_cap;
      }

      memcpy(output->data + output->size, tmp, produced);
      output->size += produced;
    }

    if (zret == Z_STREAM_END) break;
  }

cleanup:
  if (strm_initialized) inflateEnd(&strm);
  return result;
}

cgit_error_t compress_data(const unsigned char *input, size_t input_len,
                           buffer_t *output) {
  cgit_error_t result = CGIT_OK;
  int strm_initialized = 0;
  unsigned char tmp[CGIT_COMPRESSION_BUFFER_SIZE];
  output->capacity = CGIT_READ_BUFFER_SIZE;
  output->data = malloc(output->capacity);

  if (!output->data) {
    fprintf(stderr, "out of memory\n");
    result = CGIT_ERROR_MEMORY;
    goto cleanup;
  }

  z_stream strm;
  memset(&strm, 0, sizeof(strm));

  strm.next_in = (Bytef *)input;
  strm.avail_in = (uInt)input_len;

  int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) {
    fprintf(stderr, "compression error\n");
    result = CGIT_ERROR_COMPRESSION;
    goto cleanup;
  }
  strm_initialized = 1;

  do {
    strm.next_out = tmp;
    strm.avail_out = sizeof(tmp);

    ret = deflate(&strm, strm.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
      fprintf(stderr, "compression error\n");
      result = CGIT_ERROR_COMPRESSION;
      goto cleanup;
    }

    size_t produced = sizeof(tmp) - (size_t)strm.avail_out;

    if (produced > 0) {
      if (produced > output->capacity - output->size) {
        size_t new_cap = output->capacity;

        while (produced > new_cap - output->size) {
          if (new_cap > SIZE_MAX / 2) {
            fprintf(stderr, "new_cap is > max representable\n");
            result = CGIT_ERROR_MEMORY;
            goto cleanup;
          }
          new_cap *= 2;
        }

        unsigned char *tmp_acc =
            (unsigned char *)realloc(output->data, new_cap);

        if (!tmp_acc) {
          fprintf(stderr, "error: %s\n", strerror(errno));
          result = CGIT_ERROR_MEMORY;
          goto cleanup;
        }

        output->data = tmp_acc;
        output->capacity = new_cap;
      }

      memcpy(output->data + output->size, tmp, produced);
      output->size += produced;
    }
  } while (ret != Z_STREAM_END);

cleanup:
  if (strm_initialized) deflateEnd(&strm);
  return result;
}
