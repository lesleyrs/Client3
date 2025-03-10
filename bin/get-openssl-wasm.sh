#!/bin/sh
cd "$(dirname "$0")"

# binaries are already provided so this isn't required to run, it's just here to share steps to reproduce them
# thx to: https://github.com/cryptool-org/openssl-webterm/blob/master/emscr/builds/openssl/build.sh
git clone https://github.com/openssl/openssl.git && cd openssl || exit 1
emconfigure ./Configure no-shared no-asm no-threads no-ssl3 no-dtls no-engine no-dso linux-x32 -static
emmake make CROSS_COMPILE= -j$(nproc) build_libs
