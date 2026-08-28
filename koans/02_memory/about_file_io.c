/*
 * About File I/O
 *
 * <stdio.h> gives you buffered streams. The buffer is the whole point and the
 * whole trap: what you have written is not what is on disk until the buffer is
 * flushed, and a crash in between loses it.
 *
 * These koans write to temporary files and clean up after themselves.
 */
#include "koan.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A scratch file that removes itself. tmpfile() is deleted on close. */
static FILE *scratch(void)
{
    FILE *f = tmpfile();
    if (!f) KOAN_FAIL("could not create a temporary file");
    return f;
}

/*
 * Modes: "r" read, "w" truncate-or-create, "a" append. Adding "+" makes it
 * readable and writable. "w" destroys the file's contents immediately — there
 * is no confirmation.
 */
KOAN(write_then_rewind_to_read)
{
    FILE *f = scratch();

    fputs("hello", f);
    KOAN_EQ_INT(__, (int)ftell(f));

    /* The file position is shared between reading and writing. */
    rewind(f);
    KOAN_EQ_INT(__, (int)ftell(f));

    char buf[16] = {};
    KOAN_TRUE(fgets(buf, sizeof buf, f) != nullptr);
    KOAN_EQ_STR(__STR, buf);

    fclose(f);
}

/*
 * fseek moves the position; SEEK_SET, SEEK_CUR and SEEK_END are the origins.
 * Seeking to the end and asking the position is the classic way to get a
 * file's size.
 */
KOAN(fseek_moves_the_position)
{
    FILE *f = scratch();
    fputs("abcdefghij", f);

    fseek(f, 0, SEEK_END);
    KOAN_EQ_INT(__, (int)ftell(f));      /* the size */

    fseek(f, 3, SEEK_SET);
    KOAN_EQ_INT(__, fgetc(f));          /* index 3 */

    fseek(f, 2, SEEK_CUR);                      /* skip 'e','f' */
    KOAN_EQ_INT(__, fgetc(f));

    fseek(f, -2, SEEK_END);
    KOAN_EQ_INT(__, fgetc(f));

    fclose(f);
}

/*
 * fread and fwrite move blocks, and return the count of *elements*, not bytes.
 * A short return means end-of-file or an error, and only feof/ferror can tell
 * you which.
 */
KOAN(fread_returns_elements_not_bytes)
{
    FILE *f = scratch();

    int out[4] = { 10, 20, 30, 40 };
    KOAN_EQ_SZ(__SZ, fwrite(out, sizeof *out, 4, f));

    rewind(f);

    int in[4] = {};
    KOAN_EQ_SZ(__SZ, fread(in, sizeof *in, 4, f));
    KOAN_EQ_INT(__, in[2]);

    /* Asking for more than remains gives a short count, not an error. */
    rewind(f);
    int big[10] = {};
    KOAN_EQ_SZ(__SZ, fread(big, sizeof *big, 10, f));
    KOAN_EQ_INT(__, feof(f));
    KOAN_EQ_INT(__, ferror(f));

    fclose(f);
}

/*
 * End-of-file is a *state*, not a value you can test for in advance. feof only
 * becomes true after a read has already failed, so `while (!feof(f))` reads
 * one time too many. Loop on the read itself.
 */
KOAN(feof_is_true_only_after_a_failed_read)
{
    FILE *f = scratch();
    fputs("ab", f);
    rewind(f);

    KOAN_EQ_INT(__, feof(f));         /* not yet, though nothing is left
                                             * after two more reads */
    fgetc(f);
    fgetc(f);
    KOAN_EQ_INT(__, feof(f));         /* still not: we read exactly two */

    KOAN_EQ_INT(__, fgetc(f));      /* this read fails ... */
    KOAN_EQ_INT(__, feof(f));         /* ... and only now is feof true */

    /* Which is why the correct loop tests the read, not feof. */
    rewind(f);
    clearerr(f);
    int count = 0, c;
    while ((c = fgetc(f)) != EOF) count++;
    KOAN_EQ_INT(__, count);

    fclose(f);
}

/*
 * fgetc returns an int, not a char, precisely so that EOF (-1) is
 * distinguishable from a valid byte. Storing it in a char breaks on any
 * byte that happens to be 0xFF.
 */
KOAN(fgetc_returns_int_for_a_reason)
{
    KOAN_TRUE(sizeof(EOF) == sizeof(int));
    KOAN_EQ_INT(__, EOF);

    /* 0xFF read as a byte is 255; truncated to a *signed* char it is -1, and
     * would be mistaken for EOF. */
    int         as_int  = 0xFF;
    signed char as_char = (signed char)as_int;
    KOAN_EQ_INT(__, as_int);
    KOAN_EQ_INT(__, as_char);
    KOAN_TRUE(as_int != EOF);

    /* Note `signed char`, not `char`. Whether plain char is signed is
     * implementation-defined — it is signed on Apple silicon and unsigned on
     * Linux/ARM — so say which you mean whenever the sign matters. */
    KOAN_TRUE(CHAR_MIN == 0 || CHAR_MIN < 0);
}

/*
 * Output is buffered. Until it is flushed — by fflush, by fclose, or by the
 * buffer filling — the bytes are in memory, not in the file. A second handle
 * on the same file will not see them.
 */
