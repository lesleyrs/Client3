#include <stdint.h>

#include "rsa.h"

#ifdef WITH_RSA_BIGINT
#include "emscripten.h"

EM_JS(int, rsa_crypt_js, (const char* exp, const char* mod, void *temp, int length, void *enc), {
    const bigRaw = bytesToBigInt(HEAPU8.subarray(temp, temp + length));
    const bigEnc = bigIntModPow(bigRaw, BigInt(UTF8ToString(exp)), BigInt(UTF8ToString(mod)));
    const rawEnc = bigIntToBytes(bigEnc);

    HEAPU8.set(rawEnc, enc);
    return rawEnc.length;
})

int rsa_init(struct rsa *rsa, const char *exponent, const char *modulus) {
    rsa->exponent = exponent;
    rsa->modulus = modulus;
    return 0;
}

int rsa_crypt(struct rsa *rsa, void *buffer, size_t len, void *out, size_t outlen) {
    (void)outlen;
    return rsa_crypt_js(rsa->exponent, rsa->modulus, buffer, len, out);
}
#endif
