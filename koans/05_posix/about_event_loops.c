/*
 * About Event Loops
 *
 * A thread blocked in read() can do nothing else. With one connection that is
 * fine; with ten thousand it is not, and a thread each is not the answer
 * either.
 *
 * The alternative is to ask the kernel which descriptors are ready and act
 * only on those. That is poll(), and an event loop is nothing more than
 * poll() in a while loop.
 *
 * Two things must be true for it to work:
 *
 *   - descriptors must be non-blocking, or a "ready" descriptor can still
 *     block you (readiness can be a false positive)
 *   - you must handle partial reads and writes, because you can no longer
 *     loop until done
 */
#include "koan.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static bool set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

/*
 * A non-blocking read on an empty descriptor returns -1 with EAGAIN rather
 * than waiting. That is not an error — it is the whole point, and treating it
 * as one is the first bug everybody writes.
 */
KOAN(a_non_blocking_read_returns_eagain)
{
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    KOAN_EQ_INT(__, set_nonblocking(sv[0]));

    char buf[16];

    /* Nothing has been written, so there is nothing to read — but we are
     * not made to wait for it. */
    errno = 0;
    KOAN_EQ_INT(__, (int)read(sv[0], buf, sizeof buf));
    KOAN_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);

    /* Once data arrives, the same call succeeds. */
    write(sv[1], "hi", 2);
    KOAN_EQ_INT(__, (int)read(sv[0], buf, sizeof buf));

    close(sv[0]); close(sv[1]);
}

/*
 * poll takes an array of descriptors and the events you care about, and
 * returns how many became ready. It fills in `revents` on each — which you
 * must check per descriptor, because the return value only counts them.
 */
KOAN(poll_reports_which_descriptors_are_ready)
{
    int a[2], b[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, a);
    socketpair(AF_UNIX, SOCK_STREAM, 0, b);

    struct pollfd fds[2] = {
        { .fd = a[0], .events = POLLIN },
        { .fd = b[0], .events = POLLIN },
    };

    /* Nothing is ready, and a zero timeout means "do not wait". */
    KOAN_EQ_INT(__, poll(fds, 2, 0));

    /* Write to the second pair only. */
    write(b[1], "x", 1);

    KOAN_EQ_INT(__, poll(fds, 2, 100));

    /* The count says "one", but only revents says *which* one. */
    KOAN_EQ_INT(__, fds[0].revents & POLLIN);
    KOAN_TRUE((fds[1].revents & POLLIN) != 0);

    close(a[0]); close(a[1]); close(b[0]); close(b[1]);
}

/*
 * A timeout of -1 waits forever, 0 returns immediately, and a positive value
 * waits that many milliseconds. A timeout expiring returns 0, which is
 * distinct from an error (-1).
 */
KOAN(the_timeout_distinguishes_three_outcomes)
{
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    struct pollfd p = { .fd = sv[0], .events = POLLIN };

    /* Timed out: no descriptors ready, and no error. */
    KOAN_EQ_INT(__, poll(&p, 1, 10));
    KOAN_EQ_INT(__, p.revents);

    write(sv[1], "x", 1);
    KOAN_EQ_INT(__, poll(&p, 1, 10));

    close(sv[0]); close(sv[1]);
}

/*
 * POLLHUP and POLLERR arrive whether or not you asked for them. A closed peer
 * shows as readable — read then returns 0 — so an event loop must treat "0
 * bytes read" as a disconnection rather than as nothing to do.
 */
KOAN(a_closed_peer_shows_as_readable)
{
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    close(sv[1]);                       /* the peer hangs up */

    struct pollfd p = { .fd = sv[0], .events = POLLIN };
    KOAN_TRUE(poll(&p, 1, 100) > 0);

    /* Readable, or explicitly hung up — implementations differ, so accept
     * either and let the read decide. */
    KOAN_TRUE((p.revents & (POLLIN | POLLHUP)) != 0);

    char buf[8];
    KOAN_EQ_INT(__, (int)read(sv[0], buf, sizeof buf));   /* end of stream */

    close(sv[0]);
}

