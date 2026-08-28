/*
 * About the String Library
 *
 * <string.h> is small, old, and full of edges. The functions divide cleanly:
 * `str*` stop at a NUL, `mem*` take a length and do not care what the bytes
 * mean. Choosing the wrong family is how buffers overflow.
 *
 * Several of these functions have no safe form in ISO C. Where that is true,
 * the koan shows what to write instead.
 */
#include "koan.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * strncpy is not a safe strcpy. It pads with NULs when the source is short,
 * and — the part that bites — does NOT terminate when the source is too long.
 */
KOAN(strncpy_does_not_always_terminate)
{
    char padded[8];
    strncpy(padded, "ab", sizeof padded);
    KOAN_EQ_STR(__STR, padded);
    KOAN_EQ_CHR(__CHR, padded[7]);     /* padded all the way out */

    char truncated[4];
KOAN_DELIBERATE_BEGIN
    strncpy(truncated, "abcdefgh", sizeof truncated);
KOAN_DELIBERATE_END
    KOAN_EQ_CHR(__CHR, truncated[0]);
    KOAN_EQ_CHR(__CHR, truncated[3]);   /* no NUL: not a string */

    /* The fix is to terminate yourself, or to use snprintf. */
    truncated[sizeof truncated - 1] = '\0';
    KOAN_EQ_STR(__STR, truncated);
    KOAN_EQ_SZ(__SZ, strlen(truncated));
}

/*
 * snprintf is the portable bounded copy: it always terminates, and its return
 * value reports what a large enough buffer would have held.
 */
static bool copy_bounded(char *dst, size_t cap, const char *src)
{
    int wanted = snprintf(dst, cap, "%s", src);
    return wanted >= 0 && (size_t)wanted < cap;     /* false means truncated */
}

KOAN(snprintf_is_the_portable_bounded_copy)
{
    char buf[8];

    KOAN_EQ_INT(__, copy_bounded(buf, sizeof buf, "short"));
    KOAN_EQ_STR(__STR, buf);

    KOAN_EQ_INT(__, copy_bounded(buf, sizeof buf, "far too long"));
    KOAN_EQ_STR(__STR, buf);          /* terminated regardless */
    KOAN_EQ_SZ(__SZ, strlen(buf));
}

/*
 * memchr searches bytes and takes a length, so it works on data containing
 * NULs — the only way to scan a buffer that is not a string.
 */
KOAN(memchr_scans_bytes_not_strings)
{
    char data[8] = { 'a', 'b', '\0', 'c', 'd', '\0', 'e', 'f' };

    /* strchr stops at the first NUL and never sees 'd'. */
    KOAN_TRUE(strchr(data, 'd') == nullptr);

    /* memchr keeps going, because it was told how far. */
    const char *found = memchr(data, 'd', sizeof data);
    KOAN_TRUE(found != nullptr);
    KOAN_EQ_SZ(__SZ, (size_t)(found - data));

    /* Which is also how you find a length within a bounded buffer. */
    const char *nul = memchr(data, '\0', sizeof data);
    KOAN_EQ_SZ(__SZ, (size_t)(nul - data));
}

/*
 * The comparison family. memcmp compares bytes as unsigned; the sign of the
 * result is meaningful but its magnitude is not, so never test for -1 or 1.
 */
KOAN(comparisons_report_order_not_distance)
{
    KOAN_TRUE(strcmp("a", "b") < 0);
    KOAN_TRUE(memcmp("abc", "abd", 3) < 0);
    KOAN_EQ_INT(__, memcmp("abc", "abd", 2));   /* only the first two */

    /* Bytes compare as unsigned char, so a high byte sorts above 'a'. */
    unsigned char high[] = { 0xFF, 0 };
    KOAN_TRUE(memcmp(high, "a", 1) > 0);

    /* strncmp bounds the comparison — the prefix test. */
    KOAN_EQ_INT(__, strncmp("prefix_rest", "prefix_other", 7));
    KOAN_TRUE(strncmp("prefix_rest", "prefix_other", 8) != 0);
}

/*
 * strspn counts a leading run of accepted characters; strcspn counts a
 * leading run of *rejected* ones. Together they tokenise without allocating.
 */
KOAN(strspn_and_strcspn_measure_runs)
{
    const char *line = "   42abc";

    KOAN_EQ_SZ(__SZ, strspn(line, " "));            /* leading spaces */
    KOAN_EQ_SZ(__SZ, strspn(line, " 0123456789"));  /* spaces + digits */

    /* strcspn: how far to the first character in the set. */
    KOAN_EQ_SZ(__SZ, strcspn(line, "0123456789"));
    KOAN_EQ_SZ(__SZ, strcspn(line, "@"));           /* not found: whole length */

    /* strpbrk returns a pointer to the first match instead of an offset. */
    const char *digit = strpbrk(line, "0123456789");
    KOAN_EQ_CHR(__CHR, *digit);
}

