/*
 * koanhttpd — the capstone.
 *
 * A small but genuine HTTP/1.1 server: it listens on a TCP socket, parses
 * requests, serves static files, executes CGI programs, and shuts down
 * cleanly when interrupted. Nothing here is pseudocode and nothing is
 * simulated.
 *
 * Every technique in this file came from a koan you have already solved:
 *
 *   bounded string building ......... about_strings
 *   ownership and cleanup paths ..... about_dynamic_memory
 *   function-pointer dispatch ....... about_function_pointers
 *   X-macro status table ............ about_preprocessor
 *   tagged structs and wire formats . about_structs
 *   fork, exec, dup2, pipes ......... about_processes
 *   sigaction and the volatile flag . about_processes
 *   a thread pool over a queue ...... about_threads
 *
 * Build and run:
 *
 *     make -C projects
 *     ./projects/build/koanhttpd --port 8080 --root projects/webserver/www
 *     curl -v http://127.0.0.1:8080/
 *     curl http://127.0.0.1:8080/cgi-bin/env.cgi
 *
 * Security note: this is a teaching server. It refuses path traversal and
 * bounds every buffer, but it has not been audited and should not face the
 * public internet.
 */
#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strncasecmp */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Status codes — one X-macro list, three uses (about_preprocessor)    */
/* ------------------------------------------------------------------ */

#define HTTP_STATUS_LIST                                  \
    X(OK,           200, "OK")                            \
    X(BAD_REQUEST,  400, "Bad Request")                   \
    X(FORBIDDEN,    403, "Forbidden")                     \
    X(NOT_FOUND,    404, "Not Found")                     \
    X(NOT_ALLOWED,  405, "Method Not Allowed")            \
    X(TOO_LARGE,    413, "Content Too Large")             \
    X(SERVER_ERROR, 500, "Internal Server Error")         \
    X(UNAVAILABLE,  503, "Service Unavailable")

#define X(name, code, text) HTTP_##name = code,
enum HttpStatus { HTTP_STATUS_LIST };
#undef X

static const char *status_text(int code)
{
#define X(name, code_, text) case code_: return text;
    switch (code) { HTTP_STATUS_LIST }
#undef X
    return "Unknown";
}

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

constexpr size_t MAX_REQUEST   = 8192;
constexpr size_t MAX_PATH      = 1024;
constexpr int    WORKER_COUNT  = 4;
constexpr int    QUEUE_CAP     = 64;
constexpr int    BACKLOG       = 64;

typedef struct {
    int         port;
    const char *root;
    const char *cgi_dir;
    bool        verbose;
} Config;

static Config config = {
    .port    = 8080,
    .root    = "projects/webserver/www",
    .cgi_dir = "/cgi-bin/",
    .verbose = false,
};

/* Set from a signal handler, so it may only be sig_atomic_t (about_processes). */
static volatile sig_atomic_t shutting_down = 0;

static void on_signal(int sig)
{
    (void)sig;
    shutting_down = 1;
}

/* ------------------------------------------------------------------ */
/* Request                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    char method[8];
    char target[MAX_PATH];
    char query[MAX_PATH];
    char version[16];
    bool keep_alive;
} Request;

/*
 * Case-insensitive search for `value` on a header line beginning with `name`.
 * strcasestr is a BSD/GNU extension rather than POSIX, so we do it by hand —
 * which is also a reminder that HTTP header names are case-insensitive.
 */
static bool header_contains(const char *headers, const char *name,
                            const char *value)
{
    for (const char *line = headers; line && *line; ) {
        const char *end = strstr(line, "\r\n");
        size_t      len = end ? (size_t)(end - line) : strlen(line);

        if (len > 0 && strncasecmp(line, name, strlen(name)) == 0) {
            char lowered[256];
            size_t n = len < sizeof lowered - 1 ? len : sizeof lowered - 1;
            for (size_t i = 0; i < n; i++)
                lowered[i] = (char)tolower((unsigned char)line[i]);
            lowered[n] = '\0';
            if (strstr(lowered, value)) return true;
        }

        if (!end) break;
        line = end + 2;
    }
    return false;
}

/*
 * Parse the request line and just enough of the headers to decide about
 * keep-alive. Returns an HTTP status: 200 when the request is usable.
 *
 * This is deliberately strict. A parser that guesses is a parser that can be
 * fooled, and being fooled is how servers get owned.
 */
