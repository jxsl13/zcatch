#ifndef BASE_HASH_H
#define BASE_HASH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	MD5_DIGEST_LENGTH=128/8,
	MD5_MAXSTRSIZE=2*MD5_DIGEST_LENGTH+1,
};

typedef struct
{
	unsigned char data[MD5_DIGEST_LENGTH];
} MD5_DIGEST;

void md5_str(MD5_DIGEST digest, char *str, size_t max_len);
int md5_from_str(MD5_DIGEST *out, const char *str);
int md5_comp(MD5_DIGEST digest1, MD5_DIGEST digest2);

static const MD5_DIGEST MD5_ZEROED = {{0}};

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
inline bool operator==(const MD5_DIGEST &that, const MD5_DIGEST &other)
{
	return md5_comp(that, other) == 0;
}
inline bool operator!=(const MD5_DIGEST &that, const MD5_DIGEST &other)
{
	return !(that == other);
}
#endif

#endif // BASE_HASH_H