KOAN(output_is_buffered_until_flushed)
{
    const char *path = "koan_buffered.tmp";
    FILE       *w = fopen(path, "w");
    KOAN_TRUE(w != nullptr);
    fputs("written", w);

    /* Nothing is on disk yet. */
    FILE *r1 = fopen(path, "r");
    char  peek[32] = {};
    size_t got = fread(peek, 1, sizeof peek - 1, r1);
    fclose(r1);
    KOAN_EQ_SZ(__SZ, got);

    fflush(w);

    /* Now it is. */
    FILE *r2 = fopen(path, "r");
    memset(peek, 0, sizeof peek);
    fread(peek, 1, sizeof peek - 1, r2);
    fclose(r2);
    KOAN_EQ_STR(__STR, peek);

    fclose(w);
    remove(path);
}

/*
 * fprintf formats to a stream; sscanf parses from a string. sscanf returns the
 * number of items successfully *assigned*, which is the only way to know
 * whether it worked — and it is very easy for it to stop early.
 */
KOAN(sscanf_returns_the_assignment_count)
{
    int  a = 0, b = 0;
    char word[16] = {};

    KOAN_EQ_INT(__, sscanf("12 34", "%d %d", &a, &b));
    KOAN_EQ_INT(__, b);

    /* It stops at the first thing that does not match, and reports how far
     * it got. b is left untouched. */
    a = 0; b = 99;
    KOAN_EQ_INT(__, sscanf("12 xy", "%d %d", &a, &b));
    KOAN_EQ_INT(__, a);
    KOAN_EQ_INT(__, b);

    /* %s is unbounded and will overflow. Always give it a width. */
    KOAN_EQ_INT(__, sscanf("supercalifragilistic", "%15s", word));
    KOAN_EQ_SZ(__SZ, strlen(word));

    /* Nothing matched at all. */
    KOAN_EQ_INT(__, sscanf("xy", "%d", &a));
}

/*
 * Bringing it together.
 *
 * Write a record file, then read it back — the round trip that every save
 * format is. This needs a header, a count, seeking, short-read handling, and
 * a size check that refuses a truncated file rather than reading garbage.
 */
typedef struct {
    char id[8];
    int  score;
} Record;

static constexpr char MAGIC[4] = { 'K', 'O', 'A', 'N' };

static bool records_write(FILE *f, const Record *rows, size_t n)
{
    if (fwrite(MAGIC, 1, sizeof MAGIC, f) != sizeof MAGIC) return false;

    uint32_t count = (uint32_t)n;
    if (fwrite(&count, sizeof count, 1, f) != 1) return false;
    if (n && fwrite(rows, sizeof *rows, n, f) != n) return false;
    return true;
}

/* Returns the number read, or SIZE_MAX if the file is not a valid record file. */
static size_t records_read(FILE *f, Record *out, size_t cap)
{
    char magic[4];
    if (fread(magic, 1, sizeof magic, f) != sizeof magic)  return SIZE_MAX;
    if (memcmp(magic, MAGIC, sizeof MAGIC) != 0)           return SIZE_MAX;

    uint32_t count = 0;
    if (fread(&count, sizeof count, 1, f) != 1)            return SIZE_MAX;
    if (count > cap)                                       return SIZE_MAX;

    /* A short read here means the file claims more records than it holds. */
    if (fread(out, sizeof *out, count, f) != count)        return SIZE_MAX;
    return count;
}

KOAN(assembling_a_record_file)
{
    FILE  *f = scratch();
    Record written[2] = { { "ada", 100 }, { "alan", 95 } };

    KOAN_TRUE(records_write(f, written, 2));
    rewind(f);

    Record read_back[4] = {};
    KOAN_EQ_SZ(__SZ, records_read(f, read_back, 4));
    KOAN_EQ_STR(__STR, read_back[0].id);
    KOAN_EQ_INT(__, read_back[1].score);
    fclose(f);

    /* A file that is not ours is rejected on the magic number. */
    FILE *alien = scratch();
    fputs("not a record file at all", alien);
    rewind(alien);
    KOAN_TRUE(records_read(alien, read_back, 4) == SIZE_MAX);
    fclose(alien);

    /* A truncated file claims two records but holds one. */
    FILE *cut = scratch();
    fwrite(MAGIC, 1, sizeof MAGIC, cut);
    uint32_t lie = 2;
    fwrite(&lie, sizeof lie, 1, cut);
    fwrite(&written[0], sizeof written[0], 1, cut);   /* only one */
    rewind(cut);
    KOAN_TRUE(records_read(cut, read_back, 4) == SIZE_MAX);
    fclose(cut);
}

KOAN_LESSON(lesson_about_file_io, "About File I/O",
    KOAN_CASE(write_then_rewind_to_read),
    KOAN_CASE(fseek_moves_the_position),
    KOAN_CASE(fread_returns_elements_not_bytes),
    KOAN_CASE(feof_is_true_only_after_a_failed_read),
    KOAN_CASE(fgetc_returns_int_for_a_reason),
    KOAN_CASE(output_is_buffered_until_flushed),
    KOAN_CASE(sscanf_returns_the_assignment_count),
    KOAN_CASE(assembling_a_record_file)
);