static int parse_request(const char *raw, size_t len, Request *out)
{
    *out = (Request){ .keep_alive = false };

    const char *line_end = memchr(raw, '\r', len);
    if (!line_end) return HTTP_BAD_REQUEST;

    size_t line_len = (size_t)(line_end - raw);
    if (line_len < 5 || line_len > MAX_PATH) return HTTP_BAD_REQUEST;

    /* METHOD SP TARGET SP VERSION */
    const char *sp1 = memchr(raw, ' ', line_len);
    if (!sp1) return HTTP_BAD_REQUEST;

    size_t method_len = (size_t)(sp1 - raw);
    if (method_len == 0 || method_len >= sizeof out->method)
        return HTTP_BAD_REQUEST;
    memcpy(out->method, raw, method_len);
    out->method[method_len] = '\0';

    const char *target_start = sp1 + 1;
    const char *sp2 = memchr(target_start, ' ',
                             line_len - (size_t)(target_start - raw));
    if (!sp2) return HTTP_BAD_REQUEST;

    size_t target_len = (size_t)(sp2 - target_start);
    if (target_len == 0 || target_len >= sizeof out->target)
        return HTTP_BAD_REQUEST;
    memcpy(out->target, target_start, target_len);
    out->target[target_len] = '\0';

    size_t version_len = line_len - (size_t)(sp2 + 1 - raw);
    if (version_len >= sizeof out->version) return HTTP_BAD_REQUEST;
    memcpy(out->version, sp2 + 1, version_len);
    out->version[version_len] = '\0';

    /* Split the query string off the target. */
    char *q = strchr(out->target, '?');
    if (q) {
        *q = '\0';
        snprintf(out->query, sizeof out->query, "%s", q + 1);
    }

    /* HTTP/1.1 keeps the connection alive unless told otherwise. */
    out->keep_alive = (strcmp(out->version, "HTTP/1.1") == 0);
    if (header_contains(raw, "connection:", "close")) out->keep_alive = false;

    return HTTP_OK;
}

/*
 * Decode %XX escapes in place. Returns false on a malformed escape rather
 * than silently producing a different path — the difference matters, because
 * "%2e%2e" is how traversal attempts are smuggled past naive checks.
 */
static int hex_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool percent_decode(char *s)
{
    char *w = s;
    for (char *r = s; *r; ) {
        if (*r == '%') {
            int hi = hex_value((unsigned char)r[1]);
            int lo = hi < 0 ? -1 : hex_value((unsigned char)r[2]);
            if (lo < 0) return false;
            *w++ = (char)(hi * 16 + lo);
            r += 3;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    return true;
}

/*
 * Resolve a request target to a path inside the document root, or refuse.
 *
 * The check is done on the *resolved* path via realpath, because textual
 * checks for ".." are famously easy to defeat. If the resolved path does not
 * start with the resolved root, it is rejected — no exceptions.
 */
static bool resolve_path(const char *root, const char *target,
                         char *out, size_t cap)
{
    char joined[MAX_PATH * 2];
    snprintf(joined, sizeof joined, "%s%s", root, target);

    /*
     * realpath's two-argument form requires a buffer of at least PATH_MAX
     * bytes, and PATH_MAX is not a number you may assume: it is 1024 on macOS
     * and 4096 on Linux, and on some systems it is not defined at all. Handing
     * it anything smaller is a genuine buffer overflow — glibc's fortify
     * checks catch it at runtime and abort the process.
     *
     * Passing null asks realpath to allocate a correctly sized buffer itself.
     * That is POSIX.1-2008, it removes the assumption entirely, and the only
     * cost is remembering to free on every path out.
     */
    char *real_root   = realpath(root, nullptr);
    char *real_target = realpath(joined, nullptr);
    bool  ok = false;

    if (real_root && real_target) {
        size_t root_len = strlen(real_root);

        /* The resolved target must sit inside the resolved root, and the
         * boundary must fall on a separator — otherwise a root of /var/www
         * would also match /var/www-evil. */
        if (strncmp(real_target, real_root, root_len) == 0 &&
            (real_target[root_len] == '\0' || real_target[root_len] == '/') &&
            strlen(real_target) < cap) {
            snprintf(out, cap, "%s", real_target);
            ok = true;
        }
    }

    free(real_root);
    free(real_target);
    return ok;
}

static const char *content_type_for(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    static const struct { const char *ext, *type; } TYPES[] = {
        { ".html", "text/html; charset=utf-8" },
        { ".htm",  "text/html; charset=utf-8" },
        { ".css",  "text/css; charset=utf-8" },
        { ".js",   "application/javascript" },
        { ".json", "application/json" },
        { ".txt",  "text/plain; charset=utf-8" },
        { ".png",  "image/png" },
        { ".jpg",  "image/jpeg" },
        { ".svg",  "image/svg+xml" },
        { ".ico",  "image/x-icon" },
    };

    for (size_t i = 0; i < sizeof TYPES / sizeof *TYPES; i++)
        if (strcmp(dot, TYPES[i].ext) == 0) return TYPES[i].type;

    return "application/octet-stream";
}

/* ------------------------------------------------------------------ */
/* Writing responses                                                   */
/* ------------------------------------------------------------------ */

/* write() may write fewer bytes than asked. Loop until it is all gone. */
static bool write_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, data + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += (size_t)n;
    }
    return true;
}

