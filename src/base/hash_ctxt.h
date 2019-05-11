#ifndef BASE_HASH_CTXT_H
#define BASE_HASH_CTXT_H

#include "hash.h"
#include <stdint.h>

#if defined(CONF_OPENSSL)
#include <openssl/md5.h>
#include <openssl/sha.h>
#else

#include <engine/external/md5/md5.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONF_OPENSSL)
// SHA256_CTX is defined in <openssl/sha.h>
#else
typedef md5_state_t MD5_CTX;
#endif


void md5_init(MD5_CTX *ctxt);
void md5_update(MD5_CTX *ctxt, const void *data, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif // BASE_HASH_CTXT_H
