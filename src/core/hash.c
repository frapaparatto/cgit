/*
 * SHA-1 hashing using openssl/sha.h.
 *
 * SHA1() is deprecated in OpenSSL 3.x in favor of the EVP API
 * (EVP_DigestInit_ex / EVP_DigestUpdate / EVP_DigestFinal_ex).
 *
 * I use SHA1() here for simplicity — if OpenSSL ever removes it,
 * migrate to the EVP interface.
 */

#include <openssl/sha.h>
#include <stdio.h>

#include "../include/common.h"
#include "../include/core.h"

cgit_error_t compute_sha1(const unsigned char *header, size_t len,
                          char *hex_out) {
  unsigned char hash[SHA_DIGEST_LENGTH];

  if (!SHA1(header, len, hash)) {
    return CGIT_ERROR_HASH;
  }

  for (size_t i = 0; i < SHA_DIGEST_LENGTH; i++) {
    sprintf(hex_out + 2 * i, "%02x", hash[i]);
  }

  hex_out[CGIT_HASH_HEX_LEN] = '\0';

  return CGIT_OK;
}

cgit_error_t hex_to_bytes_hash(const unsigned char *hex_hash,
                               unsigned char *hash_out) {
  for (size_t j = 0; j < CGIT_HASH_RAW_LEN; j++) {
    unsigned int byte;
    sscanf((const char *)&hex_hash[j * 2], "%02x", &byte);
    hash_out[j] = (unsigned char)byte;
  }
  return CGIT_OK;
}