static void http_date(char *buf, size_t cap)
{
    time_t    now = time(nullptr);
    struct tm gmt;
    gmtime_r(&now, &gmt);
    strftime(buf, cap, "%a, %d %b %Y %H:%M:%S GMT", &gmt);
}

static bool send_response(int fd, int status, const char *type,
                          const char *body, size_t body_len, bool keep_alive)
{
    char date[64];
    http_date(date, sizeof date);

    char head[512];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %d %s\r\n"
                     "Date: %s\r\n"
                     "Server: koanhttpd\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: %s\r\n"
                     "\r\n",
                     status, status_text(status), date, type, body_len,
                     keep_alive ? "keep-alive" : "close");

    if (n < 0 || (size_t)n >= sizeof head) return false;
    if (!write_all(fd, head, (size_t)n)) return false;
    return body_len == 0 || write_all(fd, body, body_len);
}

static void send_error(int fd, int status, bool keep_alive)
{
    char body[256];
    int  n = snprintf(body, sizeof body,
                      "<!doctype html><html><head><title>%d %s</title></head>"
                      "<body><h1>%d %s</h1><hr><p>koanhttpd</p></body></html>",
                      status, status_text(status), status, status_text(status));
    send_response(fd, status, "text/html; charset=utf-8", body,
                  n < 0 ? 0 : (size_t)n, keep_alive);
}

/* ------------------------------------------------------------------ */
/* Static files                                                        */
/* ------------------------------------------------------------------ */

static void serve_file(int fd, const char *path, bool head_only, bool keep_alive)
{
    struct stat st;
    if (stat(path, &st) != 0) { send_error(fd, HTTP_NOT_FOUND, keep_alive); return; }

    /* A directory means its index.html, if there is one. */
    char resolved[MAX_PATH];
    snprintf(resolved, sizeof resolved, "%s", path);
    if (S_ISDIR(st.st_mode)) {
        char index[MAX_PATH];

        /*
         * Check for truncation rather than assuming it away. snprintf reports
         * what it *wanted* to write, so a return >= the buffer size means the
         * path was cut short — and a silently shortened path is a path to the
         * wrong file (about_strings).
         */
        int n = snprintf(index, sizeof index, "%s/index.html", path);
        if (n < 0 || (size_t)n >= sizeof index) {
            send_error(fd, HTTP_SERVER_ERROR, keep_alive);
            return;
        }

        if (stat(index, &st) != 0 || !S_ISREG(st.st_mode)) {
            send_error(fd, HTTP_FORBIDDEN, keep_alive);
            return;
        }
        snprintf(resolved, sizeof resolved, "%s", index);
    }

    if (!S_ISREG(st.st_mode)) { send_error(fd, HTTP_FORBIDDEN, keep_alive); return; }

    int file = open(resolved, O_RDONLY);
    if (file < 0) { send_error(fd, HTTP_FORBIDDEN, keep_alive); return; }

    char date[64];
    http_date(date, sizeof date);

    char head[512];
    int  hn = snprintf(head, sizeof head,
                       "HTTP/1.1 200 OK\r\n"
                       "Date: %s\r\n"
                       "Server: koanhttpd\r\n"
                       "Content-Type: %s\r\n"
                       "Content-Length: %lld\r\n"
                       "Connection: %s\r\n"
                       "\r\n",
                       date, content_type_for(resolved),
                       (long long)st.st_size,
                       keep_alive ? "keep-alive" : "close");

    if (hn > 0 && write_all(fd, head, (size_t)hn) && !head_only) {
        char    buf[8192];
        ssize_t n;
        while ((n = read(file, buf, sizeof buf)) > 0)
            if (!write_all(fd, buf, (size_t)n)) break;
    }
    close(file);
}

