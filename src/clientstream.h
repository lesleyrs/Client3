#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <errno.h>
#include <fcntl.h>

#ifdef __WII__
#include <network.h>
#else
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#endif

typedef struct ClientStream ClientStream;
#include "gameshell.h"

struct ClientStream {
#ifdef _WIN32
    SOCKET socket;
#else
    int socket;
#endif
    bool closed;
    GameShell *shell;
    int8_t buf[5000];
    int bufLen;
    int bufPos;
};

ClientStream *clientstream_new(GameShell *shell, int port);
void clientstream_close(ClientStream *stream);
int clientstream_available(ClientStream *stream, int length);
int clientstream_read_byte(ClientStream *stream);
int clientstream_read_bytes(ClientStream *stream, int8_t *dst, int off, int len);
int clientstream_write(ClientStream *stream, int8_t *src, int len, int off);
const char *dnslookup(const char *hostname);
