#pragma once

#include <stddef.h>
#include <stdint.h>

#include "datastruct/doublylinkable.h"
#include "datastruct/linklist.h"
#include "thirdparty/isaac.h"

typedef struct {
    DoublyLinkable link;
    int8_t *data;
    int length;
    int pos;
    int bit_pos;
    struct isaac random;
} Packet;

typedef struct {
    int cacheMinCount;
    int cacheMidCount;
    int cacheMaxCount;
    LinkList *cacheMin;
    LinkList *cacheMid;
    LinkList *cacheMax;
} PacketData;

void packet_free_global(void);
void packet_init_global(void);
Packet *packet_new(int8_t *src, int length);
void packet_free(Packet *packet);
Packet *packet_alloc(int type);
void packet_release(Packet *packet);
void p1isaac(Packet *packet, int opcode);
void p1(Packet *packet, int value);
void p2(Packet *packet, int value);
void ip2(Packet *packet, int value);
void p3(Packet *packet, int value);
void p4(Packet *packet, int value);
void ip4(Packet *packet, int value);
void p8(Packet *packet, int64_t value);
void pjstr(Packet *packet, const char *str);
void pdata(Packet *packet, int8_t *src, int length, int offset);
void psize1(Packet *packet, int length);
void psize2(Packet *packet, int length);
void psize4(Packet *packet, int length);
int g1isaac(Packet *packet);
int g1(Packet *packet);
int8_t g1b(Packet *packet);
int g2(Packet *packet);
int g2b(Packet *packet);
int g3(Packet *packet);
int g4(Packet *packet);
int64_t g8(Packet *packet);
char *fastgjstr(Packet *packet);
char *gjstr(Packet *packet);
int8_t *gjstrbyte(Packet *packet);
void gdata(Packet *packet, int length, int offset, int8_t *dest);
void access_bits(Packet *packet);
int gbit(Packet *packet, int n);
void pbit(Packet *packet, int n, int value);
void access_bytes(Packet *packet);
int gsmart(Packet *packet);
int gsmarts(Packet *packet);
void rsaenc(Packet *packet, const char *mod, const char *exp);
void rsadec(Packet *packet, const char *mod, const char *exp);
int rs_crc32(const int8_t *data, size_t length);