/* ------------------------------------------------------------------ */
/* CGI                                                                 */
/* ------------------------------------------------------------------ */

/*
 * Run a CGI program and relay what it writes.
 *
 * This is fork + dup2 + exec + pipe from about_processes, with the CGI
 * environment variables added. The child's stdout becomes the pipe; the
 * parent reads it and forwards it to the socket.
 *
 * CGI scripts emit their own headers followed by a blank line, so the status
 * line is all we prepend.
 */
static void serve_cgi(int fd, const char *script, const Request *req,
                      const char *peer)
{
    int fds[2];
    if (pipe(fds) != 0) { send_error(fd, HTTP_SERVER_ERROR, false); return; }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]); close(fds[1]);
        send_error(fd, HTTP_SERVER_ERROR, false);
        return;
    }

    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);

        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
        setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
        setenv("SERVER_SOFTWARE", "koanhttpd", 1);
        setenv("REQUEST_METHOD", req->method, 1);
        setenv("SCRIPT_NAME", req->target, 1);
        setenv("QUERY_STRING", req->query, 1);
        setenv("REMOTE_ADDR", peer, 1);
        setenv("CONTENT_LENGTH", "0", 1);

        execl(script, script, (char *)nullptr);
        /* exec only returns on failure. */
        fprintf(stdout, "Content-Type: text/plain\r\n\r\nCGI exec failed\n");
        fflush(stdout);
        _exit(127);
    }

    close(fds[1]);

    /* Status line first; the script supplies the rest of the headers. */
    const char *status_line = "HTTP/1.1 200 OK\r\nServer: koanhttpd\r\n"
                              "Connection: close\r\n";
    write_all(fd, status_line, strlen(status_line));

    char    buf[4096];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof buf)) > 0)
        if (!write_all(fd, buf, (size_t)n)) break;

    close(fds[0]);
    waitpid(pid, nullptr, 0);
}

/* ------------------------------------------------------------------ */
/* Handling one connection                                             */
/* ------------------------------------------------------------------ */

static void handle_connection(int fd, const char *peer)
{
    char   raw[MAX_REQUEST];
    size_t used = 0;

    /* Read until we have the end of the headers, or run out of room. */
    while (used < sizeof raw - 1) {
        ssize_t n = read(fd, raw + used, sizeof raw - 1 - used);
        if (n < 0) { if (errno == EINTR) continue; return; }
        if (n == 0) break;
        used += (size_t)n;
        raw[used] = '\0';
        if (strstr(raw, "\r\n\r\n")) break;
    }

    if (used == 0) return;
    raw[used] = '\0';

    if (used >= sizeof raw - 1 && !strstr(raw, "\r\n\r\n")) {
        send_error(fd, HTTP_TOO_LARGE, false);
        return;
    }

    Request req;
    int status = parse_request(raw, used, &req);
    if (status != HTTP_OK) { send_error(fd, status, false); return; }

    bool is_get  = strcmp(req.method, "GET") == 0;
    bool is_head = strcmp(req.method, "HEAD") == 0;
    if (!is_get && !is_head) { send_error(fd, HTTP_NOT_ALLOWED, false); return; }

    if (!percent_decode(req.target)) {
        send_error(fd, HTTP_BAD_REQUEST, false);
        return;
    }

    if (config.verbose)
        fprintf(stderr, "%s %s %s\n", peer, req.method, req.target);

    /* CGI is dispatched on a path prefix, before the static file lookup. */
    bool is_cgi = strncmp(req.target, config.cgi_dir,
                          strlen(config.cgi_dir)) == 0;

    char path[MAX_PATH];
    if (!resolve_path(config.root, req.target, path, sizeof path)) {
        send_error(fd, HTTP_NOT_FOUND, false);
        return;
    }

    if (is_cgi) {
        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) ||
            !(st.st_mode & S_IXUSR)) {
            send_error(fd, HTTP_FORBIDDEN, false);
            return;
        }
        serve_cgi(fd, path, &req, peer);
        return;
    }

    serve_file(fd, path, is_head, false);
}

/* ------------------------------------------------------------------ */
/* Thread pool over a bounded queue (about_threads)                    */
/* ------------------------------------------------------------------ */

typedef struct {
    int  fd;
    char peer[INET_ADDRSTRLEN];
} Job;

