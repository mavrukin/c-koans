/*
 * About Variadic Functions
 *
 * A variadic function takes any number of arguments, and the compiler checks
 * none of them. That is not an exaggeration: after the named parameters,
 * types are lost entirely, and va_arg simply reads whatever bytes are next
 * using the type you name.
 *
 * So a variadic function must be told, by its own arguments, how many there
 * are and what they are. There are three ways, and every real API uses one:
 *
 *   a count       print_all(3, a, b, c)
 *   a sentinel    print_all(a, b, c, nullptr)
 *   a format      printf("%d %s", n, s)
 */
#include "koan.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * va_start needs the last *named* parameter, so a variadic function must have
 * at least one. va_end must be called on every path.
 */
static int sum_counted(int count, ...)
{
    va_list args;
    va_start(args, count);

    int total = 0;
    for (int i = 0; i < count; i++)
        total += va_arg(args, int);

    va_end(args);
    return total;
}

KOAN(a_count_tells_the_callee_how_many)
{
    KOAN_EQ_INT(/*__*/ 6, sum_counted(3, 1, 2, 3));
    KOAN_EQ_INT(/*__*/ 0, sum_counted(0));
    KOAN_EQ_INT(/*__*/ 55, sum_counted(10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

    /* Lying about the count reads whatever happens to be on the stack. The
     * compiler cannot catch it; only discipline can. */
    KOAN_EQ_INT(/*__*/ 3, sum_counted(2, 1, 2, 99));   /* the 99 is ignored */
}

/*
 * A sentinel ends the list instead. It must be a value that cannot appear in
 * the data — null for pointers, or a reserved number.
 */
static size_t join(char *out, size_t cap, ...)
{
    va_list args;
    va_start(args, cap);

    size_t len = 0;
    const char *piece;

    while ((piece = va_arg(args, const char *)) != nullptr) {
        int n = snprintf(out + len, cap - len, "%s", piece);
        if (n < 0 || (size_t)n >= cap - len) { len = cap - 1; break; }
        len += (size_t)n;
    }

    va_end(args);
    out[len] = '\0';
    return len;
}

KOAN(a_sentinel_ends_the_list)
{
    char buf[64];

    KOAN_EQ_SZ(/*__SZ*/ 9, join(buf, sizeof buf, "one", "two", "six", nullptr));
    KOAN_EQ_STR(/*__STR*/ "onetwosix", buf);

    /* An empty list is just the sentinel. */
    KOAN_EQ_SZ(/*__SZ*/ 0, join(buf, sizeof buf, nullptr));
    KOAN_EQ_STR(/*__STR*/ "", buf);
}

/*
 * Default argument promotions apply to every variadic argument: char and
 * short become int, and float becomes double. So you must never read a
 * variadic argument as char, short or float — those types cannot arrive.
 */
static int read_promoted(int count, ...)
{
    va_list args;
    va_start(args, count);

    /* Passed as a char, but it arrived as an int. */
    int c = va_arg(args, int);
    /* Passed as a float, but it arrived as a double. */
    double d = va_arg(args, double);

    va_end(args);
    return c + (int)d;
}

KOAN(variadic_arguments_are_promoted)
{
    char  c = 10;
    float f = 2.5f;

    KOAN_EQ_INT(/*__*/ 12, read_promoted(2, c, f));

    /* Which is why printf has no conversion for float, and why %hhd is
     * merely a hint about how to *print* an int, not about what arrived. */
    char buf[32];
    snprintf(buf, sizeof buf, "%d %.1f", c, f);
    KOAN_EQ_STR(/*__STR*/ "10 2.5", buf);
}

/*
 * A va_list may only be walked once. To use it twice, copy it with va_copy —
 * and end both. This is exactly what a function that measures then formats
 * must do.
 */
static int measure_then_format(char *out, size_t cap, const char *fmt, ...)
{
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);

    int needed = vsnprintf(nullptr, 0, fmt, args);   /* consumes `args` */
    va_end(args);

    if (needed >= 0 && (size_t)needed < cap)
        vsnprintf(out, cap, fmt, copy);              /* consumes `copy`  */

    va_end(copy);
    return needed;
}

KOAN(va_copy_is_needed_to_walk_a_list_twice)
{
    char buf[64];

    int needed = measure_then_format(buf, sizeof buf, "%s=%d", "port", 8080);
    KOAN_EQ_INT(/*__*/ 9, needed);
    KOAN_EQ_STR(/*__STR*/ "port=8080", buf);

    /* Too long to fit: the length is reported, and nothing was written. */
    char tiny[4] = "old";
    KOAN_EQ_INT(/*__*/ 9, measure_then_format(tiny, sizeof tiny, "%s=%d", "port", 8080));
    KOAN_EQ_STR(/*__STR*/ "old", tiny);
}

/*
 * The v-prefixed functions take a va_list instead of "...", which is how you
 * forward variadic arguments to another function. There is no way to call
 * printf itself with a va_list — that is what vprintf exists for.
 */
static int log_line(char *out, size_t cap, const char *level, const char *fmt, ...)
{
    int n = snprintf(out, cap, "[%s] ", level);
    if (n < 0 || (size_t)n >= cap) return -1;

    va_list args;
    va_start(args, fmt);
    int rest = vsnprintf(out + n, cap - (size_t)n, fmt, args);
    va_end(args);

    return rest < 0 ? -1 : n + rest;
}

KOAN(v_functions_forward_a_va_list)
{
    char buf[64];

    KOAN_EQ_INT(/*__*/ 20, log_line(buf, sizeof buf, "warn", "disk %d%% full", 91));
    KOAN_EQ_STR(/*__STR*/ "[warn] disk 91% full", buf);

    log_line(buf, sizeof buf, "info", "%s", "started");
    KOAN_EQ_STR(/*__STR*/ "[info] started", buf);
}

/*
 * A format string that does not match its arguments is undefined behaviour,
 * not a wrong answer. Compilers check literal formats; they cannot check a
 * format held in a variable, which is why user-supplied format strings are a
 * genuine security hole.
 */
KOAN(the_format_must_match_the_arguments)
{
    char buf[32];

    /* Correct: the conversion and the argument agree. */
    snprintf(buf, sizeof buf, "%ld", 5L);
    KOAN_EQ_STR(/*__STR*/ "5", buf);

    /* If you must accept a caller's text, print it as data, never as a
     * format. This is the whole fix. */
    const char *untrusted = "100%% sure %s %n";
    snprintf(buf, sizeof buf, "%s", untrusted);
    KOAN_EQ_STR(/*__STR*/ "100%% sure %s %n", buf);
}

/*
 * Bringing it together.
 *
 * A tiny formatter of your own: walk a format string, pull the matching
 * argument for each conversion, and append it to a bounded buffer. This is
 * printf, minus everything except the shape — and writing it is the fastest
 * way to understand why printf cannot check its own arguments.
 *
 * Supports %d, %s, %c and %%; anything else is copied through literally.
 */
static size_t tiny_format(char *out, size_t cap, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    size_t len = 0;
    for (const char *p = fmt; *p && len + 1 < cap; p++) {
        if (*p != '%') { out[len++] = *p; continue; }

        p++;
        if (*p == '\0') break;

        char piece[32];
        const char *text = piece;

        switch (*p) {
            case 'd': snprintf(piece, sizeof piece, "%d", va_arg(args, int)); break;
            case 'c': piece[0] = (char)va_arg(args, int); piece[1] = '\0';    break;
            case 's': text = va_arg(args, const char *);                      break;
            case '%': piece[0] = '%'; piece[1] = '\0';                        break;
            default:  piece[0] = '%'; piece[1] = *p; piece[2] = '\0';         break;
        }

        while (*text && len + 1 < cap) out[len++] = *text++;
    }

    va_end(args);
    out[len] = '\0';
    return len;
}

KOAN(assembling_a_tiny_printf)
{
    char buf[64];

    /* The return value is the length written, which must agree with strlen. */
    KOAN_EQ_SZ(/*__SZ*/ 9, tiny_format(buf, sizeof buf, "%s=%d", "port", 8080));
    KOAN_EQ_STR(/*__STR*/ "port=8080", buf);
    KOAN_EQ_SZ(/*__SZ*/ 9, strlen(buf));

    tiny_format(buf, sizeof buf, "%c%c%c", 'a', 'b', 'c');
    KOAN_EQ_STR(/*__STR*/ "abc", buf);

    tiny_format(buf, sizeof buf, "100%%");
    KOAN_EQ_STR(/*__STR*/ "100%", buf);

    /* An unknown conversion is passed through rather than losing the text. */
    tiny_format(buf, sizeof buf, "%q done");
    KOAN_EQ_STR(/*__STR*/ "%q done", buf);

    /* Truncation stops cleanly at the capacity, always terminated. */
    char small[6];
    tiny_format(small, sizeof small, "%s", "abcdefghij");
    KOAN_EQ_STR(/*__STR*/ "abcde", small);
    KOAN_EQ_SZ(/*__SZ*/ 5, strlen(small));
}

KOAN_LESSON(lesson_about_variadic, "About Variadic Functions",
    KOAN_CASE(a_count_tells_the_callee_how_many),
    KOAN_CASE(a_sentinel_ends_the_list),
    KOAN_CASE(variadic_arguments_are_promoted),
    KOAN_CASE(va_copy_is_needed_to_walk_a_list_twice),
    KOAN_CASE(v_functions_forward_a_va_list),
    KOAN_CASE(the_format_must_match_the_arguments),
    KOAN_CASE(assembling_a_tiny_printf)
);