/*
 * Asking about a closed descriptor gives POLLNVAL — a programming error, not
 * a runtime condition. It is worth checking for, because the alternative is a
 * loop that spins at full speed on a descriptor it forgot to remove.
 */
KOAN(a_stale_descriptor_gives_pollnval)
{
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

    int stale = sv[0];
    close(sv[0]);

    struct pollfd p = { .fd = stale, .events = POLLIN };
    KOAN_EQ_INT(__, poll(&p, 1, 0));
    KOAN_TRUE((p.revents & POLLNVAL) != 0);

    /* A negative fd is the documented way to skip a slot without removing it
     * from the array — poll ignores it and clears revents. */
    p.fd = -1;
    KOAN_EQ_INT(__, poll(&p, 1, 0));
    KOAN_EQ_INT(__, p.revents);

    close(sv[1]);
}

/*
 * Writability is asked for with POLLOUT, and matters because a socket whose
 * send buffer is full will otherwise block. A loop that only ever polls for
 * POLLIN will stall the moment a slow client stops reading.
 */
KOAN(pollout_reports_room_to_write)
{
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    set_nonblocking(sv[0]);

    struct pollfd p = { .fd = sv[0], .events = POLLOUT };

    /* An idle socket has room immediately. */
    KOAN_EQ_INT(__, poll(&p, 1, 100));
    KOAN_TRUE((p.revents & POLLOUT) != 0);

    /* Fill the buffer until the kernel refuses. This is the condition a
     * server must handle rather than assume away. */
    char    chunk[4096];
    memset(chunk, 'x', sizeof chunk);
    size_t  written = 0;
    for (int i = 0; i < 10000; i++) {
        ssize_t n = write(sv[0], chunk, sizeof chunk);
        if (n < 0) break;
        written += (size_t)n;
    }

    KOAN_TRUE(written > 0);
    KOAN_TRUE(errno == EAGAIN || errno == EWOULDBLOCK);

    close(sv[0]); close(sv[1]);
}

/*
 * Bringing it together.
 *
 * A single-threaded echo server handling several clients at once. One thread,
 * one poll array, no blocking anywhere — the architecture behind nginx and
 * redis, in about sixty lines.
 *
 * Watch for the three things that make it correct:
 *   - the listener is polled alongside the clients
 *   - a disconnect compacts the array rather than leaving a hole
 *   - removing a slot must not skip the entry that moves into it
 */
constexpr int MAX_CLIENTS = 8;

typedef struct {
    struct pollfd fds[MAX_CLIENTS + 1];   /* slot 0 is the listener */
    int           count;                  /* including the listener */
    int           served;
    int           disconnects;
} Loop;

static void loop_add(Loop *l, int fd)
{
    if (l->count > MAX_CLIENTS) { close(fd); return; }
    set_nonblocking(fd);
    l->fds[l->count++] = (struct pollfd){ .fd = fd, .events = POLLIN };
}

static void loop_remove(Loop *l, int index)
{
    close(l->fds[index].fd);
    l->fds[index] = l->fds[l->count - 1];   /* move the last one down */
    l->count--;
    l->disconnects++;
}

