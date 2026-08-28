/*
 * About Streams
 *
 * about_printf covered formatting and about_file_io covered reading and
 * writing. This lesson is about the stream itself: the FILE object, the
 * buffer behind it, and the error state it carries.
 *
 * A FILE is a handle plus a buffer plus a position plus two sticky flags
 * (end-of-file and error). Most stdio confusion comes from forgetting one of
 * those four parts exists.
 */
#include "koan.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *scratch(void)
{
    FILE *f = tmpfile();
    if (!f) KOAN_FAIL("could not create a temporary file");
    return f;
}

/*
 * The three standard streams exist before main runs and need no opening.
 * They are ordinary FILE pointers, which is why every f-function works on them.
 */
KOAN(the_three_standard_streams_are_ordinary_files)
{
    KOAN_TRUE(stdin != nullptr && stdout != nullptr && stderr != nullptr);
    KOAN_TRUE(stdout != stderr);

    /* They are FILE *, so every f-function accepts them. Writing an empty
     * string writes nothing and reports zero characters. */
    KOAN_EQ_INT(/*__*/ 0, fprintf(stdout, "%s", ""));

    /* FOPEN_MAX is the guaranteed minimum number of simultaneously open
     * streams, and the standard requires it to be at least eight — three of
     * which the implementation has already spent on the streams above. */
    KOAN_TRUE(FOPEN_MAX >= 8);

    /* EOF is negative so it cannot collide with any unsigned char value. */
    KOAN_TRUE(EOF < 0);
}

/*
 * Buffering decides *when* your bytes leave the program. There are three
 * modes, and setvbuf chooses between them — but only before any I/O happens
 * on that stream.
 *
 *   _IOFBF  fully buffered   flushed when the buffer fills   (files, pipes)
 *   _IOLBF  line buffered    flushed at each newline         (terminals)
 *   _IONBF  unbuffered       written immediately             (stderr)
 */
KOAN(buffering_decides_when_bytes_leave)
{
    const char *path = "koan_stdio_buffering.tmp";

    FILE *w = fopen(path, "w");
    KOAN_TRUE(w != nullptr);
    KOAN_EQ_INT(/*__*/ 0, setvbuf(w, nullptr, _IOFBF, 4096));

    fputs("held", w);

    /* Fully buffered, so nothing has reached the file yet. */
    FILE *peek = fopen(path, "r");
    char  seen[32] = {};
    KOAN_EQ_SZ(/*__SZ*/ 0, fread(seen, 1, sizeof seen - 1, peek));
    fclose(peek);

    fclose(w);                  /* fclose flushes before it closes */

    FILE  *after = fopen(path, "r");
    size_t flushed = fread(seen, 1, sizeof seen - 1, after);
    fclose(after);

    KOAN_EQ_SZ(/*__SZ*/ 4, flushed);
    KOAN_EQ_STR(/*__STR*/ "held", seen);

    remove(path);
}

/*
 * Unbuffered output reaches the file immediately, which is why stderr is
 * unbuffered: a diagnostic written just before a crash must survive it.
 */
KOAN(unbuffered_streams_write_through)
{
    const char *path = "koan_stdio_unbuffered.tmp";

    FILE *w = fopen(path, "w");
    setvbuf(w, nullptr, _IONBF, 0);
    fputs("immediate", w);

    /* No flush, yet it is already there. */
    FILE  *peek = fopen(path, "r");
    char   seen[32] = {};
    size_t got = fread(seen, 1, sizeof seen - 1, peek);
    fclose(peek);

    KOAN_EQ_SZ(/*__SZ*/ 9, got);
    KOAN_EQ_STR(/*__STR*/ "immediate", seen);

    fclose(w);
    remove(path);
}

/*
 * The end-of-file and error flags are *sticky*: once set they stay set until
 * cleared. A stream that has hit EOF will keep reporting EOF even after you
 * seek back, unless you call clearerr — or use a function that clears it.
 */
