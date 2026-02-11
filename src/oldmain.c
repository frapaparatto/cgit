#include <CommonCrypto/CommonCrypto.h>
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
    fprintf(stderr, "Usage: ./.cgit <command> [<args>]\n");
    return 1;
  }

  const char *command = argv[1];

  if (strcmp(command, "cat-file") == 0) {
    if (argc < 4) {
      fprintf(stderr, "usage:.cgit cat-file (-p | -t | -s) <object>\n");
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

    // Build .cgit/objects/aa/bbbbb...
    char dir[3];
    char file[39];
    memcpy(dir, object, 2);
    dir[2] = '\0';
    memcpy(file, object + 2, 38);
    file[38] = '\0';

    char path[256];
    snprintf(path, sizeof(path), ".cgit/objects/%s/%s", dir, file);

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
          while (new_cap < acc_len + produced) new_cap *= 2;

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

      if (zret == Z_STREAM_END) break;
    }

    inflateEnd(&strm);
    free(buffer);  // compressed bytes no longer needed

    // Parse decompressed header: "<type> <size>\0<payload>"
    // Find space
    size_t i = 0;
    while (i < acc_len && acc_buff[i] != ' ') i++;
    if (i == acc_len) {
      fprintf(stderr, "error: invalid object header (no space)\n");
      free(acc_buff);
      return 1;
    }
    const char *type = (const char *)acc_buff;
    size_t type_len = i;

    // Parse decimal size until NUL
    i++;  // skip space
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
    i++;  // skip NUL
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
  }

  else if (strcmp(command, "hash-object") == 0) {
    FILE *file_content = NULL;
    unsigned char *header_buffer = NULL;
    unsigned char *acc_buff = NULL;
    FILE *compressed_file = NULL;

    if (argc < 4) {
      /* TODO: include the possibility to write the command without the -w
       * option */
      fprintf(stderr, "usage:.cgit hash-object [-w] <file>\n");
      return 1;
    }

    const char *option = argv[2];
    const char *filename = argv[3];

    if (strcmp(option, "-w") != 0) {
      fprintf(stderr, "invalid option: '%s': (-w)\n", option);
      return 1;
    }

    /* For this stage I assume the type will be blob
     *
     * TODO: include other types of data (like tree)
     * */
    char *type = "blob";

    /* Now I have to compute the size of the file */
    struct stat st;
    if (stat(filename, &st) != 0) {
      perror("stat");
      return 1;
    }

    // st.st_size is of type off_t (usually a 64-bit signed integer)
    size_t file_size = (size_t)st.st_size;

    // Calculate header length WITHOUT null terminator
    size_t header_len = snprintf(NULL, 0, "%s %zu", type, file_size);

    // Total size: header + null terminator + file content
    if (file_size > SIZE_MAX - header_len - 1) {
      fprintf(stderr, "unable to represent total size\n");
      return 1;
    }
    size_t total_size = header_len + 1 + file_size;

    file_content = fopen(filename, "rb");
    if (file_content == NULL) {
      fprintf(stderr, "error: cannot read object '%s': %s\n", filename,
              strerror(errno));
      return 1;
    }

    header_buffer = malloc(total_size);

    if (!header_buffer) {
      fprintf(stderr, "error: %s\n", strerror(errno));
      goto err;
    }

    // Write header with null terminator
    snprintf((char *)header_buffer, header_len + 1, "%s %zu", type, file_size);
    // Read file content immediately after the null terminator
    size_t byte_read =

        fread(header_buffer + header_len + 1, 1, file_size, file_content);

    if (byte_read != file_size) {
      fprintf(stderr, "error to load the file content\n");
      goto err;
    }

    /* Here I should write the sha-1 function and the compression pattern*/
    unsigned char hash[CC_SHA1_DIGEST_LENGTH];
    CC_SHA1(header_buffer, total_size, hash);

    char hex_hash[41];
    for (int j = 0; j < CC_SHA1_DIGEST_LENGTH; j++) {
      sprintf(hex_hash + 2 * j, "%02x", hash[j]);
    }
    hex_hash[40] = '\0';

    /* Here I will implement the compression */
    unsigned char tmp[32768];

    size_t acc_cap = 8192;
    size_t acc_len = 0;

    acc_buff = (unsigned char *)malloc(acc_cap);
    if (!acc_buff) {
      fprintf(stderr, "failed to allocate memory\n");
      goto err;
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));

    strm.next_in = (Bytef *)header_buffer;
    strm.avail_in = (uInt)total_size;

    int ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK) {
      fprintf(stderr, "compression error\n");
      goto err;
    }

    do {
      strm.next_out = tmp;
      strm.avail_out = sizeof(tmp);

      ret = deflate(&strm, strm.avail_in == 0 ? Z_FINISH : Z_NO_FLUSH);
      if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
        fprintf(stderr, "compression error\n");
        deflateEnd(&strm);
        goto err;
      }

      size_t produced = sizeof(tmp) - (size_t)strm.avail_out;

      if (produced > 0) {
        if (produced > acc_cap - acc_len) {
          size_t new_cap = acc_cap;

          while (produced > new_cap - acc_len) {
            if (new_cap > SIZE_MAX / 2) {
              fprintf(stderr, "new_cap is > max representable\n");
              deflateEnd(&strm);
              goto err;
            }
            new_cap *= 2;
          }

          unsigned char *tmp_acc = (unsigned char *)realloc(acc_buff, new_cap);

          if (!tmp_acc) {
            fprintf(stderr, "error: %s\n", strerror(errno));
            deflateEnd(&strm);
            goto err;
          }

          acc_buff = tmp_acc;
          acc_cap = new_cap;
        }

        memcpy(acc_buff + acc_len, tmp, produced);
        acc_len += produced;
      }
    } while (ret != Z_STREAM_END);

    deflateEnd(&strm);

    /* TODO: after decompression I should create a path, try to create file
     * and directory */

    char path[256];
    snprintf(path, sizeof(path), ".cgit/objects/%c%c", hex_hash[0],
             hex_hash[1]);

    if (mkdir(path, 0755) == -1) {
      if (errno != EEXIST) {
        fprintf(stderr, "Failed to create directories: %s\n", strerror(errno));
        goto err;
      }
    }

    char full_path[256];
    snprintf(full_path, sizeof(full_path), ".cgit/objects/%.2s/%s", hex_hash,
             hex_hash + 2);

    compressed_file = fopen(full_path, "wb");
    if (!compressed_file) {
      fprintf(stderr, "failed to open the file\n");
      goto err;
    }

    size_t bytes_write = fwrite(acc_buff, 1, acc_len, compressed_file);
    if (bytes_write != acc_len) {
      fprintf(stderr, "failed to write file\n");
      goto err;
    }

    fwrite(hex_hash, 1, strlen(hex_hash), stdout);
    goto cleanup;

  err:
    if (header_buffer) free(header_buffer);
    if (file_content) fclose(file_content);
    if (acc_buff) free(acc_buff);
    if (compressed_file) fclose(compressed_file);
    return 1;

  cleanup:
    free(header_buffer);
    fclose(file_content);
    free(acc_buff);
    fclose(compressed_file);
    return 0;
  }

  else {
    fprintf(stderr, "Unknown command %s\n", command);
    return 1;
  }

  return 0;
}