/*
 * strtok splits a string, and is notorious: it modifies its input, keeps
 * static state between calls, and is therefore neither reentrant nor
 * thread-safe. Know it, because you will read it; prefer to write the
 * explicit version below.
 */
KOAN(strtok_mutates_and_remembers)
{
    char text[] = "a,b,,c";       /* an array, because strtok writes to it */

    char *first = strtok(text, ",");
    KOAN_EQ_STR(__STR, first);

    /* The separator was overwritten with a NUL, in place. */
    KOAN_EQ_CHR(__CHR, text[1]);

    KOAN_EQ_STR(__STR, strtok(nullptr, ","));

    /* Consecutive separators are treated as one, so the empty field between
     * the two commas is silently skipped — rarely what a parser wants. */
    KOAN_EQ_STR(__STR, strtok(nullptr, ","));
    KOAN_TRUE(strtok(nullptr, ",") == nullptr);
}

/*
 * Transformations. There is no standard `strupr`; ctype plus a loop is the
 * portable way, and the cast to unsigned char is required — passing a
 * negative char to toupper is undefined.
 */
static void upcase(char *s)
{
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

KOAN(ctype_needs_an_unsigned_char_cast)
{
    char word[] = "koan42";
    upcase(word);
    KOAN_EQ_STR(__STR, word);

    KOAN_EQ_INT(__, isdigit((unsigned char)'7') != 0);
    KOAN_EQ_INT(__, isdigit((unsigned char)'x') != 0);
    KOAN_EQ_INT(__, isalpha((unsigned char)'x') != 0);
    KOAN_EQ_INT(__, isspace((unsigned char)'\t') != 0);

    /* toupper leaves anything that is not a lowercase letter alone. */
    KOAN_EQ_INT(__, toupper((unsigned char)'4'));
}

/*
 * Bringing it together.
 *
 * A CSV field splitter that does not allocate, does not modify its input, and
 * — unlike strtok — preserves empty fields. It returns the number of fields
 * found even when that exceeds the caller's capacity, so the caller can
 * detect that it needs a bigger array.
 */
typedef struct {
    const char *start;
    size_t      len;
} Field;

static size_t split_csv(const char *line, char sep, Field *out, size_t cap)
{
    size_t      count = 0;
    const char *cursor = line;

    for (;;) {
        size_t      len = strcspn(cursor, (char[]){ sep, '\0' });
        if (count < cap) out[count] = (Field){ .start = cursor, .len = len };
        count++;

        if (cursor[len] == '\0') break;
        cursor += len + 1;              /* step over the separator */
    }
    return count;
}

static bool field_equals(Field f, const char *text)
{
    return strlen(text) == f.len && memcmp(f.start, text, f.len) == 0;
}

KOAN(assembling_a_csv_splitter)
{
    Field fields[4];

    KOAN_EQ_SZ(__SZ, split_csv("alpha,beta,gamma", ',', fields, 4));
    KOAN_EQ_INT(__, field_equals(fields[0], "alpha"));
    KOAN_EQ_INT(__, field_equals(fields[2], "gamma"));
    KOAN_EQ_SZ(__SZ, fields[1].len);

    /* Empty fields are preserved, which is the whole point. */
    KOAN_EQ_SZ(__SZ, split_csv("a,,b,", ',', fields, 4));
    KOAN_EQ_SZ(__SZ, fields[1].len);
    KOAN_EQ_INT(__, field_equals(fields[3], ""));

    /* A line with no separator is one field. */
    KOAN_EQ_SZ(__SZ, split_csv("solo", ',', fields, 4));
    KOAN_EQ_INT(__, field_equals(fields[0], "solo"));

    /* The empty line is one empty field, not zero fields. */
    KOAN_EQ_SZ(__SZ, split_csv("", ',', fields, 4));
    KOAN_EQ_SZ(__SZ, fields[0].len);

    /* Overflow is reported rather than hidden: the count exceeds the cap. */
    KOAN_EQ_SZ(__SZ, split_csv("1,2,3,4,5,6", ',', fields, 4));

    /* And the input was never modified. */
    const char *original = "a,b";
    split_csv(original, ',', fields, 4);
    KOAN_EQ_STR(__STR, original);
}

KOAN_LESSON(lesson_about_string_library, "About the String Library",
    KOAN_CASE(strncpy_does_not_always_terminate),
    KOAN_CASE(snprintf_is_the_portable_bounded_copy),
    KOAN_CASE(memchr_scans_bytes_not_strings),
    KOAN_CASE(comparisons_report_order_not_distance),
    KOAN_CASE(strspn_and_strcspn_measure_runs),
    KOAN_CASE(strtok_mutates_and_remembers),
    KOAN_CASE(ctype_needs_an_unsigned_char_cast),
    KOAN_CASE(assembling_a_csv_splitter)
);
