/*
 * About Strings
 *
 * C has no string type. It has a convention: a run of characters terminated
 * by a zero byte. Every string bug you will ever write comes from one of
 * three places — a missing terminator, a buffer too small, or confusing the
 * length of the text with the size of the storage holding it.
 */
#include "koan.h"

#include <stdio.h>
#include <string.h>

/*
 * A string literal is an array of char with a NUL appended, so its sizeof is
 * one greater than its visible length.
 */
KOAN(strings_carry_a_terminator)
{
    char word[] = "koan";

    KOAN_EQ_SZ(/*__SZ*/ 5, sizeof word);   /* four letters plus the NUL */
    KOAN_EQ_SZ(/*__SZ*/ 4, strlen(word));
    KOAN_EQ_CHR(/*__CHR*/ '\0', word[4]);
}

/*
 * strlen counts up to the terminator; it does not know the buffer. These two
 * numbers are independent, and confusing them is how buffers overflow.
 */
KOAN(length_and_capacity_are_different_questions)
{
    char buf[32] = "hi";

    KOAN_EQ_SZ(/*__SZ*/ 32, sizeof buf);
    KOAN_EQ_SZ(/*__SZ*/ 2, strlen(buf));

    /* Writing a terminator early shortens the string without touching size. */
    buf[1] = '\0';
    KOAN_EQ_SZ(/*__SZ*/ 1, strlen(buf));
    KOAN_EQ_SZ(/*__SZ*/ 32, sizeof buf);
}

/*
 * A string literal has static storage duration and is not writable. Assigning
 * one to a `char *` and then writing through it is undefined behaviour — it
 * usually faults. Point at literals with `const char *`, always.
 */
KOAN(literals_are_read_only)
{
    const char *literal = "immutable";
    char        copy[]  = "mutable";     /* an array, initialised from a literal */

    KOAN_EQ_CHR(/*__CHR*/ 'i', literal[0]);

    copy[0] = 'M';                        /* legal: copy is our own array */
    KOAN_EQ_STR(/*__STR*/ "Mutable", copy);
}

/*
 * Comparing strings with == compares addresses. Use strcmp, which returns
 * zero for equal, and orders by the first differing byte otherwise.
 */
KOAN(strcmp_orders_by_first_difference)
{
    KOAN_EQ_INT(/*__*/ 0, strcmp("abc", "abc"));
    KOAN_TRUE(strcmp("abc", "abd") < 0);
    KOAN_TRUE(strcmp("abd", "abc") > 0);

    /* A prefix sorts before the longer string containing it. */
    KOAN_TRUE(strcmp("ab", "abc") < 0);

    /* strncmp bounds the comparison, which is how you test a prefix. */
    KOAN_EQ_INT(/*__*/ 0, strncmp("abcdef", "abcXXX", 3));
}

/*
 * strcpy will happily write past the end of a buffer. snprintf will not: it
 * always terminates, and it tells you how much it *wanted* to write, which is
 * how you detect truncation.
 */
KOAN(snprintf_is_the_safe_way_to_build_strings)
{
    char small[8];

KOAN_DELIBERATE_BEGIN
    int wanted = snprintf(small, sizeof small, "%s-%d", "abcdefgh", 42);
KOAN_DELIBERATE_END

    /* The result is truncated but always terminated. */
    KOAN_EQ_SZ(/*__SZ*/ 7, strlen(small));
    KOAN_EQ_STR(/*__STR*/ "abcdefg", small);

    /* And the return value reveals what a big enough buffer would have held. */
    KOAN_EQ_INT(/*__*/ 11, wanted);
    KOAN_TRUE(wanted >= (int)sizeof small);   /* the truncation test */
}

/*
 * Searching. strchr finds a byte, strstr finds a substring, and both return
 * a pointer into the original string or null. Pointer subtraction turns that
 * back into an index.
 */
KOAN(searching_returns_pointers_not_indices)
{
    const char *s = "the quick brown fox";

    const char *space = strchr(s, ' ');
    KOAN_EQ_SZ(/*__SZ*/ 3, (size_t)(space - s));

    const char *brown = strstr(s, "brown");
    KOAN_EQ_SZ(/*__SZ*/ 10, (size_t)(brown - s));

    /* Not found is a null pointer, not -1. */
    KOAN_TRUE(strstr(s, "cat") == nullptr);

    /* strrchr searches from the end — the usual way to find a file extension. */
    const char *path = "/usr/local/lib/libc.a";
    KOAN_EQ_STR(/*__STR*/ "libc.a", strrchr(path, '/') + 1);
    KOAN_EQ_STR(/*__STR*/ ".a", strrchr(path, '.'));
}

