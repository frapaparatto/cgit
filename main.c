#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

int main(int argc, char *argv[]) {
  // Disable output buffering
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  if (argc < 2) {
    fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
    return 1;
  }

  const char *command = argv[1];

  if (strcmp(command, "init") == 0) {
    if (mkdir(".git", 0755) == -1 || mkdir(".git/objects", 0755) == -1 ||
        mkdir(".git/refs", 0755) == -1) {
      fprintf(stderr, "Failed to create directories: %s\n", strerror(errno));
      return 1;
    }

    FILE *headFile = fopen(".git/HEAD", "w");
    if (headFile == NULL) {
      fprintf(stderr, "Failed to create .git/HEAD file: %s\n", strerror(errno));
      return 1;
    }
    fprintf(headFile, "ref: refs/heads/main\n");
    fclose(headFile);

    printf("Initialized git directory\n");
    return 0;
  } 

  else if (strcmp(command, "cat-file") == 0) {
    if (argc < 4) {
      fprintf(stderr, "usage: git cat-file (-p | -t | -s) <object>\n");
      return 1;
    }

    const char *option = argv[2];
    const char *object = argv[3];

    if (strcmp(option, "-t") != 0 && strcmp(option, "-p") != 0 &&
        strcmp(option, "-s") != 0) {
      fprintf(stderr, "invalid option '%s': (-p | -t | -s)\n", option);
      return 1;
    }

    // Validate object id (40 hex chars)
    size_t objlen = strlen(object);
    if (objlen != 40) {
      fprintf(stderr,
              "error: invalid object name '%s': expected 40 hexadecimal "
              "characters\n",
              object);
      return 1;
    }
    for (size_t i = 0; i < 40; i++) {
      if (!isxdigit((unsigned char)object[i])) {
        fprintf(stderr,
                "error: invalid object name '%s': non-hexadecimal character\n",
                object);
        return 1;
      }
    }

    // Build .git/objects/aa/bbbbb...
    char dir[3];
    char file[39];
    memcpy(dir, object, 2);
    dir[2] = '\0';
    memcpy(file, object + 2, 38);
    file[38] = '\0';

    char path[256];
    snprintf(path, sizeof(path), ".git/objects/%s/%s", dir, file);

    // Read compressed object file into memory
    FILE *objectFile = fopen(path, "rb");
    if (objectFile == NULL) {
      fprintf(stderr, "error: cannot read object '%s': %s\n", object,
              strerror(errno));
      return 1;
    }

    if (fseek(objectFile, 0, SEEK_END) != 0) {
      fprintf(stderr, "error: fseek failed: %s\n", strerror(errno));
      fclose(objectFile);
      return 1;
    }

    long endpos = ftell(objectFile);
    if (endpos < 0) {
      fprintf(stderr, "error: ftell failed: %s\n", strerror(errno));
      fclose(objectFile);
      return 1;
    }
    rewind(objectFile);

    size_t fileLen = (size_t)endpos;
    unsigned char *buffer = (unsigned char *)malloc(fileLen);
    if (!buffer) {
      fprintf(stderr, "out of memory\n");
      fclose(objectFile);
      return 1;
    }

    size_t bytes_read = fread(buffer, 1, fileLen, objectFile);
    fclose(objectFile);

    if (bytes_read != fileLen) {
      fprintf(stderr, "error: Failed to read file: %s\n", strerror(errno));
      free(buffer);
      return 1;
    }

    // zlib uses uInt for avail_in (fine for loose objects; guard anyway)
    if (fileLen > (size_t)UINT_MAX) {
      fprintf(stderr, "error: object too large\n");
      free(buffer);
      return 1;
    }

    // Inflate (decompress) into an accumulator buffer
    unsigned char tmp[32768];

    size_t acc_cap = 8192;
    size_t acc_len = 0;
    unsigned char *acc_buff = (unsigned char *)malloc(acc_cap);
    if (!acc_buff) {
      fprintf(stderr, "out of memory\n");
      free(buffer);
      return 1;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    strm.next_in = (Bytef *)buffer;
    strm.avail_in = (uInt)fileLen;

    int zret = inflateInit(&strm);
    if (zret != Z_OK) {
      fprintf(stderr, "error: inflateInit failed\n");
      free(acc_buff);
      free(buffer);
      return 1;
    }

    for (;;) {
      // Reset output window every iteration
      strm.next_out = (Bytef *)tmp;
      strm.avail_out = (uInt)sizeof(tmp);

      zret = inflate(&strm, Z_NO_FLUSH);

      if (zret != Z_OK && zret != Z_STREAM_END) {
        fprintf(stderr, "error: inflate failed (corrupt object?)\n");
        inflateEnd(&strm);
        free(acc_buff);
        free(buffer);
        return 1;
      }

      size_t produced = sizeof(tmp) - (size_t)strm.avail_out;
      if (produced > 0) {
        if (acc_len + produced > acc_cap) {
          size_t new_cap = acc_cap;
          while (new_cap < acc_len + produced)
            new_cap *= 2;

          unsigned char *p = (unsigned char *)realloc(acc_buff, new_cap);
          if (!p) {
            fprintf(stderr, "out of memory\n");
            inflateEnd(&strm);
            free(acc_buff);
            free(buffer);
            return 1;
          }
          acc_buff = p;
          acc_cap = new_cap;
        }

        memcpy(acc_buff + acc_len, tmp, produced);
        acc_len += produced;
      }

      if (zret == Z_STREAM_END)
        break;
    }

    inflateEnd(&strm);
    free(buffer); // compressed bytes no longer needed

    // Parse decompressed header: "<type> <size>\0<payload>"
    // Find space
    size_t i = 0;
    while (i < acc_len && acc_buff[i] != ' ')
      i++;
    if (i == acc_len) {
      fprintf(stderr, "error: invalid object header (no space)\n");
      free(acc_buff);
      return 1;
    }
    const char *type = (const char *)acc_buff;
    size_t type_len = i;

    // Parse decimal size until NUL
    i++; // skip space
    if (i >= acc_len) {
      fprintf(stderr, "error: invalid object header\n");
      free(acc_buff);
      return 1;
    }

    size_t size_val = 0;
    int saw_digit = 0;
    while (i < acc_len && acc_buff[i] != '\0') {
      unsigned char c = acc_buff[i];
      if (c < '0' || c > '9') {
        fprintf(stderr, "error: invalid object header (bad size)\n");
        free(acc_buff);
        return 1;
      }
      saw_digit = 1;
      size_t digit = (size_t)(c - '0');
      if (size_val > (SIZE_MAX - digit) / 10) {
        fprintf(stderr, "error: object size overflow\n");
        free(acc_buff);
        return 1;
      }
      size_val = size_val * 10 + digit;
      i++;
    }
    if (!saw_digit || i == acc_len) {
      fprintf(stderr, "error: invalid object header (no NUL)\n");
      free(acc_buff);
      return 1;
    }

    // Payload begins after NUL
    i++; // skip NUL
    if (i > acc_len) {
      fprintf(stderr, "error: invalid object header\n");
      free(acc_buff);
      return 1;
    }

    const unsigned char *payload = acc_buff + i;
    size_t payload_len = acc_len - i;

    // Optional strict check: payload_len must equal declared size
    if (payload_len != size_val) {
      fprintf(stderr, "error: invalid object (size mismatch)\n");
      free(acc_buff);
      return 1;
    }

    if (strcmp(option, "-t") == 0) {
      fwrite(type, 1, type_len, stdout);
      fputc('\n', stdout);

    } else if (strcmp(option, "-s") == 0) {
      printf("%zu\n", size_val);

    } else if (strcmp(option, "-p") == 0) {
      fwrite(payload, 1, payload_len, stdout);
    }

    free(acc_buff);
    return 0;
  } else {
    fprintf(stderr, "Unknown command %s\n", command);
    return 1;
  }

  return 0;
}
