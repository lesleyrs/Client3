#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct ClientStream ClientStream;

struct ClientStream {
    int socket;
    bool closed;
    int8_t buf[5000];
    int bufLen;
    int bufPos;
};

bool clientstream_init(void);
ClientStream *clientstream_new(void);
ClientStream *clientstream_opensocket(int port);
void clientstream_close(ClientStream *stream);
int clientstream_available(ClientStream *stream, int length);
int clientstream_read_byte(ClientStream *stream);
int clientstream_read_bytes(ClientStream *stream, int8_t *dst, int off, int len);
int clientstream_write(ClientStream *stream, const int8_t *src, int len, int off);
const char *dnslookup(const char *hostname);
