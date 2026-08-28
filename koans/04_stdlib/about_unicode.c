/*
 * About Text and Unicode
 *
 * C's `char` is a byte, not a character. Those were the same thing in 1978
 * and have not been since. Almost every text bug in C comes from code that
 * still assumes they are.
 *
 * The distinctions that matter:
 *
 *   byte        one char; strlen counts these
 *   code point  one Unicode value, U+0000..U+10FFFF; char32_t holds one
 *   grapheme    what a reader calls "a character"; may be several code points
 *
 * UTF-8 encodes a code point in 1-4 bytes. It is the only encoding worth
 * using, and it was designed so that C string functions keep working: no
 * multi-byte sequence contains a zero byte, and ASCII is unchanged.
 */
#include "koan.h"
#include "compat.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/*
 * strlen counts bytes. For anything outside ASCII that is not the number of
 * characters, and no amount of wishing will change it.
 */
KOAN(strlen_counts_bytes_not_characters)
{
    const char *ascii = "hello";
    const char *greek = "αβγ";      /* three Greek letters */
    const char *emoji = "\U0001F600";              /* one emoji */

    KOAN_EQ_SZ(__SZ, strlen(ascii));

    /* Each Greek letter is two bytes in UTF-8. */
    KOAN_EQ_SZ(__SZ, strlen(greek));

    /* The emoji is four bytes, and is one character. */
    KOAN_EQ_SZ(__SZ, strlen(emoji));
}

/*
 * UTF-8's structure, which is the whole reason it won:
 *
 *   0xxxxxxx                     1 byte   ASCII, unchanged
 *   110xxxxx 10xxxxxx            2 bytes
 *   1110xxxx 10xxxxxx 10xxxxxx   3 bytes
 *   11110xxx 10xxxxxx x3         4 bytes
 *
 * A continuation byte always starts 10, so no byte is ambiguous and you can
 * find a character boundary from anywhere by scanning backwards.
 */
static bool is_continuation(unsigned char b) { return (b & 0xC0) == 0x80; }

static size_t utf8_sequence_length(unsigned char lead)
{
    if (lead < 0x80)          return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;                                   /* not a lead byte */
}

KOAN(utf8_lead_bytes_announce_the_length)
{
    const unsigned char *greek = (const unsigned char *)"α";
    const unsigned char *emoji = (const unsigned char *)"\U0001F600";

    KOAN_EQ_SZ(__SZ, utf8_sequence_length('A'));
    KOAN_EQ_SZ(__SZ, utf8_sequence_length(greek[0]));
    KOAN_EQ_SZ(__SZ, utf8_sequence_length(emoji[0]));

    /* Continuation bytes are not lead bytes, and never mistakable for one. */
    KOAN_EQ_INT(__, is_continuation(greek[1]));
    KOAN_EQ_INT(__, is_continuation(greek[0]));
    KOAN_EQ_SZ(__SZ, utf8_sequence_length(greek[1]));

    /* ASCII bytes are never continuation bytes, which is why strchr(s, '/')
     * is still correct on UTF-8 text. */
    KOAN_EQ_INT(__, is_continuation('/'));
}

/*
 * Counting characters therefore means counting non-continuation bytes. That
 * is a one-line loop, and it is what most code actually needs.
 */
static size_t utf8_count(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!is_continuation(*p)) n++;
    return n;
}

KOAN(counting_code_points_means_skipping_continuations)
{
    KOAN_EQ_SZ(__SZ, utf8_count("hello"));
    KOAN_EQ_SZ(__SZ, utf8_count("αβγ"));
    KOAN_EQ_SZ(__SZ, utf8_count("\U0001F600"));

    /* Mixed text: 5 ASCII + 1 space + 3 Greek. */
    KOAN_EQ_SZ(__SZ, utf8_count("hello αβγ"));

    /* And the byte count differs, which is the point. */
    KOAN_EQ_SZ(__SZ, strlen("hello αβγ"));
}

