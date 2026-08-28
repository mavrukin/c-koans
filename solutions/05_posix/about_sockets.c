/*
 * About Sockets
 *
 * A socket is a file descriptor you can read and write — everything from
 * about_low_level_io still applies. What is new is that the other end is a
 * program, possibly on another machine, and it can vanish mid-sentence.
 *
 * The server sequence is fixed and worth memorising:
 *
 *   socket()   make an endpoint
 *   bind()     give it an address
 *   listen()   mark it as accepting
 *   accept()   take one connection; returns a NEW descriptor
 *
 * The client is shorter: socket(), connect().
 *
 * These koans bind to 127.0.0.1 on a kernel-assigned port, so they never
 * collide with anything and never touch a network.
 */
#include "koan.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* The read/write loops from about_low_level_io. Sockets need them more. */
static ssize_t read_full(int fd, void *buf, size_t want)
{
    unsigned char *p = buf;
    size_t         got = 0;
    while (got < want) {
        ssize_t n = read(fd, p + got, want - got);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) break;
        got += (size_t)n;
    }
    return (ssize_t)got;
}

static bool write_full(int fd, const void *buf, size_t len)
{
    const unsigned char *p = buf;
    size_t               sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, p + sent, len - sent);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        sent += (size_t)n;
    }
    return true;
}

/*
 * A listening socket bound to port 0 gets a free port from the kernel, which
 * getsockname then reports. This is how a test binds without guessing.
 */
static int listen_on_any_port(uint16_t *chosen_port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = 0 };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) { close(fd); return -1; }
    if (listen(fd, 8) != 0)                                    { close(fd); return -1; }

    struct sockaddr_in bound;
    socklen_t          len = sizeof bound;
    if (getsockname(fd, (struct sockaddr *)&bound, &len) != 0)  { close(fd); return -1; }

    *chosen_port = ntohs(bound.sin_port);
    return fd;
}

static int connect_to(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(port) };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/*
 * A socket is just a descriptor. Binding to port 0 asks the kernel to pick.
 */
KOAN(a_socket_is_a_descriptor)
{
    uint16_t port = 0;
    int      listener = listen_on_any_port(&port);

    KOAN_TRUE(listener > 2);
    KOAN_TRUE(port > 0);            /* the kernel chose a real port */

    close(listener);
}

/*
 * Addresses are stored in network byte order — big-endian, regardless of the
 * machine. htons/ntohs convert; forgetting them is the classic first bug, and
 * it fails silently by connecting to the wrong port.
 */
KOAN(network_byte_order_is_big_endian)
{
    uint16_t host = 0x1234;
    uint16_t net  = htons(host);

    /* Converting back always returns the original. */
    KOAN_EQ_INT(/*__*/ 0x1234, ntohs(net));

    /* On a little-endian machine the bytes are swapped; on a big-endian one
     * they are not. Assert what is true everywhere. */
    KOAN_EQ_INT(/*__*/ 0x1234, ntohs(htons(0x1234)));
    KOAN_EQ_INT(/*__*/ 8080, (int)ntohs(htons(8080)));

    /* inet_pton parses text into an in_addr, in network order. */
    struct in_addr addr;
    KOAN_EQ_INT(/*__*/ 1, inet_pton(AF_INET, "127.0.0.1", &addr));

    char text[INET_ADDRSTRLEN];
    KOAN_TRUE(inet_ntop(AF_INET, &addr, text, sizeof text) != nullptr);
    KOAN_EQ_STR(/*__STR*/ "127.0.0.1", text);

    /* Malformed input is rejected: 0 means "not parseable", -1 means the
     * address family was wrong. */
    KOAN_EQ_INT(/*__*/ 0, inet_pton(AF_INET, "not.an.address", &addr));
}

/*
 * accept returns a *new* descriptor for the connection. The listening socket
 * stays open and keeps accepting; closing the wrong one is a common mistake.
 */
KOAN(accept_returns_a_new_descriptor)
{
    uint16_t port = 0;
    int      listener = listen_on_any_port(&port);
    KOAN_TRUE(listener >= 0);

    int client = connect_to(port);
    KOAN_TRUE(client >= 0);

    int served = accept(listener, nullptr, nullptr);
    KOAN_TRUE(served >= 0);

    /* Three distinct descriptors: the listener, the client end, the server
     * end of that one connection. */
    KOAN_TRUE(served != listener);
    KOAN_TRUE(served != client);

    /* The listener is still usable — a second client connects fine. */
    int second = connect_to(port);
    KOAN_TRUE(second >= 0);
    int served2 = accept(listener, nullptr, nullptr);
    KOAN_TRUE(served2 >= 0);

    close(served2); close(second);
    close(served); close(client); close(listener);
}

