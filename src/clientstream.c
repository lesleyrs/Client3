#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "clientstream.h"
#include "platform.h"

#if _WIN32
#define close closesocket
#define ioctl ioctlsocket

static int winsock_init = 0;
#endif

#ifdef WII
#define socket(x, y, z) net_socket(x, y, z)
#define gethostbyname net_gethostbyname
// #define getsockopt net_getsockopt
#define setsockopt net_setsockopt
#define connect net_connect
#define close net_close
#define write net_write
#define recv net_recv
#define ioctl net_ioctl
#define select net_select
#endif

extern ClientData _Client;

ClientStream *clientstream_new(GameShell *shell, int port) {
#ifdef _WIN32
    if (!winsock_init) {
        WSADATA wsa_data = {0};
        int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);

        if (ret < 0) {
            rs2_error("WSAStartup() error: %d\n", WSAGetLastError());
            exit(1);
        }
        winsock_init = 1;
    }
#endif
    ClientStream *stream = calloc(1, sizeof(ClientStream));
    stream->shell = shell;
    stream->closed = false;

    int ret = 0;

#ifdef WII
    char local_ip[16] = {0};
    char gateway[16] = {0};
    char netmask[16] = {0};

    // TODO only forcing 127.0.0.1 works on dolphin emulator
    // TODO but it still shows -1 when it works?
    ret = if_config("127.0.0.1", netmask, gateway, true, 20);
    // ret = if_config(local_ip, netmask, gateway, true, 20);

    if (ret < 0) {
        rs2_error("if_config(): %d\n", ret);
        // exit(1);
    }
#endif

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

#ifdef MODERN_POSIX
    struct addrinfo hints = {0};
    struct addrinfo *result = {0};

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int status = getaddrinfo(_Client.socketip, NULL, &hints, &result);

    if (status != 0) {
#if defined(_WIN32) && defined(__i386__)
        rs2_error("getaddrinfo(): %d\n", status);
#else
        rs2_error("getaddrinfo(): %s\n", gai_strerror(status));
#endif
        stream->closed = 1;
        free(stream);
        return NULL;
    }

    for (struct addrinfo *rp = result; rp; rp = rp->ai_next) {
        struct sockaddr_in *addr = (struct sockaddr_in *)rp->ai_addr;

        if (addr) {
            memcpy(&server_addr.sin_addr, &addr->sin_addr,
                   sizeof(struct in_addr));
            break;
        }
    }

    freeaddrinfo(result);
#else
    // TODO rm #if defined(WIN9X) || defined(WII)
    struct hostent *host_addr = gethostbyname(_Client.socketip);

    if (host_addr) {
        memcpy(&server_addr.sin_addr, host_addr->h_addr_list[0],
               sizeof(struct in_addr));
    }
#endif

#ifdef __SWITCH__
    socketInitializeDefault();
