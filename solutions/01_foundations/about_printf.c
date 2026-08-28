/*
 * About printf
 *
 * printf is how a C program says anything. It is also the first place most
 * people meet undefined behaviour, because the format string is a promise
 * about the arguments that the compiler can only partly check.
 *
 * The family:
 *
 *   printf(fmt, ...)                  -> stdout
 *   fprintf(stream, fmt, ...)         -> a stream (stderr, a file)
 *   snprintf(buf, cap, fmt, ...)      -> a buffer, bounded
 *   sprintf(buf, fmt, ...)            -> a buffer, UNBOUNDED. never use it.
 *
 * These koans format into buffers so the results can be asserted. Everything
 * they show applies identically to printf.
 */
#include "koan.h"

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Format into a scratch buffer and return it, so koans read as one line. */
static char scratch[256];

#define FMT(...) (snprintf(scratch, sizeof scratch, __VA_ARGS__), scratch)

/*
 * A conversion specification is: %[flags][width][.precision][length]conversion
 * Every part is optional except the % and the conversion.
 */
KOAN(conversions_name_the_type)
{
    KOAN_EQ_STR(/*__STR*/ "42", FMT("%d", 42));
    KOAN_EQ_STR(/*__STR*/ "-42", FMT("%d", -42));
    KOAN_EQ_STR(/*__STR*/ "42", FMT("%u", 42u));
    KOAN_EQ_STR(/*__STR*/ "2a", FMT("%x", 42));
    KOAN_EQ_STR(/*__STR*/ "2A", FMT("%X", 42));
    KOAN_EQ_STR(/*__STR*/ "52", FMT("%o", 42));
    KOAN_EQ_STR(/*__STR*/ "k", FMT("%c", 'k'));
    KOAN_EQ_STR(/*__STR*/ "koan", FMT("%s", "koan"));

    /* A literal percent sign is %%, not \%. */
    KOAN_EQ_STR(/*__STR*/ "100%", FMT("100%%"));
}

/*
 * Width pads to a minimum; precision means different things per conversion.
 * For %f it is digits after the point; for %s it is a maximum length; for
 * integers it is a minimum number of digits.
 */
KOAN(width_pads_and_precision_truncates)
{
    /* Width 5, right-aligned by default. */
    KOAN_EQ_STR(/*__STR*/ "   42", FMT("%5d", 42));
    KOAN_EQ_STR(/*__STR*/ "42   ", FMT("%-5d", 42));
    KOAN_EQ_STR(/*__STR*/ "00042", FMT("%05d", 42));

    /* Width never truncates — it is a minimum. */
    KOAN_EQ_STR(/*__STR*/ "123456", FMT("%3d", 123456));

    /* For strings, precision is a maximum. This is the safe way to print a
     * possibly-unterminated buffer. */
    KOAN_EQ_STR(/*__STR*/ "koa", FMT("%.3s", "koan"));
    KOAN_EQ_STR(/*__STR*/ "  koa", FMT("%5.3s", "koan"));

    /* For integers, precision is a minimum digit count. */
    KOAN_EQ_STR(/*__STR*/ "0042", FMT("%.4d", 42));
}

/*
 * Floating point. %f is fixed, %e is scientific, %g picks whichever is
 * shorter and strips trailing zeros. The default precision for %f is 6.
 */
KOAN(floating_point_conversions)
{
    KOAN_EQ_STR(/*__STR*/ "1.500000", FMT("%f", 1.5));
    KOAN_EQ_STR(/*__STR*/ "1.50", FMT("%.2f", 1.5));
    KOAN_EQ_STR(/*__STR*/ "2", FMT("%.0f", 1.5));      /* ties-to-even */
    KOAN_EQ_STR(/*__STR*/ "2", FMT("%.0f", 2.5));

    KOAN_EQ_STR(/*__STR*/ "1.5", FMT("%g", 1.5));
    KOAN_EQ_STR(/*__STR*/ "1", FMT("%g", 1.0));        /* no trailing .000000 */

    /* %f takes a double. A float argument is promoted to double
     * automatically, so there is no %f-for-float and no length modifier. */
    float f = 0.5f;
    KOAN_EQ_STR(/*__STR*/ "0.50", FMT("%.2f", f));
}

/*
 * Length modifiers say how wide the argument is. Getting one wrong is
 * undefined behaviour: printf reads the number of bytes the format claims,
 * not the number you passed.
 */
KOAN(length_modifiers_must_match_the_argument)
{
    long      big  = 1234567890L;
    long long huge = 1234567890123LL;
    size_t    n    = 42;

    KOAN_EQ_STR(/*__STR*/ "1234567890", FMT("%ld", big));
    KOAN_EQ_STR(/*__STR*/ "1234567890123", FMT("%lld", huge));

    /* %zu for size_t — not %u, and not %lu. This is the one people get
     * wrong most often, and it breaks on 32-bit platforms. */
    KOAN_EQ_STR(/*__STR*/ "42", FMT("%zu", n));

    /* Small types are promoted to int before printf sees them, so %d is
     * correct for char and short — there is no %hhd requirement. */
    char  c = 5;
    short s = 6;
    KOAN_EQ_STR(/*__STR*/ "5 6", FMT("%d %d", c, s));
}

/*
 * The exact-width types need macros from <inttypes.h>, because the right
 * modifier for int64_t differs between platforms. PRId64 expands to the
 * correct one, and adjacent string literals join it to your format.
 */