/*
 * Data written to one end appears at the other. A socket is a byte stream: it
 * preserves order but not boundaries, so one write may arrive as several
 * reads, and several writes may arrive as one.
 */
KOAN(a_stream_socket_preserves_order_not_boundaries)
{
    uint16_t port = 0;
    int      listener = listen_on_any_port(&port);
    int      client = connect_to(port);
    int      served = accept(listener, nullptr, nullptr);

    /* Three separate writes... */
    write_full(client, "one", 3);
    write_full(client, "two", 3);
    write_full(client, "six", 3);
    shutdown(client, SHUT_WR);          /* say "no more from me" */

    /* ...may arrive as one read. read_full loops until it has all nine. */
    char buf[16] = {};
    KOAN_EQ_INT(/*__*/ 9, (int)read_full(served, buf, 9));
    KOAN_EQ_STR(/*__STR*/ "onetwosix", buf);

    /* After the peer shuts down writing, read returns 0: end of stream. */
    KOAN_EQ_INT(/*__*/ 0, (int)read(served, buf, sizeof buf));

    close(served); close(client); close(listener);
}

/*
 * Because boundaries are not preserved, a protocol must supply its own. The
 * two ways are a length prefix and a delimiter; the web server uses a
 * delimiter (the blank line ending the headers).
 */
static bool send_framed(int fd, const char *msg)
{
    uint32_t len = htonl((uint32_t)strlen(msg));
    return write_full(fd, &len, sizeof len) && write_full(fd, msg, strlen(msg));
}

static bool recv_framed(int fd, char *out, size_t cap)
{
    uint32_t len_net;
    if (read_full(fd, &len_net, sizeof len_net) != (ssize_t)sizeof len_net)
        return false;

    uint32_t len = ntohl(len_net);
    if (len >= cap) return false;                 /* refuse to overflow */

    if (read_full(fd, out, len) != (ssize_t)len) return false;
    out[len] = '\0';
    return true;
}

KOAN(a_protocol_must_supply_its_own_framing)
{
    uint16_t port = 0;
    int      listener = listen_on_any_port(&port);
    int      client = connect_to(port);
    int      served = accept(listener, nullptr, nullptr);

    KOAN_EQ_INT(/*__*/ 1, send_framed(client, "first message"));
    KOAN_EQ_INT(/*__*/ 1, send_framed(client, "second"));

    char buf[64];
    KOAN_EQ_INT(/*__*/ 1, recv_framed(served, buf, sizeof buf));
    KOAN_EQ_STR(/*__STR*/ "first message", buf);

    /* The second message is recovered intact, even though both may have
     * arrived in one packet. */
    KOAN_EQ_INT(/*__*/ 1, recv_framed(served, buf, sizeof buf));
    KOAN_EQ_STR(/*__STR*/ "second", buf);

    /* A length larger than the buffer is refused rather than overflowing. */
    uint32_t huge = htonl(1000000);
    write_full(client, &huge, sizeof huge);
    KOAN_EQ_INT(/*__*/ 0, recv_framed(served, buf, sizeof buf));

    close(served); close(client); close(listener);
}

/*
 * socketpair makes two already-connected sockets without any addressing. It
 * is the simplest way for a parent and child to talk, and unlike a pipe it is
 * bidirectional.
 */
KOAN(socketpair_needs_no_address)
{
    int sv[2];
    KOAN_EQ_INT(/*__*/ 0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));

    /* Both directions work on both descriptors. */
    write_full(sv[0], "ping", 4);
    char buf[8] = {};
    KOAN_EQ_INT(/*__*/ 4, (int)read_full(sv[1], buf, 4));
    KOAN_EQ_STR(/*__STR*/ "ping", buf);

    memset(buf, 0, sizeof buf);
    write_full(sv[1], "pong", 4);
    KOAN_EQ_INT(/*__*/ 4, (int)read_full(sv[0], buf, 4));
    KOAN_EQ_STR(/*__STR*/ "pong", buf);

    close(sv[0]); close(sv[1]);
}

