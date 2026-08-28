/*
 * About File Descriptors
 *
 * Below <stdio.h> there are no streams and no buffers — only integers naming
 * open files, and read/write moving bytes. Everything a FILE does, it does by
 * calling these.
 *
 * The differences that matter:
 *
 *   FILE *              fd (int)
 *   buffered            unbuffered; every call is a system call
 *   fread returns items read returns BYTES, and may return fewer than asked
 *   ferror/feof         errno, and a return of -1 or 0
 *
 * A short read is not an error. Handling that correctly is most of this
 * lesson, and it is the bug that appears the moment a program meets a socket.
 */
#include "koan.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* A scratch file that removes itself when the koan is done. */
static int scratch_fd(const char *path, const char *contents)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) KOAN_FAIL("could not create %s", path);
    if (contents && *contents) {
        ssize_t n = write(fd, contents, strlen(contents));
        (void)n;
        lseek(fd, 0, SEEK_SET);
    }
    return fd;
}

/*
 * open returns the lowest unused descriptor. 0, 1 and 2 are already taken by
 * stdin, stdout and stderr, so the first file you open is usually 3.
 */
KOAN(descriptors_are_small_integers)
{
    KOAN_EQ_INT(__, STDIN_FILENO);
    KOAN_EQ_INT(__, STDOUT_FILENO);
    KOAN_EQ_INT(__, STDERR_FILENO);

    const char *path = "koan_fd_basic.tmp";
    int fd = scratch_fd(path, "hello");

    KOAN_TRUE(fd > 2);

    /* Closing frees the number, and the next open reuses it. */
    int again = dup(fd);
    KOAN_TRUE(again != fd);
    close(again);

    close(fd);
    unlink(path);

    /* Operating on a closed descriptor fails with EBADF rather than crashing. */
    errno = 0;
    char byte;
    KOAN_EQ_INT(__, (int)read(fd, &byte, 1));
    KOAN_EQ_INT(__, errno);
}

/*
 * The open flags are a bitmask. Exactly one of O_RDONLY/O_WRONLY/O_RDWR, plus
 * any of the modifiers. O_CREAT is the only one that uses the mode argument.
 */
KOAN(open_flags_are_a_bitmask)
{
    const char *path = "koan_fd_flags.tmp";

    /* O_EXCL with O_CREAT fails if the file already exists — the atomic
     * "create only if absent" that a lock file needs. */
    int first = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    KOAN_TRUE(first >= 0);
    close(first);

    errno = 0;
    int second = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    KOAN_EQ_INT(__, second);
    KOAN_EQ_INT(__, errno);

    /* Opening for reading something that does not exist gives ENOENT. */
    errno = 0;
    KOAN_EQ_INT(__, open("koan_fd_absent.tmp", O_RDONLY));
    KOAN_EQ_INT(__, errno);

    unlink(path);
}

/*
 * read returns the number of BYTES read, which may be fewer than requested
 * without anything being wrong. Zero means end of file. Only -1 is an error.
 */
KOAN(read_returns_bytes_and_may_be_short)
{
    const char *path = "koan_fd_read.tmp";
    int fd = scratch_fd(path, "abcdefghij");

    char buf[4];
    ssize_t n = read(fd, buf, sizeof buf);
    KOAN_EQ_INT(__, (int)n);
    KOAN_EQ_INT(__, buf[0]);

    /* Asking for more than remains gives what is there, not an error. */
    char rest[100];
    n = read(fd, rest, sizeof rest);
    KOAN_EQ_INT(__, (int)n);

    /* And then zero, forever, which is how end of file is spelled. */
    KOAN_EQ_INT(__, (int)read(fd, rest, sizeof rest));
    KOAN_EQ_INT(__, (int)read(fd, rest, sizeof rest));

    close(fd);
    unlink(path);
}

/*
 * Because reads and writes are short, every real transfer is a loop. These
 * two functions are the ones to memorise; you will write them again in the
 * socket lesson and they are already in the web server.
 */
