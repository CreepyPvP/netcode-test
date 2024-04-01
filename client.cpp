#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/gl.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef float f32;
typedef double f64;


static ADDRINFOA* win32_select_addr(ADDRINFOA *addr)
{
    ADDRINFOA *selected = NULL;

    while (addr) {
        // We prefer ipv6
        if (!selected || addr->ai_family == AF_INET6) {
            selected = addr;
        }
        addr = addr->ai_next;
    }

    return selected;
}

static ADDRINFO* win32_get_addr_info(char *name, char *port, bool will_bind)
{
    ADDRINFOA hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (!name && will_bind) {     
        // NOTE: Dont set this when connecting to remote host
        hints.ai_flags |= AI_PASSIVE;
    }

    ADDRINFOA *result;
    if (getaddrinfo(name, port, &hints, &result)) {
        // TODO: Handle this
        assert(0);
    }

    return win32_select_addr(result);
}

static void win32_check_socket_error()
{
    i32 error = WSAGetLastError();
    printf("Error %i\n", error);
    if (error == WSAEWOULDBLOCK) {
        printf("(Would block)\n");
    }   
}

union Win32Addr
{
    sockaddr addr;
    sockaddr_in6 ipv6;
    sockaddr_in ipv4;
};

bool win32_compare_addr(Win32Addr* a, Win32Addr *b)
{
    if (a->addr.sa_family != b->addr.sa_family) {
        return false;
    }

    if (a->addr.sa_family == AF_INET) {
        return a->ipv4.sin_port == b->ipv4.sin_port &&
                a->ipv4.sin_addr.S_un.S_addr == b->ipv4.sin_addr.S_un.S_addr;
    }
    if (a->addr.sa_family == AF_INET6) {
        if (a->ipv6.sin6_port != b->ipv6.sin6_port) {
            return false;
        }

        for (u32 i = 0; i < 16; ++i) {
            if (a->ipv6.sin6_addr.u.Byte[i] != b->ipv6.sin6_addr.u.Byte[i]) {
                return false;
            }
        }

        return true;
    }

    return false;
}

i32 main(i32 argc, char **argv)
{
    bool is_server = argc != 1;

    if (is_server) {
        printf("Running server.\n");
    } else {
        printf("Running client.\n");
    }

    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        printf("WSAStartup failed\n");
        assert(0);
    }

    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
        printf("Version 2.2 of Winsock is not available\n");
        WSACleanup();
        assert(0);
    }

    ADDRINFOA *server = win32_get_addr_info(NULL, "7000", is_server);

    i32 sock = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    assert(sock != -1);


    if (is_server) {
        if (bind(sock, server->ai_addr, server->ai_addrlen)) {
            assert(0);
        }
        printf("Bound to port.\n");

        Win32Addr last = {};
        char buffer[256];
        Win32Addr from;
        i32 from_len = sizeof(from);

        while (true) {
            printf("---------------------------------\n");
            i32 bytes_read = recvfrom(sock, buffer, sizeof(buffer), 0, (SOCKADDR*) &from, &from_len);
            if (bytes_read == -1) {
                win32_check_socket_error();
            }
            if (bytes_read > 0) {
                if (!win32_compare_addr(&last, &from)) {
                    printf("New client\n");
                }
                printf("Received message:\n%s\nBytes read: %i\n", buffer, bytes_read);

                last = from;
            }
        }
    } else {
        if (connect(sock, server->ai_addr, server->ai_addrlen)) {
            win32_check_socket_error();
            assert(0);
        }

        printf("Connected to server.\n");

        char message[] = "Hello world this is being sent via udp";
        i32 bytes_sent = send(sock, message, sizeof(message), 0);
        printf("%d bytes sent\n", bytes_sent);
        bytes_sent = send(sock, message, sizeof(message), 0);
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
