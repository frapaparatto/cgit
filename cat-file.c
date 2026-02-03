#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zconf.h>
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
  } else if (strcmp(command, "cat-file") == 0) {
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
      strm.next_out = (Bytef *)tmp;
      strm.avail_out = (uInt)sizeof(tmp);

      size_t produced = sizeof(tmp) - (size_t)strm.avail_out;
      if (produced > 0) {

      }
    }

  } else {
    fprintf(stderr, "Unknown command %s\n", command);
    return 1;
  }

  return 0;
}
