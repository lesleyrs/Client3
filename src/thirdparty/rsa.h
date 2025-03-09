#ifndef RSA_H
#define RSA_H

/* from RSC Sundae. Public domain. */

#include <stdio.h>

#ifdef WITH_RSA_BIGINT
struct rsa {
    const char *exponent;
    const char *modulus;
};
#elif defined(WITH_RSA_OPENSSL)
#include <openssl/bn.h>

struct rsa {
    BN_CTX *ctx;
    BIGNUM *exponent;
    BIGNUM *modulus;
};
#elif defined(WITH_RSA_LIBTOM)
#include "tommath.h"

struct rsa {
	mp_int exponent;
	mp_int modulus;
};
#else
#include "bn.h"

struct rsa {
    struct bn exponent;
    struct bn modulus;
};
#endif

int rsa_init(struct rsa *rsa, const char *exponent, const char *modulus);
int rsa_crypt(struct rsa *rsa, void *buffer, size_t len, void *out, size_t outlen);
#endif
