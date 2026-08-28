/*
 * koan.c — implementation of the assertion framework declared in koan.h.
 */
#include "koan.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * The blank string is compared by address, not by content, so that a koan
 * whose answer genuinely is this text still behaves sensibly.
 */
const char koan_blank_string[] = "<-- fill me in -->";

bool koan__blank_bool_seen = false;

jmp_buf     koan__escape;
koan_result koan__current;

bool koan_blank_bool(void)
{
    koan__blank_bool_seen = true;
    return false;
}

void koan__begin(const char *file, int line, const char *expr)
{
    koan__blank_bool_seen = false;
    koan__current.status = KOAN_OK;
    koan__current.file   = file;
    koan__current.line   = line;
    koan__current.expr   = expr;
    koan__current.expected[0] = '\0';
    koan__current.actual[0]   = '\0';
    koan__current.note[0]     = '\0';
}

static void set_field(char *dst, size_t cap, const char *fmt, va_list ap)
{
    vsnprintf(dst, cap, fmt, ap);
}

void koan__set_expected(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_field(koan__current.expected, sizeof koan__current.expected, fmt, ap);
    va_end(ap);
}

void koan__set_actual(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_field(koan__current.actual, sizeof koan__current.actual, fmt, ap);
    va_end(ap);
}

void koan__set_note(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    set_field(koan__current.note, sizeof koan__current.note, fmt, ap);
    va_end(ap);
}

[[noreturn]] void koan__abort(koan_status status)
{
    koan__current.status = status;
    longjmp(koan__escape, 1);
}

bool koan__is_blank_str(const char *s)
{
    return s == koan_blank_string;
}

bool koan__is_blank_ptr(const void *p)
{
    return p == (const void *)koan_blank_string;
}

bool koan__is_blank_dbl(double d)
{
    return d == KOAN_BLANK_DBL;
}

bool koan__streq(const char *a, const char *b)
{
    if (a == b)            return true;
    if (!a || !b)          return false;
    return strcmp(a, b) == 0;
}

bool koan__near(double a, double b, double eps)
{
    if (isnan(a) && isnan(b)) return true;
    if (isinf(a) || isinf(b)) return a == b;
    double diff = fabs(a - b);
    return diff <= eps;
}

bool koan__memeq(const void *a, const void *b, size_t n)
{
    if (a == b)   return true;
    if (!a || !b) return false;
    return memcmp(a, b, n) == 0;
}

/* Render the first differing byte, which is nearly always the useful one. */
void koan__describe_mem(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = a, *pb = b;

    if (!pa || !pb) {
        koan__set_expected("%s", pa ? "<buffer>" : "(null)");
        koan__set_actual("%s", pb ? "<buffer>" : "(null)");
        return;
    }

    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) {
            koan__set_expected("byte[%zu] = 0x%02x", i, pa[i]);
            koan__set_actual("byte[%zu] = 0x%02x", i, pb[i]);
            return;
        }
    }
    koan__set_expected("%zu identical bytes", n);
    koan__set_actual("%zu identical bytes", n);
}