/*
 * C23 gives string literals explicit encodings. u8"" is UTF-8 and its element
 * type is char8_t; u"" is UTF-16; U"" is UTF-32, where one element is exactly
 * one code point.
 */
KOAN(literals_can_declare_their_encoding)
{
    const char32_t *wide = U"aα\U0001F600";

    /* One element per code point, so indexing is meaningful here. */
    KOAN_EQ_INT(__, (int)wide[0]);        /* 'a'   */
    KOAN_EQ_INT(__, (int)wide[1]);       /* alpha */
    KOAN_EQ_INT(__, (int)wide[2]);     /* emoji */
    KOAN_EQ_INT(__, (int)wide[3]);           /* terminator */

    KOAN_EQ_SZ(__SZ, sizeof(char32_t));

    /* The same text as UTF-8 needs a different number of elements. */
    KOAN_EQ_SZ(__SZ, strlen("aα\U0001F600"));
}

/*
 * Decoding: turn bytes into a code point. This is the function every parser
 * of UTF-8 needs, and it must reject malformed input rather than guess —
 * over-long encodings are a classic way to smuggle a '/' past a filter.
 */
static size_t utf8_decode(const char *s, size_t avail, char32_t *out)
{
    const unsigned char *p = (const unsigned char *)s;
    if (avail == 0) return 0;

    size_t len = utf8_sequence_length(p[0]);
    if (len == 0 || len > avail) return 0;

    static const char32_t MIN_FOR_LENGTH[5] = { 0, 0, 0x80, 0x800, 0x10000 };
    static const unsigned char LEAD_MASK[5] = { 0, 0x7F, 0x1F, 0x0F, 0x07 };

    char32_t cp = p[0] & LEAD_MASK[len];
    for (size_t i = 1; i < len; i++) {
        if (!is_continuation(p[i])) return 0;
        cp = (cp << 6) | (p[i] & 0x3F);
    }

    if (cp < MIN_FOR_LENGTH[len]) return 0;      /* over-long encoding */
    if (cp > 0x10FFFF)            return 0;      /* beyond Unicode */
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0;  /* surrogate half */

    *out = cp;
    return len;
}

KOAN(decoding_must_reject_malformed_input)
{
    char32_t cp = 0;

    KOAN_EQ_SZ(__SZ, utf8_decode("A", 1, &cp));
    KOAN_EQ_INT(__, (int)cp);

    KOAN_EQ_SZ(__SZ, utf8_decode("α", 2, &cp));
    KOAN_EQ_INT(__, (int)cp);

    KOAN_EQ_SZ(__SZ, utf8_decode("\U0001F600", 4, &cp));
    KOAN_EQ_INT(__, (int)cp);

    /* A truncated sequence is rejected, not half-decoded. */
    KOAN_EQ_SZ(__SZ, utf8_decode("α", 1, &cp));

    /* A continuation byte where a lead byte belongs. */
    KOAN_EQ_SZ(__SZ, utf8_decode("\x80", 1, &cp));

    /* The security case: C0 AF is an over-long encoding of '/'. Accepting it
     * would let "..%C0%AF.." slip past a check for "../". */
    KOAN_EQ_SZ(__SZ, utf8_decode("\xC0\xAF", 2, &cp));
}

/*
 * Encoding is the inverse, and the sizes fall out of the same table.
 */
