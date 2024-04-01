#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/gl.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#define TICK_RATE 60

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

struct ServerState
{
    Win32Addr last;
    char buffer[256];
};

ServerState* create_server_state(i32 sock, ADDRINFOA *server)
{
    if (bind(sock, server->ai_addr, server->ai_addrlen)) {
        assert(0);
    }
    printf("Bound to port.\n");

    ServerState *state = (ServerState*) malloc(sizeof(ServerState));
    *state = {};

    return state;
}

void create_client_state(i32 sock, ADDRINFOA *server)
{
    if (connect(sock, server->ai_addr, server->ai_addrlen)) {
        win32_check_socket_error();
        assert(0);
    }

    printf("Connected to server.\n");

    char message[] = "----Hello world this is being sent via udp";
    u32 *header = (u32*) message;
    *header = 12346;
    i32 bytes_sent = send(sock, message, sizeof(message), 0);
    printf("%d bytes sent\n", bytes_sent);
    bytes_sent = send(sock, message, sizeof(message), 0);
}

void do_server_things(i32 sock, ServerState *state) 
{
    u32 *header = (u32*) state->buffer;
    char *message = state->buffer + 4;
    Win32Addr from;
    i32 from_len = sizeof(from);

    while (true) {
        i32 bytes_read = recvfrom(sock, state->buffer, sizeof(state->buffer), 0, 
                                  (SOCKADDR*) &from, &from_len);
        if (bytes_read == -1) {
            win32_check_socket_error();
            break;
        }
        if (bytes_read > 0) {
            // Check if message starts with magic number
            if (bytes_read <= 4) {
                continue;
            }
            if ((*header) != 12346) {
                continue;
            }

            printf("---------------------------------\n");
            if (!win32_compare_addr(&state->last, &from)) {
                printf("New client\n");
            }
            printf("Received message:\n%s\nBytes read: %i\n", message, bytes_read);

            state->last = from;
        } else {
            break;
        }
    }
}

void do_client_things()
{
}

i32 main(i32 argc, char **argv)
{
    LARGE_INTEGER perf_count_freq_res;
    QueryPerformanceFrequency(&perf_count_freq_res);
    f64 perf_count_freq = perf_count_freq_res.QuadPart;

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

    void *state = NULL;
    if (is_server) {
        state = create_server_state(sock, server);
    } else {
        create_client_state(sock, server);
    }

    while (true) {
        LARGE_INTEGER perf_start;
        QueryPerformanceCounter(&perf_start);

        if (is_server) {
            do_server_things(sock, (ServerState*) state);
        } else {
            do_client_things();
        }

        LARGE_INTEGER perf_end;
        QueryPerformanceCounter(&perf_end);
        f64 counter_elapsed = perf_end.QuadPart - perf_start.QuadPart;
        f64 time_elapsed = counter_elapsed / perf_count_freq;
        f64 time_to_sleep = ((1.0f / TICK_RATE) - time_elapsed) * 1000;
        
        if (time_to_sleep < 0) {
            printf("Warning: Missed frame budget by %.02fms\n", time_to_sleep);
            time_to_sleep = 0;
        }
        Sleep(time_to_sleep);
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