/* One turn of the loop. Returns false when the listener has gone away. */
static bool loop_step(Loop *l, int timeout_ms)
{
    int ready = poll(l->fds, (nfds_t)l->count, timeout_ms);
    if (ready <= 0) return true;

    /* Accept first, so a new client is served this turn. */
    if (l->fds[0].revents & POLLIN) {
        int conn = accept(l->fds[0].fd, nullptr, nullptr);
        if (conn >= 0) loop_add(l, conn);
    }

    /* Walk backwards so removing a slot cannot skip an unexamined one. */
    for (int i = l->count - 1; i >= 1; i--) {
        if (!(l->fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;

        char    buf[256];
        ssize_t n = read(l->fds[i].fd, buf, sizeof buf);

        if (n > 0) {
            ssize_t sent = write(l->fds[i].fd, buf, (size_t)n);
            (void)sent;
            l->served++;
        } else if (n == 0) {
            loop_remove(l, i);            /* clean disconnect */
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            loop_remove(l, i);            /* a real error */
        }
    }
    return true;
}

/*
 * The same helpers as about_sockets. They are `static` there, so that copy is
 * invisible here — internal linkage means every translation unit needs its
 * own. Sharing them would mean a header and a third .c file.
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

    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) { close(fd); return -1; }
    return fd;
}

KOAN(assembling_a_single_threaded_event_loop)
{
    signal(SIGPIPE, SIG_IGN);

    uint16_t port = 0;
    int      listener = listen_on_any_port(&port);
    KOAN_TRUE(listener >= 0);
    set_nonblocking(listener);

    Loop l = { .count = 1 };
    l.fds[0] = (struct pollfd){ .fd = listener, .events = POLLIN };

    /* Three clients connect before the loop runs at all. */
    int c1 = connect_to(port), c2 = connect_to(port), c3 = connect_to(port);
    KOAN_TRUE(c1 >= 0 && c2 >= 0 && c3 >= 0);

    /* Each turn accepts at most one, so three turns take all three. */
    for (int i = 0; i < 3; i++) loop_step(&l, 50);
    KOAN_EQ_INT(__, l.count);        /* listener + three clients */

    /* All three speak at once; one thread answers all of them. */
    write(c1, "aa", 2);
    write(c2, "bb", 2);
    write(c3, "cc", 2);

    for (int i = 0; i < 4; i++) loop_step(&l, 50);
    KOAN_EQ_INT(__, l.served);

    /* Each client gets back its own bytes, not another client's — the
     * property that makes this a server rather than a broadcast. Drain all
     * three, or the leftovers turn up in a later read. */
    char buf[16] = {};
    KOAN_EQ_INT(__, (int)read(c1, buf, sizeof buf));
    KOAN_EQ_STR(__STR, buf);

    memset(buf, 0, sizeof buf);
    KOAN_EQ_INT(__, (int)read(c2, buf, sizeof buf));
    KOAN_EQ_STR(__STR, buf);

    memset(buf, 0, sizeof buf);
    KOAN_EQ_INT(__, (int)read(c3, buf, sizeof buf));
    KOAN_EQ_STR(__STR, buf);

    /* A client hanging up is noticed and its slot reclaimed. */
    close(c2);
    for (int i = 0; i < 4; i++) loop_step(&l, 50);
    KOAN_EQ_INT(__, l.disconnects);
    KOAN_EQ_INT(__, l.count);

    /* The survivors are unaffected — this is the property the compaction
     * has to preserve. */
    memset(buf, 0, sizeof buf);
    write(c3, "zz", 2);
    for (int i = 0; i < 4; i++) loop_step(&l, 50);
    KOAN_EQ_INT(__, (int)read(c3, buf, sizeof buf));
    KOAN_EQ_STR(__STR, buf);

    close(c1); close(c3);
    for (int i = 0; i < 4; i++) loop_step(&l, 50);
    KOAN_EQ_INT(__, l.disconnects);
    KOAN_EQ_INT(__, l.count);        /* only the listener remains */

    close(listener);
}

KOAN_LESSON(lesson_about_event_loops, "About Event Loops",
    KOAN_CASE(a_non_blocking_read_returns_eagain),
    KOAN_CASE(poll_reports_which_descriptors_are_ready),
    KOAN_CASE(the_timeout_distinguishes_three_outcomes),
    KOAN_CASE(a_closed_peer_shows_as_readable),
    KOAN_CASE(a_stale_descriptor_gives_pollnval),
    KOAN_CASE(pollout_reports_room_to_write),
    KOAN_CASE(assembling_a_single_threaded_event_loop)
);