static size_t utf8_encode(char32_t cp, char *out)
{
    if (cp <= 0x7F)     { out[0] = (char)cp; return 1; }
    if (cp <= 0x7FF)    {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF)   {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

KOAN(encoding_is_the_inverse)
{
    char buf[8] = {};

    KOAN_EQ_SZ(__SZ, utf8_encode(0x41, buf));
    KOAN_EQ_STR(__STR, buf);

    memset(buf, 0, sizeof buf);
    KOAN_EQ_SZ(__SZ, utf8_encode(0x3B1, buf));
    KOAN_EQ_STR(__STR, buf);

    memset(buf, 0, sizeof buf);
    KOAN_EQ_SZ(__SZ, utf8_encode(0x1F600, buf));
    KOAN_EQ_STR(__STR, buf);

    /* A round trip must be exact — the real test of a codec. */
    char32_t back = 0;
    utf8_decode(buf, 4, &back);
    KOAN_EQ_INT(__, (int)back);
}

/*
 * wchar_t predates all of this and is a portability trap: 4 bytes on Linux and
 * macOS, 2 on Windows — where it cannot hold a code point above U+FFFF. Use
 * char32_t when you mean a code point, and UTF-8 in char for everything else.
 */
KOAN(wchar_t_is_not_portable)
{
    KOAN_TRUE(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4);

    /* char32_t, by contrast, is exactly 32 bits everywhere. */
    KOAN_EQ_SZ(__SZ, sizeof(char32_t));
    KOAN_EQ_SZ(__SZ, sizeof(char16_t));

    /* Which is why "how many bytes is a character" has no answer, and
     * "how many bytes is this code point in UTF-8" always does. */
    char scratch[4];
    KOAN_EQ_SZ(__SZ, utf8_encode(0xFFFD, scratch));   /* replacement char */
}

/*
 * Bringing it together.
 *
 * Truncate UTF-8 text to a byte budget without splitting a character in half.
 * Naive truncation produces invalid text; this is the function every
 * log-line and database-column truncation needs, and getting it right means
 * walking characters, not bytes.
 */
static size_t utf8_truncate(const char *s, size_t max_bytes, char *out, size_t cap)
{
    size_t   in = 0, written = 0;
    char32_t cp;

    while (s[in]) {
        size_t len = utf8_decode(s + in, strlen(s) - in, &cp);
        if (len == 0) break;                       /* malformed: stop cleanly */
        if (written + len > max_bytes) break;      /* would exceed the budget */
        if (written + len + 1 > cap) break;        /* would exceed the buffer */

        memcpy(out + written, s + in, len);
        written += len;
        in += len;
    }

    out[written] = '\0';
    return written;
}

KOAN(assembling_a_utf8_safe_truncation)
{
    char out[32];

    /* ASCII truncates exactly at the budget. */
    KOAN_EQ_SZ(__SZ, utf8_truncate("hello world", 5, out, sizeof out));
    KOAN_EQ_STR(__STR, out);

    /* "αβγ" is 6 bytes. A budget of 5 must stop after two letters, not
     * three-and-a-half — so it writes 4 bytes, not 5. */
    KOAN_EQ_SZ(__SZ, utf8_truncate("αβγ", 5, out, sizeof out));
    KOAN_EQ_SZ(__SZ, utf8_count(out));

    /* The result is always valid UTF-8, which is the whole requirement. */
    char32_t cp = 0;
    KOAN_EQ_SZ(__SZ, utf8_decode(out, strlen(out), &cp));
    KOAN_EQ_INT(__, (int)cp);

    /* A budget smaller than the first character yields the empty string
     * rather than a broken byte. */
    KOAN_EQ_SZ(__SZ, utf8_truncate("\U0001F600", 3, out, sizeof out));
    KOAN_EQ_STR(__STR, out);

    /* A budget larger than the text copies all of it. */
    KOAN_EQ_SZ(__SZ, utf8_truncate("αβγ", 100, out, sizeof out));
    KOAN_EQ_SZ(__SZ, utf8_count(out));
}

KOAN_LESSON(lesson_about_unicode, "About Text and Unicode",
    KOAN_CASE(strlen_counts_bytes_not_characters),
    KOAN_CASE(utf8_lead_bytes_announce_the_length),
    KOAN_CASE(counting_code_points_means_skipping_continuations),
    KOAN_CASE(literals_can_declare_their_encoding),
    KOAN_CASE(decoding_must_reject_malformed_input),
    KOAN_CASE(encoding_is_the_inverse),
    KOAN_CASE(wchar_t_is_not_portable),
    KOAN_CASE(assembling_a_utf8_safe_truncation)
);
