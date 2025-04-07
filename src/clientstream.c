#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "clientstream.h"
#include "platform.h"

#ifndef NXDK
#include <fcntl.h>
#endif

#ifdef __WII__
#include <network.h>
#elif defined(NXDK)
#include <lwip/netdb.h>
#include <nxdk/net.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifndef __vita__
#include <sys/ioctl.h>
#endif
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef _arch_dreamcast
#include <kos.h>
#include <kos/net.h>
#include <ppp/ppp.h>
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_NET);
#endif

#ifdef __PSP__
#include <pspdebug.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <psputility.h>
#endif

#if _WIN32
#define close closesocket
#define ioctl ioctlsocket
#endif

#ifdef __WII__
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

bool clientstream_init(void) {
#ifdef _WIN32
    WSADATA wsa_data = {0};
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);

    if (ret < 0) {
        rs2_error("WSAStartup() error: %d\n", WSAGetLastError());
        return false;
    }
#endif
#ifdef _arch_dreamcast
    if (!modem_init()) {
        rs2_error("modem_init failed!\n");
        return false;
    }

    ppp_init();

    rs2_log("Dialing connection\n");
    int err = ppp_modem_init("555", 0, NULL);
    if (err != 0) {
        rs2_error("Couldn't dial a connection (%d)\n", err);
        return false;
    }

    rs2_log("Establishing PPP link\n");
    ppp_set_login("dream", "cast");

    err = ppp_connect();
    if (err != 0) {
        rs2_error("Couldn't establish PPP link (%d)\n", err);
        return false;
    }
#endif
#ifdef __PSP__
    sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    sceUtilityLoadNetModule(PSP_NET_MODULE_INET);

    // pspDebugScreenClear();
    pspDebugScreenEnableBackColor(0); // NOTE: hides glitchy bg

    int err = sceNetInit(64 * 1024, 32, 2 * 1024, 32, 2 * 1024);
    if (err) {
        pspDebugScreenPrintf("Error 0x%08X in sceNetInit\n", err);
        return false;
    }
    err = sceNetInetInit();
    if (err) {
        pspDebugScreenPrintf("Error 0x%08X in sceNetInetInit\n", err);
        return false;
    }
    err = sceNetResolverInit();
    if (err) {
        pspDebugScreenPrintf("Error 0x%08X in sceNetResolverInit\n", err);
        return false;
    }

    err = sceNetApctlInit(0x2000, 20);
    if (err) {
        pspDebugScreenPrintf("Error 0x%08X in sceNetApctlInit\n", err);
        return false;
    }
    err = sceNetApctlConnect(1);
    if (err) {
        pspDebugScreenPrintf("Error 0x%08X in sceNetApctlConnect\n", err);
        return false;
    }

    // TODO: handle multiple saved access points
    int apctl_state = 0;
    int apctl_last_state = -1;
    bool apctl_connected = false;
    while (!apctl_connected) {
        if (apctl_state != apctl_last_state) {
            pspDebugScreenPrintf("[WLAN] Connection state %d of 4\n", apctl_state);
            apctl_last_state = apctl_state;
        }

        if (apctl_state == PSP_NET_APCTL_STATE_GOT_IP) {
            pspDebugScreenPrintf("[WLAN] Connected!\n");
            apctl_connected = true;
        }

        err = sceNetApctlGetState(&apctl_state);
        if (err) {
            pspDebugScreenPrintf("Error 0x%08X in sceNetApctlGetState\n", err);
            return false;
        }

        if (apctl_state == 0 && apctl_last_state > 0) {
            pspDebugScreenPrintf("[WLAN] Retrying...\n");
            apctl_last_state = -1;
            err = sceNetApctlConnect(1);
            if (err) {
                pspDebugScreenPrintf("Error 0x%08X in sceNetApctlConnect\n", err);
                return false;
            }
        }

        delay_ticks(50);
    }
#endif
#ifdef NXDK
    if (nxNetInit(NULL) < 0) {
        return false;
    }
#endif
#ifdef __SWITCH__
    socketInitializeDefault();
#endif
    return true;
}

ClientStream *clientstream_new(void) {
    ClientStream *stream = calloc(1, sizeof(ClientStream));
    return stream;
}