static ssize_t read_full(int fd, void *buf, size_t want)
{
    unsigned char *p = buf;
    size_t         got = 0;

    while (got < want) {
        ssize_t n = read(fd, p + got, want - got);
        if (n < 0) {
            if (errno == EINTR) continue;    /* a signal, not a failure */
            return -1;
        }
        if (n == 0) break;                   /* end of file */
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
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

KOAN(short_transfers_require_a_loop)
{
    const char *path = "koan_fd_loop.tmp";
    int fd = scratch_fd(path, nullptr);

    char payload[5000];
    memset(payload, 'x', sizeof payload);

    KOAN_EQ_INT(__, write_full(fd, payload, sizeof payload));
    lseek(fd, 0, SEEK_SET);

    char back[5000] = {};
    KOAN_EQ_INT(__, (int)read_full(fd, back, sizeof back));
    KOAN_EQ_INT(__, memcmp(payload, back, sizeof payload));

    /* At end of file read_full returns what it managed, not an error. */
    KOAN_EQ_INT(__, (int)read_full(fd, back, 10));

    close(fd);
    unlink(path);
}

/*
 * lseek moves the offset and returns the new position, so seeking zero from
 * the end is how you get a file's size without stat.
 */
KOAN(lseek_returns_the_new_offset)
{
    const char *path = "koan_fd_seek.tmp";
    int fd = scratch_fd(path, "0123456789");

    KOAN_EQ_INT(__, (int)lseek(fd, 0, SEEK_END));
    KOAN_EQ_INT(__, (int)lseek(fd, 0, SEEK_SET));
    KOAN_EQ_INT(__, (int)lseek(fd, 3, SEEK_CUR));

    char c;
    read(fd, &c, 1);
    KOAN_EQ_CHR(__CHR, c);

    /* Seeking past the end is legal; it creates a hole when written to. */
    KOAN_EQ_INT(__, (int)lseek(fd, 20, SEEK_SET));
    write_full(fd, "end", 3);
    KOAN_EQ_INT(__, (int)lseek(fd, 0, SEEK_END));

    close(fd);
    unlink(path);
}

/*
 * dup2 makes a descriptor refer to the same open file as another, closing the
 * target first. It is how a shell wires up redirection, and how the web server
 * gives a CGI child its stdout.
 */
KOAN(dup2_redirects_a_descriptor)
{
    const char *path = "koan_fd_dup.tmp";
    int fd = scratch_fd(path, nullptr);

    int saved = dup(STDOUT_FILENO);       /* keep the real stdout */
    KOAN_TRUE(saved > 2);

    dup2(fd, STDOUT_FILENO);              /* stdout now names our file */
    write_full(STDOUT_FILENO, "captured", 8);

    dup2(saved, STDOUT_FILENO);           /* put it back */
    close(saved);

    lseek(fd, 0, SEEK_SET);
    char buf[16] = {};
    read_full(fd, buf, 8);
    KOAN_EQ_STR(__STR, buf);

    /* Both descriptors shared one file offset, because dup2 duplicates the
     * descriptor, not the open file. */
    close(fd);
    unlink(path);
}

/*
 * stat reports a file's metadata. st_mode carries both the type and the
 * permission bits, which is why the S_IS* macros exist.
 */
KOAN(stat_reports_type_and_size)
{
    const char *path = "koan_fd_stat.tmp";
    int fd = scratch_fd(path, "twelve bytes");
    close(fd);

    struct stat st;
    KOAN_EQ_INT(__, stat(path, &st));

    KOAN_EQ_INT(__, (int)st.st_size);
    KOAN_EQ_INT(__, S_ISREG(st.st_mode) != 0);
    KOAN_EQ_INT(__, S_ISDIR(st.st_mode) != 0);

    /* The permission bits are the low nine, and we created it 0600. */
    KOAN_EQ_INT(__, st.st_mode & 0777);

    /* A directory is a different type with the same call. */
    struct stat dir;
    KOAN_EQ_INT(__, stat(".", &dir));
    KOAN_EQ_INT(__, S_ISDIR(dir.st_mode) != 0);

    /* Statting what is not there fails with ENOENT. */
    errno = 0;
    KOAN_EQ_INT(__, stat("koan_fd_absent.tmp", &st));
    KOAN_EQ_INT(__, errno);

    unlink(path);
}

/*
 * A descriptor survives being unlinked: the name goes, the open file stays
 * until the last descriptor closes. This is how a temporary file that cannot
 * be left behind is made, and why deleting a log file does not free the disk
 * space while a process still holds it open.
 */
KOAN(an_unlinked_file_lives_until_its_last_descriptor_closes)
{
    const char *path = "koan_fd_unlinked.tmp";
    int fd = scratch_fd(path, "still here");

    KOAN_EQ_INT(__, unlink(path));

    /* The name is gone... */
    struct stat st;
    KOAN_EQ_INT(__, stat(path, &st));

    /* ...but the bytes are not. */
    char buf[16] = {};
    lseek(fd, 0, SEEK_SET);
    KOAN_EQ_INT(__, (int)read_full(fd, buf, 10));
    KOAN_EQ_STR(__STR, buf);

    close(fd);      /* only now is the storage released */
}

/*
 * Bringing it together.
 *
 * Copy a file the way cp does: open source and destination, loop until the
 * source is exhausted, handle short transfers and EINTR, and unwind every
 * descriptor on every path. Then verify the copy is byte-identical.
 *
 * The `goto` unwind from about_control_flow is doing real work here.
 */
typedef enum { COPY_OK, COPY_NO_SOURCE, COPY_NO_DEST, COPY_IO_ERROR } CopyResult;

static CopyResult copy_file(const char *from, const char *to, size_t *bytes)
{
    CopyResult result = COPY_OK;
    *bytes = 0;

    int in = open(from, O_RDONLY);
    if (in < 0) return COPY_NO_SOURCE;

    int out = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out < 0) { result = COPY_NO_DEST; goto close_in; }

    char buf[4096];
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR) continue;
            result = COPY_IO_ERROR;
            goto close_both;
        }
        if (n == 0) break;

        if (!write_full(out, buf, (size_t)n)) {
            result = COPY_IO_ERROR;
            goto close_both;
        }
        *bytes += (size_t)n;
    }