static struct {
    Job             slots[QUEUE_CAP];
    size_t          head, tail, count;
    bool            closed;
    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} queue;

static void queue_setup(void)
{
    queue.head = queue.tail = queue.count = 0;
    queue.closed = false;
    pthread_mutex_init(&queue.lock, nullptr);
    pthread_cond_init(&queue.not_full, nullptr);
    pthread_cond_init(&queue.not_empty, nullptr);
}

static bool queue_put(Job job)
{
    pthread_mutex_lock(&queue.lock);
    while (queue.count == QUEUE_CAP && !queue.closed)
        pthread_cond_wait(&queue.not_full, &queue.lock);

    if (queue.closed) { pthread_mutex_unlock(&queue.lock); return false; }

    queue.slots[queue.tail] = job;
    queue.tail = (queue.tail + 1) % QUEUE_CAP;
    queue.count++;
    pthread_cond_signal(&queue.not_empty);
    pthread_mutex_unlock(&queue.lock);
    return true;
}

static bool queue_take(Job *out)
{
    pthread_mutex_lock(&queue.lock);
    while (queue.count == 0 && !queue.closed)
        pthread_cond_wait(&queue.not_empty, &queue.lock);

    if (queue.count == 0) { pthread_mutex_unlock(&queue.lock); return false; }

    *out = queue.slots[queue.head];
    queue.head = (queue.head + 1) % QUEUE_CAP;
    queue.count--;
    pthread_cond_signal(&queue.not_full);
    pthread_mutex_unlock(&queue.lock);
    return true;
}

static void queue_shutdown(void)
{
    pthread_mutex_lock(&queue.lock);
    queue.closed = true;
    pthread_cond_broadcast(&queue.not_empty);
    pthread_cond_broadcast(&queue.not_full);
    pthread_mutex_unlock(&queue.lock);
}

static void *worker(void *arg)
{
    (void)arg;
    Job job;
    while (queue_take(&job)) {
        handle_connection(job.fd, job.peer);
        close(job.fd);
    }
    return nullptr;
}

/* ------------------------------------------------------------------ */
/* Listening                                                           */
/* ------------------------------------------------------------------ */

static int open_listener(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    /*
     * Bind to the loopback address only. Exposing a teaching server on every
     * interface would be a poor default; change this deliberately if you mean
     * to. inet_pton is used rather than INADDR_LOOPBACK because the latter is
     * hidden by strict POSIX feature-test macros on some platforms.
     */
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port   = htons((uint16_t)port),
    };
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, BACKLOG) != 0) { close(fd); return -1; }
    return fd;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--port N] [--root DIR] [--verbose]\n\n"
            "  --port N      port to listen on (default 8080)\n"
            "  --root DIR    document root (default projects/webserver/www)\n"
            "  --verbose     log each request\n",
            argv0);
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            config.root = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    /*
     * A CGI child that dies leaves us writing to a closed pipe, which raises
     * SIGPIPE and would kill the server. Ignore it and handle the write error
     * instead — this single line is the difference between a server that
     * stays up and one that does not.
     */
    signal(SIGPIPE, SIG_IGN);

    struct sigaction sa = { 0 };
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    int listener = open_listener(config.port);
    if (listener < 0) {
        fprintf(stderr, "koanhttpd: cannot listen on port %d: %s\n",
                config.port, strerror(errno));
        return 1;
    }

    queue_setup();

    pthread_t workers[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; i++)
        pthread_create(&workers[i], nullptr, worker, nullptr);

    fprintf(stderr, "koanhttpd listening on http://127.0.0.1:%d/ "
                    "(root %s, %d workers)\n",
            config.port, config.root, WORKER_COUNT);

    while (!shutting_down) {
        struct sockaddr_in peer;
        socklen_t          peer_len = sizeof peer;

        int client = accept(listener, (struct sockaddr *)&peer, &peer_len);
        if (client < 0) {
            if (errno == EINTR) continue;   /* a signal arrived; re-check */
            break;
        }

        Job job = { .fd = client };
        inet_ntop(AF_INET, &peer.sin_addr, job.peer, sizeof job.peer);

        if (!queue_put(job)) { close(client); break; }
    }

    fprintf(stderr, "\nkoanhttpd shutting down\n");
    close(listener);
    queue_shutdown();
    for (int i = 0; i < WORKER_COUNT; i++) pthread_join(workers[i], nullptr);

    return 0;
}