ClientStream *clientstream_opensocket(int port) {
    ClientStream *stream = clientstream_new();

    int ret = 0;
#ifdef __WII__
    char local_ip[16] = {0};
    char gateway[16] = {0};
    char netmask[16] = {0};

    // TODO only forcing 127.0.0.1 works on dolphin emulator
    // TODO but it still shows -1 when it works?
    ret = if_config("127.0.0.1", netmask, gateway, true, 20);
    // ret = if_config(local_ip, netmask, gateway, true, 20);

    if (ret < 0) {
        rs2_error("if_config(): %d\n", ret);
        // return NULL;
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
    // used by old windows, xbox, wii, psp, nds, etc
    struct hostent *host_addr = gethostbyname(_Client.socketip);

    if (host_addr) {
        memcpy(&server_addr.sin_addr, host_addr->h_addr_list[0],
               sizeof(struct in_addr));
    }
#endif

    stream->socket = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (stream->socket < 0) {
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
#ifndef __NDS__
    setsockopt(stream->socket, IPPROTO_TCP, TCP_NODELAY, (const char *)&set, sizeof(set));
#endif
#if !defined(__3DS__) && !defined(__WIIU__)
    struct timeval socket_timeout = {30, 0};
    setsockopt(stream->socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&socket_timeout, sizeof(socket_timeout));
    setsockopt(stream->socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&socket_timeout, sizeof(socket_timeout));
#endif

#if defined(__PSP__) || defined(__vita__) || defined(__ps2sdk__) || defined(_arch_dreamcast)
    int flags = fcntl(stream->socket, F_GETFL, 0);
    if (flags == -1) {
        rs2_error("fcntl F_GETFL failed\n");
        clientstream_close(stream);
        return NULL;
    }
    if (fcntl(stream->socket, F_SETFL, flags | O_NONBLOCK) == -1) {
        rs2_error("fcntl F_SETFL failed\n");
        clientstream_close(stream);
        return NULL;
    }
#else
    ret = ioctl(stream->socket, FIONBIO, &set);

    if (ret < 0) {
        rs2_error("ioctl() error: %d\n", ret);
        clientstream_close(stream);
        return NULL;
    }
#endif

// TODO check if this is needed
#if !defined(_WIN32) && !defined(__WII__)
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

#ifndef __WII__
                if (getsockopt(stream->socket, SOL_SOCKET, SO_ERROR, (void *)(&valopt), &lon) < 0) {
                    rs2_error("getsockopt() error:  %s (%d)\n", strerror(errno),
                              errno);

                    return NULL;
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
        // fixes emulators
#if !defined(__vita__) && !defined(__3DS__)
        clientstream_close(stream);
        return NULL;
#endif
    }

    return stream;
}

void clientstream_close(ClientStream *stream) {
    if (stream->socket > -1) {
        close(stream->socket);
        stream->socket = -1;
    }

    stream->closed = true;
}

int clientstream_available(ClientStream *stream, int len) {
    if (stream->bufLen >= len) {
        return 1;
    }

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

int clientstream_write(ClientStream *stream, const int8_t *src, int len, int off) {
    if (!stream->closed) {
#if defined(_WIN32) || defined(__SWITCH__) || defined(__NDS__)
        return send(stream->socket, (const char *)src + off, len, 0);
#else
        return write(stream->socket, src + off, len);
#endif
    }

    return -1;
}

// TODO test this when adding new platforms due to localhost not showing welcome screen by default
const char *dnslookup(const char *hostname) {
#ifdef __WII__
    u32 ip = net_gethostip();

    ip = ntohl(ip);

    struct in_addr addr;
    addr.s_addr = ip;
    char *ip_str = inet_ntoa(addr);
    if (!ip_str) {
        return "unknown";
    }
    return ip_str;
#elif defined(_arch_dreamcast) || defined(NXDK) || defined(__NDS__)
    return "unknown";
#elif defined(MODERN_POSIX)
    struct sockaddr_in client_addr = {0};
    client_addr.sin_family = AF_INET;

    inet_pton(AF_INET, hostname, &client_addr.sin_addr);
#ifndef __WIIU__
    char host[MAX_STR];
    int result = getnameinfo((struct sockaddr *)&client_addr, sizeof(client_addr), host, sizeof(host), NULL, 0, NI_NAMEREQD);
    if (result == 0) {
        return platform_strdup(host);
    }
#endif
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