close_both:
    close(out);
close_in:
    close(in);
    return result;
}

KOAN(assembling_a_file_copier)
{
    const char *src = "koan_copy_src.tmp";
    const char *dst = "koan_copy_dst.tmp";

    /* A payload larger than the copy buffer, so the loop runs several times. */
    int    fd = scratch_fd(src, nullptr);
    char   payload[10000];
    for (size_t i = 0; i < sizeof payload; i++) payload[i] = (char)('a' + i % 26);
    write_full(fd, payload, sizeof payload);
    close(fd);

    size_t     copied = 0;
    CopyResult r = copy_file(src, dst, &copied);

    KOAN_EQ_INT(__, r);
    KOAN_EQ_SZ(__SZ, copied);

    /* Verify byte for byte, which is the only verification worth doing. */
    struct stat a, b;
    stat(src, &a);
    stat(dst, &b);
    KOAN_EQ_INT(__, a.st_size == b.st_size);

    int  check = open(dst, O_RDONLY);
    char back[10000] = {};
    KOAN_EQ_INT(__, (int)read_full(check, back, sizeof back));
    KOAN_EQ_INT(__, memcmp(payload, back, sizeof payload));
    close(check);

    /* A missing source is reported, and no destination is created. */
    unlink(dst);
    KOAN_EQ_INT(__, copy_file("koan_absent.tmp", dst, &copied));
    KOAN_EQ_INT(__, stat(dst, &b));

    /* Copying an empty file succeeds and copies nothing. */
    int empty = scratch_fd("koan_copy_empty.tmp", nullptr);
    close(empty);
    KOAN_EQ_INT(__, copy_file("koan_copy_empty.tmp", dst, &copied));
    KOAN_EQ_SZ(__SZ, copied);

    unlink(src);
    unlink(dst);
    unlink("koan_copy_empty.tmp");
}

KOAN_LESSON(lesson_about_low_level_io, "About File Descriptors",
    KOAN_CASE(descriptors_are_small_integers),
    KOAN_CASE(open_flags_are_a_bitmask),
    KOAN_CASE(read_returns_bytes_and_may_be_short),
    KOAN_CASE(short_transfers_require_a_loop),
    KOAN_CASE(lseek_returns_the_new_offset),
    KOAN_CASE(dup2_redirects_a_descriptor),
    KOAN_CASE(stat_reports_type_and_size),
    KOAN_CASE(an_unlinked_file_lives_until_its_last_descriptor_closes),
    KOAN_CASE(assembling_a_file_copier)
);
