#include "rsa.h"

#ifdef WITH_RSA_BIGINT

#include "js/wasm.h"
WASM_IMPORT(int, rsaCrypt, (const char* exp, const char* mod, void *temp, int length, void *enc));

int rsa_init(struct rsa *rsa, const char *exponent, const char *modulus) {
    rsa->exponent = exponent;
    rsa->modulus = modulus;
    return 0;
}

int rsa_crypt(struct rsa *rsa, void *buffer, size_t len, void *out, size_t outlen) {
    (void)outlen;
    return rsaCrypt(rsa->exponent, rsa->modulus, buffer, len, out);
}
#endif
