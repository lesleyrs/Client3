#include <stdint.h>

#include "rsa.h"

#ifdef WITH_RSA_BIGINT
#include "emscripten.h"

// NOTE: this code can no longer works when using closure compiler, could be used for clang wasm32 still
// <script type="text/javascript">
//     function bytesToBigInt(bytes) {
//         let result = 0n;
//         for (let index = 0; index < bytes.length; index++) {
//             result = (result << 8n) | BigInt(bytes[index]);
//         }
//         return result;
//     }

//     function bigIntToBytes(bigInt) {
//         const bytes = [];
//         while (bigInt > 0n) {
//             bytes.unshift(Number(bigInt & 0xffn));
//             bigInt >>= 8n;
//         }

//         if (bytes[0] & 0x80) {
//             bytes.unshift(0);
//         }

//         return new Uint8Array(bytes);
//     }

//     function bigIntModPow(base, exponent, modulus) {
//         let result = 1n;
//         while (exponent > 0n) {
//             if (exponent % 2n === 1n) {
//                 result = (result * base) % modulus;
//             }
//             base = (base * base) % modulus;
//             exponent >>= 1n;
//         }
//         return result;
//     }
// </script>

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