KOAN(exact_width_types_need_inttypes_macros)
{
    int64_t  i = -9000000000LL;
    uint32_t u = 4000000000u;

    KOAN_EQ_STR(/*__STR*/ "-9000000000", FMT("%" PRId64, i));
    KOAN_EQ_STR(/*__STR*/ "4000000000", FMT("%" PRIu32, u));

    /* Which is just string-literal concatenation, visible here: */
    KOAN_EQ_STR(/*__STR*/ "value=-9000000000", FMT("value=%" PRId64, i));
}

/*
 * snprintf never overflows and always terminates. Its return value is the
 * length it *wanted* — so a return >= the capacity means truncation happened.
 * That is the only reliable truncation test.
 */
KOAN(snprintf_reports_what_it_wanted_to_write)
{
    char small[8];

KOAN_DELIBERATE_BEGIN
    int wanted = snprintf(small, sizeof small, "%s", "abcdefghij");
KOAN_DELIBERATE_END
    KOAN_EQ_INT(/*__*/ 10, wanted);
    KOAN_EQ_STR(/*__STR*/ "abcdefg", small);
    KOAN_TRUE((size_t)wanted >= sizeof small);      /* truncated */

    int fits = snprintf(small, sizeof small, "%s", "abc");
    KOAN_EQ_INT(/*__*/ 3, fits);
    KOAN_TRUE((size_t)fits < sizeof small);         /* complete */

    /* A capacity of zero writes nothing but still reports the length, which
     * is how you size a buffer before allocating it. */
    KOAN_EQ_INT(/*__*/ 5, snprintf(nullptr, 0, "%s", "hello"));
}

/*
 * Building a string in pieces: advance the cursor by what was written, and
 * shrink the remaining capacity. Get this wrong and you write past the end.
 */
KOAN(appending_needs_the_cursor_and_the_remainder)
{
    char   buf[32];
    size_t len = 0;

    len += (size_t)snprintf(buf + len, sizeof buf - len, "%s", "a=1");
    len += (size_t)snprintf(buf + len, sizeof buf - len, ";%s", "b=2");
    len += (size_t)snprintf(buf + len, sizeof buf - len, ";%s", "c=3");

    KOAN_EQ_STR(/*__STR*/ "a=1;b=2;c=3", buf);
    KOAN_EQ_SZ(/*__SZ*/ 11, len);
    KOAN_EQ_SZ(/*__SZ*/ 11, strlen(buf));
}

/*
 * stdout is line-buffered to a terminal and block-buffered to a pipe; stderr
 * is unbuffered. That is why a crash can lose your printf output but never
 * your fprintf(stderr, ...) output — and why diagnostics belong on stderr.
 */
KOAN(stderr_is_unbuffered_and_stdout_is_not)
{
    /* Both are FILE *, and fprintf writes to either. */
    KOAN_TRUE(stdout != nullptr);
    KOAN_TRUE(stderr != nullptr);
    KOAN_TRUE(stdout != stderr);

    /* printf(fmt, ...) is exactly fprintf(stdout, fmt, ...). */
    int via_stream = fprintf(stdout, "%s", "");
    KOAN_EQ_INT(/*__*/ 0, via_stream);      /* zero characters written */
}

/*
 * Bringing it together.
 *
 * A table formatter: columns that line up regardless of content, with
 * truncation rather than overflow when a field is too wide. Width and
 * precision do all the work — no manual padding loops.
 *
 * Target layout, with a 10-wide left-aligned name and a 6-wide right-aligned
 * score:
 *
 *     ada        |     90
 *     bartholom~ |    100
 */
static const char *format_row(char *out, size_t cap,
                              const char *name, int score)
{
    /* %-10.10s: left-aligned, minimum 10, maximum 10 — so always exactly 10. */
    snprintf(out, cap, "%-10.10s | %6d", name, score);
    return out;
}

KOAN(assembling_a_table_formatter)
{
    char row[64];

    KOAN_EQ_STR(/*__STR*/ "ada        |     90",
                format_row(row, sizeof row, "ada", 90));

    /* The column is exactly ten wide whatever the name's length. */
    KOAN_EQ_SZ(/*__SZ*/ 19, strlen(row));

    /* An over-long name is truncated to ten, not allowed to shift the column. */
    KOAN_EQ_STR(/*__STR*/ "bartholome |    100",
                format_row(row, sizeof row, "bartholomew", 100));

    /* Every row is the same width, which is the point. */
    KOAN_EQ_SZ(/*__SZ*/ 19, strlen(row));

    /* A negative score still fits the six-wide column. */
    format_row(row, sizeof row, "zed", -5);
    KOAN_EQ_SZ(/*__SZ*/ 19, strlen(row));
    KOAN_TRUE(strstr(row, "    -5") != nullptr);
}

KOAN_LESSON(lesson_about_printf, "About printf",
    KOAN_CASE(conversions_name_the_type),
    KOAN_CASE(width_pads_and_precision_truncates),
    KOAN_CASE(floating_point_conversions),
    KOAN_CASE(length_modifiers_must_match_the_argument),
    KOAN_CASE(exact_width_types_need_inttypes_macros),
    KOAN_CASE(snprintf_reports_what_it_wanted_to_write),
    KOAN_CASE(appending_needs_the_cursor_and_the_remainder),
    KOAN_CASE(stderr_is_unbuffered_and_stdout_is_not),
    KOAN_CASE(assembling_a_table_formatter)
);