KOAN(stream_flags_are_sticky)
{
    FILE *f = scratch();
    fputs("ab", f);
    rewind(f);

    while (fgetc(f) != EOF) { }
    KOAN_EQ_INT(/*__*/ 1, feof(f) != 0);
    KOAN_EQ_INT(/*__*/ 0, ferror(f));

    /* clearerr resets both flags. */
    clearerr(f);
    KOAN_EQ_INT(/*__*/ 0, feof(f));

    /* rewind is documented to clear the error flag too, which is the one
     * convenience the interface offers. */
    while (fgetc(f) != EOF) { }
    rewind(f);
    KOAN_EQ_INT(/*__*/ 0, feof(f));
    KOAN_EQ_INT(/*__*/ 'a', fgetc(f));

    fclose(f);
}

/*
 * Writing to a stream opened for reading fails and sets the error flag rather
 * than crashing. The failure is silent unless you look — this is why long
 * chains of unchecked fputs are a bad idea.
 */
KOAN(a_failed_write_sets_the_error_flag)
{
    const char *path = "koan_stdio_readonly.tmp";

    FILE *make = fopen(path, "w");
    fputs("x", make);
    fclose(make);

    FILE *r = fopen(path, "r");
    KOAN_TRUE(r != nullptr);

    /* This cannot succeed, and does not say so in its return value alone. */
    fputs("nope", r);

    KOAN_EQ_INT(/*__*/ 1, ferror(r) != 0);

    clearerr(r);
    KOAN_EQ_INT(/*__*/ 0, ferror(r));

    fclose(r);
    remove(path);
}

/*
 * ungetc pushes one character back so the next read returns it. Exactly one
 * is guaranteed; more is unspecified. It is what a hand-written parser uses
 * for one token of lookahead.
 */
KOAN(ungetc_gives_one_character_of_lookahead)
{
    FILE *f = scratch();
    fputs("42x", f);
    rewind(f);

    /* Read digits until something that is not one, then put it back. */
    int  value = 0, c;
    while ((c = fgetc(f)) != EOF && c >= '0' && c <= '9')
        value = value * 10 + (c - '0');

    KOAN_EQ_INT(/*__*/ 42, value);
    KOAN_EQ_INT(/*__*/ 'x', c);

    ungetc(c, f);
    KOAN_EQ_INT(/*__*/ 'x', fgetc(f));      /* the same character again */

    fclose(f);
}

/*
 * fgetpos/fsetpos save and restore a position opaquely. Unlike ftell they
 * work for files too large for a long, and for text streams where a byte
 * offset is not meaningful.
 */
KOAN(fgetpos_saves_an_opaque_position)
{
    FILE *f = scratch();
    fputs("abcdefgh", f);
    rewind(f);

    fgetc(f); fgetc(f);                     /* consume "ab" */

    fpos_t mark;
    KOAN_EQ_INT(/*__*/ 0, fgetpos(f, &mark));
    KOAN_EQ_INT(/*__*/ 'c', fgetc(f));

    fseek(f, 0, SEEK_END);
    KOAN_EQ_INT(/*__*/ EOF, fgetc(f));

    /* Back to exactly where the mark was taken. */
    clearerr(f);
    KOAN_EQ_INT(/*__*/ 0, fsetpos(f, &mark));
    KOAN_EQ_INT(/*__*/ 'c', fgetc(f));

    fclose(f);
}

/*
 * remove and rename are the two filesystem operations in ISO C. Both report
 * failure by returning non-zero, and both set errno.
 */
KOAN(remove_and_rename_are_standard_c)
{
    const char *first  = "koan_stdio_first.tmp";
    const char *second = "koan_stdio_second.tmp";

    FILE *f = fopen(first, "w");
    fputs("contents", f);
    fclose(f);

    KOAN_EQ_INT(/*__*/ 0, rename(first, second));

    /* The old name is gone... */
    KOAN_TRUE(fopen(first, "r") == nullptr);

    /* ...and the contents moved with the name. */
    FILE  *check = fopen(second, "r");
    char   buf[32] = {};
    size_t moved = fread(buf, 1, sizeof buf - 1, check);
    fclose(check);

    KOAN_EQ_SZ(/*__SZ*/ 8, moved);
    KOAN_EQ_STR(/*__STR*/ "contents", buf);

    KOAN_EQ_INT(/*__*/ 0, remove(second));

    /* Removing what is not there fails rather than succeeding quietly. */
    KOAN_TRUE(remove(second) != 0);
}

