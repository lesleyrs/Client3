#include "rsa.h"

#if !defined(WITH_RSA_BIGINT) && !defined(WITH_RSA_OPENSSL) && defined(WITH_RSA_LIBTOM)
/* from RSC Sundae. Public domain. */


int rsa_init(struct rsa *rsa, const char *exponent, const char *modulus) {
    if (mp_read_radix(&rsa->exponent, (char *)exponent, 16) != MP_OKAY) {
        return -1;
    }
    if (mp_read_radix(&rsa->modulus, (char *)modulus, 16) != MP_OKAY) {
        mp_clear(&rsa->exponent);
        return -1;
    }

    return 0;
}

int rsa_crypt(struct rsa *rsa, void *buffer, size_t len,
              void *out, size_t outlen) {
    mp_int encrypted = {0};
    mp_int result = {0};
    uint8_t *inbuf = buffer;
    uint8_t *outbuf = out;

    if (mp_from_ubin(&encrypted, inbuf, len) != MP_OKAY) {
        goto error;
    }

    if (mp_init(&result) != MP_OKAY) {
        goto error;
    }

    if (mp_exptmod(&encrypted,
                   &rsa->exponent, &rsa->modulus, &result) != MP_OKAY) {
        goto error;
    }

    mp_clear(&encrypted);

    if (mp_to_ubin(&result, outbuf, outlen, &outlen) != MP_OKAY) {
        goto error;
    }

    mp_clear(&result);
    return (int)outlen;
error:
    mp_clear(&encrypted);
    mp_clear(&result);
    return -1;
}
#endif