/*
 * strcat appends, and requires that the destination already hold a valid
 * string with room to spare. Building strings by repeated strcat is O(n^2)
 * because each call re-scans for the terminator; track the end yourself.
 */
KOAN(concatenation_must_have_room)
{
    char buf[16] = "abc";

    strcat(buf, "def");
    KOAN_EQ_STR(/*__STR*/ "abcdef", buf);
    KOAN_EQ_SZ(/*__SZ*/ 6, strlen(buf));

    /* The cheaper idiom: remember where the end is. */
    char   out[32] = "";
    size_t len = 0;
    len += (size_t)snprintf(out + len, sizeof out - len, "%s", "one");
    len += (size_t)snprintf(out + len, sizeof out - len, ",%s", "two");
    KOAN_EQ_STR(/*__STR*/ "one,two", out);
    KOAN_EQ_SZ(/*__SZ*/ 7, len);
}

/*
 * The mem* family works on bytes and takes an explicit length, so it is
 * indifferent to terminators. Use it whenever the data is not text.
 * memmove, unlike memcpy, is defined when the regions overlap.
 */
KOAN(mem_functions_ignore_terminators)
{
    char data[8] = { 'a', 'b', 'c', 'd', 0, 0, 0, 0 };

    memset(data + 4, 'z', 3);
    KOAN_EQ_CHR(/*__CHR*/ 'z', data[4]);
    KOAN_EQ_STR(/*__STR*/ "abcdzzz", data);

    /* Overlapping shift: memmove handles it, memcpy would be undefined. */
    char shift[] = "abcdef";
    memmove(shift + 1, shift, 5);
    shift[6] = '\0';
    KOAN_EQ_STR(/*__STR*/ "aabcde", shift);
}

/*
 * Bringing it together.
 *
 * A bounded string builder: appends without ever overflowing, and reports
 * whether anything was lost. This is the pattern that replaces strcat in
 * code you intend to keep, and you will reuse it in the web server at the
 * end of the path.
 */
typedef struct {
    char  *buf;
    size_t cap;
    size_t len;
    bool   overflowed;
} Builder;

static void build_init(Builder *b, char *storage, size_t cap)
{
    b->buf = storage;
    b->cap = cap;
    b->len = 0;
    b->overflowed = false;
    if (cap > 0) b->buf[0] = '\0';
}

static void build_append(Builder *b, const char *text)
{
    if (b->cap == 0) { b->overflowed = true; return; }

    size_t room = b->cap - b->len - 1;      /* reserve the terminator */
    size_t want = strlen(text);

    if (want > room) { b->overflowed = true; want = room; }

    memcpy(b->buf + b->len, text, want);
    b->len += want;
    b->buf[b->len] = '\0';
}

KOAN(assembling_a_bounded_string_builder)
{
    char    storage[12];
    Builder b;

    build_init(&b, storage, sizeof storage);
    build_append(&b, "hello");
    build_append(&b, ", ");
    build_append(&b, "world");

    /* "hello, world" is 12 bytes, but one is needed for the terminator. */
    KOAN_EQ_STR(/*__STR*/ "hello, worl", b.buf);
    KOAN_EQ_SZ(/*__SZ*/ 11, b.len);
    KOAN_EQ_INT(/*__*/ 1, b.overflowed);

    /* With room to spare, nothing is lost. */
    char    roomy[64];
    Builder ok;
    build_init(&ok, roomy, sizeof roomy);
    build_append(&ok, "hello");
    build_append(&ok, ", world");
    KOAN_EQ_STR(/*__STR*/ "hello, world", ok.buf);
    KOAN_EQ_INT(/*__*/ 0, ok.overflowed);
}

KOAN_LESSON(lesson_about_strings, "About Strings",
    KOAN_CASE(strings_carry_a_terminator),
    KOAN_CASE(length_and_capacity_are_different_questions),
    KOAN_CASE(literals_are_read_only),
    KOAN_CASE(strcmp_orders_by_first_difference),
    KOAN_CASE(snprintf_is_the_safe_way_to_build_strings),
    KOAN_CASE(searching_returns_pointers_not_indices),
    KOAN_CASE(concatenation_must_have_room),
    KOAN_CASE(mem_functions_ignore_terminators),
    KOAN_CASE(assembling_a_bounded_string_builder)
);