/*
 * Writing to a socket whose peer has closed raises SIGPIPE, which by default
 * kills the process. A server must either ignore the signal and check for
 * EPIPE, or use MSG_NOSIGNAL on each send.
 */
KOAN(writing_to_a_closed_peer_would_kill_you)
{
    void (*previous)(int) = signal(SIGPIPE, SIG_IGN);

    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    close(sv[1]);                         /* the peer is gone */

    errno = 0;
    ssize_t n = write(sv[0], "data", 4);

    /* With SIGPIPE ignored, the write fails with EPIPE instead of killing us. */
    KOAN_EQ_INT(/*__*/ -1, (int)n);
    KOAN_EQ_INT(/*__*/ EPIPE, errno);

    close(sv[0]);
    signal(SIGPIPE, previous);
}

/*
 * Bringing it together.
 *
 * A request/response server on a background thread, and a client that talks
 * to it — the smallest complete networked program. It uses the framing above,
 * handles a client that disconnects early, and shuts down cleanly.
 */
typedef struct {
    uint16_t port;
    int      listener;
    int      handled;
} Server;

/*
 * Note how this loop ends. Closing the listener from another thread while
 * this one is blocked in accept() is NOT portably guaranteed to wake it —
 * the call may sit there forever. A protocol-level shutdown message is
 * deterministic, and is what a real server does too.
 */
static void *serve_until_quit(void *arg)
{
    Server *s = arg;
    bool    running = true;

    while (running) {
        int conn = accept(s->listener, nullptr, nullptr);
        if (conn < 0) break;

        char request[256];
        while (recv_framed(conn, request, sizeof request)) {
            if (strcmp(request, "quit") == 0) { running = false; break; }

            char reply[300];
            snprintf(reply, sizeof reply, "echo:%s", request);
            if (!send_framed(conn, reply)) break;
            s->handled++;
        }
        close(conn);
    }
    return nullptr;
}

KOAN(assembling_a_request_response_server)
{
    Server s = { 0 };
    s.listener = listen_on_any_port(&s.port);
    KOAN_TRUE(s.listener >= 0);

    pthread_t thread;
    pthread_create(&thread, nullptr, serve_until_quit, &s);

    int client = connect_to(s.port);
    KOAN_TRUE(client >= 0);

    char reply[300];

    KOAN_EQ_INT(/*__*/ 1, send_framed(client, "hello"));
    KOAN_EQ_INT(/*__*/ 1, recv_framed(client, reply, sizeof reply));
    KOAN_EQ_STR(/*__STR*/ "echo:hello", reply);

    /* The connection stays open for a second exchange. */
    KOAN_EQ_INT(/*__*/ 1, send_framed(client, "again"));
    KOAN_EQ_INT(/*__*/ 1, recv_framed(client, reply, sizeof reply));
    KOAN_EQ_STR(/*__STR*/ "echo:again", reply);

    /* Hanging up mid-conversation must not disturb the server. */
    close(client);

    int second = connect_to(s.port);
    KOAN_TRUE(second >= 0);
    KOAN_EQ_INT(/*__*/ 1, send_framed(second, "still up"));
    KOAN_EQ_INT(/*__*/ 1, recv_framed(second, reply, sizeof reply));
    KOAN_EQ_STR(/*__STR*/ "echo:still up", reply);
    close(second);

    /* A final connection carrying "quit" ends the server loop cleanly. */
    int closer = connect_to(s.port);
    send_framed(closer, "quit");
    close(closer);

    pthread_join(thread, nullptr);
    close(s.listener);

    /* Three real exchanges; "quit" is control, not an echo. */
    KOAN_EQ_INT(/*__*/ 3, s.handled);
}

KOAN_LESSON(lesson_about_sockets, "About Sockets",
    KOAN_CASE(a_socket_is_a_descriptor),
    KOAN_CASE(network_byte_order_is_big_endian),
    KOAN_CASE(accept_returns_a_new_descriptor),
    KOAN_CASE(a_stream_socket_preserves_order_not_boundaries),
    KOAN_CASE(a_protocol_must_supply_its_own_framing),
    KOAN_CASE(socketpair_needs_no_address),
    KOAN_CASE(writing_to_a_closed_peer_would_kill_you),
    KOAN_CASE(assembling_a_request_response_server)
);
