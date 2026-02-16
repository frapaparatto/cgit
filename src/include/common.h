#ifndef CGIT_COMMON_H
#define CGIT_COMMON_H

#include <stddef.h>

typedef enum {
  CGIT_OK = 0,
  CGIT_ERROR_INVALID_ARGS,
  CGIT_ERROR_FILE_NOT_FOUND,
  CGIT_ERROR_MEMORY,
  CGIT_ERROR_INVALID_OBJECT,
  CGIT_ERROR_IO,
  CGIT_ERROR_COMPRESSION,
} cgit_error_t;

#define CGIT_DIR ".cgit"
#define CGIT_OBJECTS_DIR CGIT_DIR "/objects"
#define CGIT_REFS_DIR CGIT_DIR "/refs"
#define CGIT_HEAD_FILE CGIT_DIR "/HEAD"

#define CGIT_HASH_HEX_LEN 40
#define CGIT_COMPRESSION_BUFFER_SIZE 32768
#define CGIT_READ_BUFFER_SIZE 8192
#define CGIT_MAX_PATH_LENGTH 256
#define CGIT_DIR_BUF_SIZE (2 + 1)
#define CGIT_OBJ_NAME_BUF_SIZE (CGIT_HASH_HEX_LEN - 2 + 1)
#define CGIT_DEFAULT_OBJ_TYPE "blob"
#define CGIT_MAX_TYPE_LEN 16

typedef struct {
  unsigned char *data;
  size_t size;
  size_t capacity;
} buffer_t;

#endif