#endif

    stream->socket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (stream->socket == INVALID_SOCKET) {
#else
    if (stream->socket < 0) {
#endif
        rs2_error("socket error: %s (%d)\n", strerror(errno), errno);
        clientstream_close(stream);
        return NULL;
    }

#ifdef __EMSCRIPTEN__
    int attempts_ms = 0;
    errno = 0; // reset errno for subsequent logins

    // TODO confirm this works on all browsers
    while (errno == 0 || errno == 7) {
        if (attempts_ms >= 5000) {
            break;
        }

        ret = connect(stream->socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
        // 30 = Socket is connected (misleading, you get this when it fails too)
        // 7 = Operation already in progress
        // rs2_log("%s %d %d %d\n", strerror(errno), errno, ret, attempts_ms);
        if (errno == 30 && attempts_ms < 2100 /* timed out, this is the only way to know if it failed */) {
            ret = 0;
        } else {
            delay_ticks(100);
            attempts_ms += 100;
        }
    }
#else

#ifdef _WIN32
    unsigned long set = true;
#else
    int set = true;
#endif
    // NOTE: cast for windows
    setsockopt(stream->socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&set, sizeof(set));
    #ifndef __3DS__
    struct timeval socket_timeout = {30, 0};
    setsockopt(stream->socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&socket_timeout, sizeof(socket_timeout));
    setsockopt(stream->socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&socket_timeout, sizeof(socket_timeout));
    #endif

    // NOTE need this since we are single threaded
// #ifdef FIONBIO
    ret = ioctl(stream->socket, FIONBIO, &set);

    if (ret < 0) {
        rs2_error("ioctl() error: %d\n", ret);
        exit(1);
    }
// #endif

// TODO check if this is needed
#if !defined(_WIN32) && !defined(WII)
// #include <signal.h>
//     signal(SIGPIPE, SIG_IGN);
#endif

    ret = connect(stream->socket, (struct sockaddr *)&server_addr,
                  sizeof(server_addr));

    if (ret == -1) {
#ifdef _WIN32
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
        if (errno == EINPROGRESS) {
#endif
            // TODO: why 5 seconds? from rsc-c
            struct timeval timeout = {0};
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(stream->socket, &write_fds);

            // NOTE: cast for windows
            ret = select((int)stream->socket + 1, NULL, &write_fds, NULL, &timeout);

            if (ret > 0) {
                socklen_t lon = sizeof(int);
                int valopt = 0;

                #ifndef WII
                if (getsockopt(stream->socket, SOL_SOCKET, SO_ERROR, (void *)(&valopt), &lon) < 0) {
                    rs2_error("getsockopt() error:  %s (%d)\n", strerror(errno),
                              errno);

                    exit(1);
                }
                #endif

                if (valopt > 0) {
                    ret = -1;
                    errno = valopt;
                } else {
                    ret = 0;
                }
            } else if (ret == 0) {
                rs2_error("connect() timeout\n");
                clientstream_close(stream);
                return NULL;
            }
        }
    }
#endif /* not EMSCRIPTEN */

    if (ret < 0 && errno != 0) {
        rs2_error("connect() error: %s (%d)\n", strerror(errno), errno);
        clientstream_close(stream);
        return NULL;
    }

    return stream;
}

void clientstream_close(ClientStream *stream) {
#ifdef _WIN32
    if (stream->socket > (SOCKET)-1) {
#else
    if (stream->socket > -1) {
#endif
        close(stream->socket);
        stream->socket = -1;
    }

    stream->closed = true;
}

int clientstream_available(ClientStream *stream, int len) {
    if (stream->bufLen >= len) {
        return 1;
    }

    // NOTE: cast for windows to stop clang warning
    int bytes = recv(stream->socket, (char *)stream->buf + stream->bufPos + stream->bufLen, len - stream->bufLen, 0);

    if (bytes < 0) {
        bytes = 0;
    }

    stream->bufLen += bytes;

    if (stream->bufLen < len) {
        return 0;
    }

    return 1;
}

int clientstream_read_byte(ClientStream *stream) {
    if (stream->closed) {
        return -1;
    }

    int8_t byte;

    if (clientstream_read_bytes(stream, &byte, 0, 1) > -1) {
        return byte & 0xff;
    }

    return -1;
}

int clientstream_read_bytes(ClientStream *stream, int8_t *dst, int off, int len) {
    if (stream->closed) {
        return -1;
    }

    if (stream->bufLen > 0) {
        int copy_length;

        if (len > stream->bufLen) {
            copy_length = stream->bufLen;
        } else {
            copy_length = len;
        }

        memcpy(dst, stream->buf + stream->bufPos, copy_length);

        len -= copy_length;
        stream->bufLen -= copy_length;

        if (stream->bufLen == 0) {
            stream->bufPos = 0;
        } else {
            stream->bufPos += copy_length;
        }
    }

    int read_duration = 0;

    while (len > 0) {
        // NOTE: cast for windows to stop clang warning
        int bytes = recv(stream->socket, (char *)dst + off, len, 0);
        if (bytes > 0) {
            off += bytes;
            len -= bytes;
        } else if (bytes == 0) {
            stream->closed = true;
            return -1;
        } else {
            read_duration += 1;

            if (read_duration >= 5000) {
                clientstream_close(stream);
                return -1;
            } else {
                delay_ticks(1);
            }
        }
    }

    return 0;
}

int clientstream_write(ClientStream *stream, int8_t *src, int len, int off) {
    if (!stream->closed) {
#if defined(_WIN32) || defined(__SWITCH__)
        // NOTE: cast for windows
        return send(stream->socket, (const char *)src + off, len, 0);
#else
        return write(stream->socket, src + off, len);
#endif
    }

    return -1;
}

const char *dnslookup(const char *hostname, bool hide_dns) {
    if (hide_dns) {
        return "unknown";
    }

#ifdef WII
    u32 ip = net_gethostip();

    ip = ntohl(ip);

    struct in_addr addr;
    addr.s_addr = ip;
    char *ip_str = inet_ntoa(addr);
    if (!ip_str) {
        return "unknown";
    }
    return ip_str;
#elif defined(MODERN_POSIX)
    struct sockaddr_in client_addr = {0};
    client_addr.sin_family = AF_INET;

    inet_pton(AF_INET, hostname, &client_addr.sin_addr);

    char host[MAX_STR];
    int result = getnameinfo((struct sockaddr *)&client_addr, sizeof(client_addr), host, sizeof(host), NULL, 0, NI_NAMEREQD);
    if (result == 0) {
        return platform_strdup(host);
    }
    return "unknown";
#else
    struct in_addr addr = {.s_addr = inet_addr(hostname)};
    struct hostent *host = gethostbyaddr((const char *)&addr, sizeof(addr), AF_INET);
    if (!host) {
        return "unknown";
    }
    return host->h_name;
#endif
}
