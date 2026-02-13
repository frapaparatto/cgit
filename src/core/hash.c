#include <CommonCrypto/CommonCrypto.h>

#include "../include/core.h"

cgit_error_t compute_sha1(const unsigned char *data, size_t len,
                          char *hex_out) {
  /*
   *
   * This function should calculate the sha1 value
   *
   * First let's recap the process to calculate the sha_1 value
   *
   * Level: 1 (core) so basically I can't use functions of levels above, I can
   * use only fuction within the same level
   *
   * In this case to compute the sha1 I need the header <type> <size>\0<content>
   *
   * I imagine *hex_out to be a pointer to the hexadecimal buffer where I will
   * store the output
   *
   * This hexadecimal value (computation) is needed only for the cat-file
   * command so basically I think that I should initialize it in the
   * handle_hash_object function
   *
   * write object function should create the header
   *
   * arguments
   * - data = header + data
   * - len = full_len (total_size)
   * - hex_out = the hex_out buffer
   *
   */

  return 0;
}
