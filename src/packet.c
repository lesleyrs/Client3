#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "datastruct/doublylinkable.h"
#include "packet.h"
#include "platform.h"
#include "thirdparty/isaac.h"
#include "thirdparty/rsa.h"
#include "defines.h"

static int crctable[256];
static const int BITMASK[] = {
    0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff,
    0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff, 0xffff,
    0x1ffff, 0x3ffff, 0x7ffff, 0xfffff, 0x1fffff, 0x3fffff, 0x7fffff, 0xffffff,
    0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

PacketData _Packet = {0};

void packet_free_global(void) {
    linklist_free(_Packet.cacheMin);
    linklist_free(_Packet.cacheMid);
    linklist_free(_Packet.cacheMax);
}

void packet_init_global(void) {
    _Packet.cacheMin = linklist_new();
    _Packet.cacheMid = linklist_new();
    _Packet.cacheMax = linklist_new();

    const int CRC32_POLYNOMIAL = 0xedb88320;
    for (int i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if ((crc & 1) == 1) {
                crc = crc >> 1 ^ CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        crctable[i] = crc;
    }
}

Packet *packet_new(int8_t *src, int length) {
    Packet *packet = calloc(1, sizeof(Packet));
    packet->link = (DoublyLinkable){(Linkable){0}, NULL, NULL};
    packet->data = src;
    packet->length = length;
    packet->pos = 0;
    packet->bit_pos = 0;
    return packet;
}

void packet_free(Packet *packet) {
    free(packet->data);
    free(packet);
}

int rs_crc32(const int8_t *data, size_t length) {
    int crc = -1;
    for (size_t i = 0; i < length; i++) {
        crc = ((uint32_t)crc >> 8) ^ crctable[(crc ^ data[i]) & 0xff];
    }
    return ~crc;
}

Packet *packet_alloc(int type) {
    // synchronized (cacheMid) {
    Packet *cached = NULL;
    if (type == 0 && _Packet.cacheMinCount > 0) {
        _Packet.cacheMinCount--;
        cached = (Packet *)linklist_remove_head(_Packet.cacheMin);
    } else if (type == 1 && _Packet.cacheMidCount > 0) {
        _Packet.cacheMidCount--;
        cached = (Packet *)linklist_remove_head(_Packet.cacheMid);
    } else if (type == 2 && _Packet.cacheMaxCount > 0) {
        _Packet.cacheMaxCount--;
        cached = (Packet *)linklist_remove_head(_Packet.cacheMax);
    }

    if (cached) {
        cached->pos = 0;
        return cached;
    }
    // }

    int length;
    if (type == 0) {
        length = 1000;
    } else if (type == 1) {
        length = 5000;
    } else {
        length = 30000;
    }
    Packet *packet = packet_new(calloc(length, sizeof(int8_t)), length);
    return packet;
}

void packet_release(Packet *packet) {
    // synchronized (cacheMid) {
    packet->pos = 0;

    if (packet->length == 100 && _Packet.cacheMinCount < 1000) {
        linklist_add_tail(_Packet.cacheMin, &packet->link.link);
        _Packet.cacheMinCount++;
    } else if (packet->length == 5000 && _Packet.cacheMidCount < 250) {
        linklist_add_tail(_Packet.cacheMid, &packet->link.link);
        _Packet.cacheMidCount++;
    } else if (packet->length == 30000 && _Packet.cacheMaxCount < 50) {
        linklist_add_tail(_Packet.cacheMax, &packet->link.link);
        _Packet.cacheMaxCount++;
    }
    // }
}

static int8_t *copy_of_range(int8_t *src, int start, int end) {
    if (start > end) {
        return NULL;
    }
    const int len = end - start;
    int8_t *array = malloc(len);
    memcpy(array, src + start, len);
    return array;
}

int8_t *take(Packet *packet) {
    int size = packet->pos;
    packet->pos = 0;
    return copy_of_range(packet->data, 0, size);
}

Packet *slice(Packet *packet, int offset, int length) {
    return packet_new(copy_of_range(packet->data, offset, offset + length), length);
}

int8_t *slice_bytes(Packet *packet, int offset, int length) {
    return copy_of_range(packet->data, offset, offset + length);
}

void p1isaac(Packet *packet, int opcode) {
    packet->data[packet->pos++] = (int8_t)(opcode + isaac_next(&packet->random));
}

void p1(Packet *packet, int value) {
    packet->data[packet->pos++] = (int8_t)value;
}

void p2(Packet *packet, int value) {
    packet->data[packet->pos++] = (int8_t)(value >> 8);
    packet->data[packet->pos++] = (int8_t)value;
}

void ip2(Packet *packet, int value) {
    packet->data[packet->pos++] = (int8_t)value;
    packet->data[packet->pos++] = (int8_t)(value >> 8);
}

void p3(Packet *packet, int value) {
    packet->data[packet->pos++] = (int8_t)(value >> 16);
    packet->data[packet->pos++] = (int8_t)(value >> 8);
    packet->data[packet->pos++] = (int8_t)value;
}

void p4(Packet *packet, int value) {
    packet->data[packet->pos++] = (int8_t)(value >> 24);
    packet->data[packet->pos++] = (int8_t)(value >> 16);
    packet->data[packet->pos++] = (int8_t)(value >> 8);
    packet->data[packet->pos++] = (int8_t)value;
}

void ip4(Packet *packet, int value) {
    packet->data[packet->pos++] = (int8_t)value;
    packet->data[packet->pos++] = (int8_t)(value >> 8);
    packet->data[packet->pos++] = (int8_t)(value >> 16);
    packet->data[packet->pos++] = (int8_t)(value >> 24);
}

void p8(Packet *packet, int64_t value) {
    packet->data[packet->pos++] = (int8_t)(value >> 56);
    packet->data[packet->pos++] = (int8_t)(value >> 48);
    packet->data[packet->pos++] = (int8_t)(value >> 40);
    packet->data[packet->pos++] = (int8_t)(value >> 32);
    packet->data[packet->pos++] = (int8_t)(value >> 24);
    packet->data[packet->pos++] = (int8_t)(value >> 16);
    packet->data[packet->pos++] = (int8_t)(value >> 8);
    packet->data[packet->pos++] = (int8_t)value;
}

void pjstr(Packet *packet, const char *str) {
    size_t len = strlen(str);
    memcpy(packet->data + packet->pos, str, len);
    packet->pos += (int)len;
    packet->data[packet->pos++] = 10;
}

void pdata(Packet *packet, int8_t *src, int length, int offset) {
    memcpy(packet->data + packet->pos, src + offset, length);
    packet->pos += length;
}

void psize1(Packet *packet, int length) {
    packet->data[packet->pos - length - 1] = (int8_t)length;
}

void psize2(Packet *packet, int length) {
    packet->data[packet->pos - length - 2] = (int8_t)(length >> 8);
    packet->data[packet->pos - length - 1] = (int8_t)length;
}

void psize4(Packet *packet, int length) {
    packet->data[packet->pos - length - 4] = (int8_t)(length >> 24);
    packet->data[packet->pos - length - 3] = (int8_t)(length >> 16);
    packet->data[packet->pos - length - 2] = (int8_t)(length >> 8);
    packet->data[packet->pos - length - 1] = (int8_t)length;
}

int g1isaac(Packet *packet) {
    return (packet->data[packet->pos++] - isaac_next(&packet->random)) & 0xff;
}

int g1(Packet *packet) {
    return packet->data[packet->pos++] & 0xff;
}

int8_t g1b(Packet *packet) {
    return packet->data[packet->pos++];
}

int g2(Packet *packet) {
    packet->pos += 2;
    return ((packet->data[packet->pos - 2] & 0xff) << 8) + (packet->data[packet->pos - 1] & 0xff);
}

int g2b(Packet *packet) {
    packet->pos += 2;
    int value = ((packet->data[packet->pos - 2] & 0xff) << 8) + (packet->data[packet->pos - 1] & 0xff);
    if (value > 32767) {
        value -= 65536;
    }
    return value;
}

int g3(Packet *packet) {
    packet->pos += 3;
    return ((packet->data[packet->pos - 3] & 0xff) << 16) + ((packet->data[packet->pos - 2] & 0xff) << 8) + (packet->data[packet->pos - 1] & 0xff);
}

int g4(Packet *packet) {
    packet->pos += 4;
    return ((packet->data[packet->pos - 4] & 0xff) << 24) + ((packet->data[packet->pos - 3] & 0xff) << 16) + ((packet->data[packet->pos - 2] & 0xff) << 8) + (packet->data[packet->pos - 1] & 0xff);
}

int64_t g8(Packet *packet) {
    int64_t high = (int64_t)g4(packet) & 0xffffffffLL;
    int64_t low = (int64_t)g4(packet) & 0xffffffffLL;
    return (high << 32) + low;
}

char *fastgjstr(Packet *packet) {
    if (packet->data[packet->pos] == 10) {
        packet->pos++;
        return NULL;
    } else {
        return gjstr(packet);
    }
}

char *gjstr(Packet *packet) {
    int start = packet->pos;
    while (packet->data[packet->pos++] != 10)
        ;
    int len = packet->pos - start - 1;
    char *str = malloc(len + 1);
    memcpy(str, packet->data + start, len);
    str[len] = '\0';
    return str;
}

int8_t *gjstrbyte(Packet *packet) {
    int start = packet->pos;
    while (packet->data[packet->pos++] != 10)
        ;
    int len = packet->pos - start - 1;
    int8_t *bytes = malloc(len);
    memcpy(bytes, packet->data + start, len);
    return bytes;
}

void gdata(Packet *packet, int length, int offset, int8_t *dest) {
    memcpy(dest + offset, packet->data + packet->pos, length);
    packet->pos += length;
}

void access_bits(Packet *packet) {
    packet->bit_pos = packet->pos * 8;
}

int gbit(Packet *packet, int n) {
    int byteOffset = packet->bit_pos >> 3;
    int msb = 8 - (packet->bit_pos & 0x7);
    int i = 0;
    packet->bit_pos += n;
    while (n > msb) {
        i += (packet->data[byteOffset++] & BITMASK[msb]) << (n - msb);
        n -= msb;
        msb = 8;
    }
    if (n == msb) {
        i += packet->data[byteOffset] & BITMASK[msb];
    } else {
        i += packet->data[byteOffset] >> (msb - n) & BITMASK[n];
    }
    return i;
}

void pbit(Packet *packet, int n, int value) {
    int byteOffset = packet->bit_pos >> 3;
    int remaining = 8 - (packet->bit_pos & 0x7);
    packet->bit_pos += n;

    for (; n > remaining; remaining = 8) {
        packet->data[byteOffset] &= ~BITMASK[remaining];
        packet->data[byteOffset++] |= (value >> (n - remaining)) & BITMASK[remaining];
        n -= remaining;
    }

    if (n == remaining) {
        packet->data[byteOffset] &= ~BITMASK[remaining];
        packet->data[byteOffset] |= value & BITMASK[remaining];
    } else {
        packet->data[byteOffset] &= ~BITMASK[n] << (remaining - n);
        packet->data[byteOffset] |= (value & BITMASK[n]) << (remaining - n);
    }
}

void access_bytes(Packet *packet) {
    packet->pos = (packet->bit_pos + 7) / 8;
}

int gsmart(Packet *packet) {
    int peek = packet->data[packet->pos] & 0xff;
    return peek < 128 ? g1(packet) - 64 : g2(packet) - 0xc000;
}

int gsmarts(Packet *packet) {
    int peek = packet->data[packet->pos] & 0xff;
    return peek < 128 ? g1(packet) : g2(packet) - 0x8000;
}

void rsaenc(Packet *packet, const char *mod, const char *exp) {
    int length = packet->pos;
    packet->pos = 0;

    int8_t *temp = malloc(length);
    gdata(packet, length, 0, temp);

    // NOTE: keeping init here instead of packet_init_global so it's easy to disable rsa
    struct rsa rsa = {0};
    if (rsa_init(&rsa, exp, mod) < 0) {
        rs2_error("rsa_init failed\n");
        exit(1);
    }

    int8_t enc[RSA_BUF_LEN / 2] = {0};
    int enc_len = rsa_crypt(&rsa, temp, length, enc, sizeof(enc));
    if (enc_len < 0) {
        rs2_error("failed to rsa_crypt\n");
        return;
    }
    free(temp);

    packet->pos = 0;
    p1(packet, enc_len);

    /* in java's BigInteger byte array array, zeros at the beginning are
     * ignored unless they're being used to indicate the MSB for sign. since
     * the byte array lengths range from 63-65 and we always want a positive
     * integer, we can make result_length 65 and begin with up to two 0 bytes */
    // TODO: what's wrong here? this applies to all crypto libs
    if (sizeof(enc) != enc_len) {
        if ((size_t)enc_len < sizeof(enc)) {
            // without check this turns into infinite loop when enc_len is 65
            for (size_t i = 0; i < sizeof(enc) - enc_len; i++) {
                rs2_error("buffer %zu enc len %d\n", sizeof(enc), enc_len);
                p1(packet, 0);
            }
        }
        rs2_error("buffer %zu enc len %d\n", sizeof(enc), enc_len);
    }

    pdata(packet, enc, enc_len, 0);
}

void rsadec(Packet *packet, const char *mod, const char *exp) {
    int length = g1(packet);
    int8_t *enc = malloc(length);
    gdata(packet, length, 0, enc);

    // NOTE: keeping init here instead of packet_init_global so it's easy to disable rsa
    struct rsa rsa = {0};
    if (rsa_init(&rsa, exp, mod) < 0) {
        rs2_error("rsa_init failed\n");
        exit(1);
    }

    int8_t dec[RSA_BUF_LEN / 2] = {0};
    int dec_len = rsa_crypt(&rsa, enc, length, dec, sizeof(enc));
    if (dec_len < 0) {
        rs2_error("failed to rsa_crypt\n");
        return;
    }
    free(enc);

    packet->pos = 0;
    pdata(packet, dec, dec_len, 0);
    packet->pos = 0; // reset afterwards to read the new data
}