/*
 * Bringing it together.
 *
 * A line reader that grows its own buffer, so no line is ever too long. This
 * is the function C is most often accused of lacking; it is thirty lines.
 *
 * It must distinguish three outcomes — a line, end of input, and an
 * allocation failure — and it must not lose the final line of a file that
 * does not end in a newline.
 */
typedef enum { LINE_OK, LINE_EOF, LINE_NOMEM } LineResult;

static LineResult read_line(FILE *f, char **buf, size_t *cap, size_t *len)
{
    *len = 0;
    int c;

    for (;;) {
        c = fgetc(f);
        if (c == EOF) break;

        /* Always keep room for this byte and a terminator. */
        if (*len + 2 > *cap) {
            size_t next = *cap ? *cap * 2 : 64;
            char  *grown = realloc(*buf, next);
            if (!grown) return LINE_NOMEM;
            *buf = grown;
            *cap = next;
        }

        if (c == '\n') break;
        (*buf)[(*len)++] = (char)c;
    }

    /* EOF with nothing read means there was no final line. */
    if (c == EOF && *len == 0) return LINE_EOF;

    (*buf)[*len] = '\0';
    return LINE_OK;
}

KOAN(assembling_a_growing_line_reader)
{
    FILE *f = scratch();

    /* A short line, a very long line, an empty line, and no trailing newline. */
    fputs("short\n", f);
    for (int i = 0; i < 300; i++) fputc('x', f);
    fputs("\n\nlast", f);
    rewind(f);

    char  *buf = nullptr;
    size_t cap = 0, len = 0;

    KOAN_EQ_INT(/*__*/ LINE_OK, read_line(f, &buf, &cap, &len));
    KOAN_EQ_STR(/*__STR*/ "short", buf);
    KOAN_EQ_SZ(/*__SZ*/ 5, len);

    /* The buffer grew to hold the long line; nothing was truncated. */
    KOAN_EQ_INT(/*__*/ LINE_OK, read_line(f, &buf, &cap, &len));
    KOAN_EQ_SZ(/*__SZ*/ 300, len);
    KOAN_TRUE(cap > 300);
    KOAN_EQ_CHR(/*__CHR*/ 'x', buf[299]);
    KOAN_EQ_CHR(/*__CHR*/ '\0', buf[300]);

    /* An empty line is a line, not end of input. */
    KOAN_EQ_INT(/*__*/ LINE_OK, read_line(f, &buf, &cap, &len));
    KOAN_EQ_SZ(/*__SZ*/ 0, len);
    KOAN_EQ_STR(/*__STR*/ "", buf);

    /* A final line with no newline is still returned. */
    KOAN_EQ_INT(/*__*/ LINE_OK, read_line(f, &buf, &cap, &len));
    KOAN_EQ_STR(/*__STR*/ "last", buf);

    /* Only now is it end of input. */
    KOAN_EQ_INT(/*__*/ LINE_EOF, read_line(f, &buf, &cap, &len));

    free(buf);
    fclose(f);
}

KOAN_LESSON(lesson_about_stdio, "About Streams",
    KOAN_CASE(the_three_standard_streams_are_ordinary_files),
    KOAN_CASE(buffering_decides_when_bytes_leave),
    KOAN_CASE(unbuffered_streams_write_through),
    KOAN_CASE(stream_flags_are_sticky),
    KOAN_CASE(a_failed_write_sets_the_error_flag),
    KOAN_CASE(ungetc_gives_one_character_of_lookahead),
    KOAN_CASE(fgetpos_saves_an_opaque_position),
    KOAN_CASE(remove_and_rename_are_standard_c),
    KOAN_CASE(assembling_a_growing_line_reader)
);
