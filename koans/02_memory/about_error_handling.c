/*
 * About Error Handling
 *
 * C has no exceptions. Every failure travels back as a return value, and the
 * caller is trusted to look. That trust is misplaced roughly as often as
 * people say it is — so the discipline is: decide what a failed call returns,
 * say so, and check it every time.
 */
#include "koan.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * errno is set on failure by library and system calls, and is NOT cleared on
 * success. Reading it after a call that succeeded tells you nothing about that
 * call. Clear it first, or only read it once a call has already told you it
 * failed.
 */
KOAN(errno_is_only_meaningful_after_a_failure)
{
    errno = 0;
    FILE *missing = fopen("/koan/no/such/path", "r");

    KOAN_TRUE(missing == nullptr);
    KOAN_EQ_INT(__, errno);

    /*
     * A successful call leaves errno unspecified: it may clear it, leave it,
     * or set it to something of its own. So clear it immediately before the
     * call you mean to test, and only read it once that call has told you it
     * failed. Nothing is asserted about errno below, because nothing may be.
     */
    errno = 0;
    FILE *fine = tmpfile();
    KOAN_TRUE(fine != nullptr);
    fclose(fine);
}

/*
 * strerror turns an errno into text. Do not hold on to the pointer: the next
 * call may overwrite it.
 */
KOAN(strerror_names_the_code)
{
    KOAN_TRUE(strlen(strerror(ENOENT)) > 0);
    KOAN_TRUE(strstr(strerror(ENOENT), "o such file") != nullptr);

    /* The value zero is not an error, and has no useful message. */
    KOAN_TRUE(strerror(0) != nullptr);
}

/*
 * The three common return conventions. Which one a function uses is part of
 * its contract, and you must read it rather than guess.
 *
 *   pointer-returning   null on failure          (malloc, fopen)
 *   int-returning       negative or -1           (most of POSIX)
 *   bool-returning      false, with an out-param (your own code, usually)
 */
typedef enum { PARSE_OK, PARSE_EMPTY, PARSE_JUNK, PARSE_RANGE } ParseError;

/*
 * strtol reports three different failures three different ways, and getting
 * this right is a rite of passage:
 *
 *   nothing consumed  -> end == start
 *   trailing junk     -> *end != '\0'
 *   out of range      -> errno == ERANGE and the result is LONG_MIN/MAX
 */
static ParseError parse_long(const char *s, long *out)
{
    errno = 0;
    char *end = nullptr;
    long  value = strtol(s, &end, 10);

    if (end == s)                       return PARSE_EMPTY;
    if (*end != '\0')                   return PARSE_JUNK;
    if (errno == ERANGE)                return PARSE_RANGE;

    *out = value;
    return PARSE_OK;
}

KOAN(strtol_distinguishes_three_failures)
{
    long v = 0;

    KOAN_EQ_INT(__, parse_long("42", &v));
    KOAN_EQ_INT(__, (int)v);

    KOAN_EQ_INT(__, parse_long("-7", &v));
    KOAN_EQ_INT(__, (int)v);

    KOAN_EQ_INT(__, parse_long("", &v));
    KOAN_EQ_INT(__, parse_long("abc", &v));
    KOAN_EQ_INT(__, parse_long("42abc", &v));
    KOAN_EQ_INT(__, parse_long("42 ", &v));
    KOAN_EQ_INT(__,
                parse_long("999999999999999999999999999", &v));

    /* Leading whitespace is consumed, which surprises people. */
    KOAN_EQ_INT(__, parse_long("   42", &v));

    /* atoi cannot report any of this. It returns 0 for "abc" and for "0"
     * alike, which is why it should never appear in code you keep. */
    KOAN_EQ_INT(__, atoi("abc"));
    KOAN_EQ_INT(__, atoi("0"));
}

/*
 * An error enum plus a table of messages beats scattered magic numbers, and
 * costs nothing. Note that the table is indexed by the enum, so adding a
 * member without a message is caught by static_assert rather than at runtime.
 */
static const char *const PARSE_MESSAGES[] = {
    [PARSE_OK]    = "ok",
    [PARSE_EMPTY] = "no digits found",
    [PARSE_JUNK]  = "trailing characters",
    [PARSE_RANGE] = "out of range",
};

static_assert(sizeof PARSE_MESSAGES / sizeof *PARSE_MESSAGES == PARSE_RANGE + 1,
              "every ParseError needs a message");

KOAN(error_codes_deserve_names)
{
    KOAN_EQ_STR(__STR, PARSE_MESSAGES[PARSE_JUNK]);
    KOAN_EQ_STR(__STR, PARSE_MESSAGES[PARSE_RANGE]);
}

/*
 * assert() states an invariant you believe cannot be false. It is for
 * programmer error, never for input validation: NDEBUG removes it entirely,
 * so anything with a side effect inside an assert disappears in a release
 * build.
 */
KOAN(assert_is_for_impossible_states_not_bad_input)
{
    int  calls = 0;
    int  n = 5;

    assert(n > 0);                  /* an invariant: fine */

    /* This is the bug: the increment vanishes under -DNDEBUG. */
    assert(++calls == 1);

#ifdef NDEBUG
    KOAN_EQ_INT(0, calls);
#else
    KOAN_EQ_INT(__, calls);
#endif
}

/*
 * setjmp/longjmp is C's non-local jump: it unwinds the stack to a saved point.
 * It does NOT run any cleanup, so anything allocated between the setjmp and
 * the longjmp is leaked unless you free it yourself.
 *
 * setjmp returns 0 when it saves, and the value passed to longjmp when jumped
 * to. Use it sparingly — this whole koan runner is built on it, which is about
 * the right frequency.
 */
static jmp_buf escape_hatch;

static void fail_deep(int depth)
{
    if (depth == 0) longjmp(escape_hatch, 42);
    fail_deep(depth - 1);
}

KOAN(setjmp_saves_a_place_to_return_to)
{
    int landed = setjmp(escape_hatch);

    if (landed == 0) {
        fail_deep(5);               /* jumps back to the setjmp above */
        KOAN_FAIL("longjmp should not have returned here");
    }

    /* Second time through, with the value longjmp was given. */
    KOAN_EQ_INT(__, landed);
}

/*
 * Variables modified between setjmp and longjmp must be `volatile`, or their
 * value after the jump is indeterminate — the compiler may have kept them in
 * a register that the jump discards.
 */
KOAN(locals_crossing_a_longjmp_must_be_volatile)
{
    volatile int survived = 0;
    jmp_buf      here;

    if (setjmp(here) == 0) {
        survived = 1;               /* volatile, so this is guaranteed to stick */
        longjmp(here, 1);
    }

    KOAN_EQ_INT(__, survived);
}

/*
 * Bringing it together.
 *
 * A parser for "key=value" pairs that reports precisely what went wrong and
 * where, cleans up on every path, and never writes past its output. Every
 * failure below is one a real config parser must handle.
 */
typedef struct {
    ParseError code;
    size_t     offset;          /* where in the input it went wrong */
    char       key[32];
    long       value;
} Setting;

static bool parse_setting(const char *line, Setting *out)
{
    *out = (Setting){ .code = PARSE_OK };

    const char *eq = strchr(line, '=');
    if (!eq) {
        out->code   = PARSE_JUNK;
        out->offset = strlen(line);
        return false;
    }

    size_t key_len = (size_t)(eq - line);
    if (key_len == 0 || key_len >= sizeof out->key) {
        out->code   = key_len == 0 ? PARSE_EMPTY : PARSE_RANGE;
        out->offset = 0;
        return false;
    }
    memcpy(out->key, line, key_len);
    out->key[key_len] = '\0';

    ParseError err = parse_long(eq + 1, &out->value);
    if (err != PARSE_OK) {
        out->code   = err;
        out->offset = key_len + 1;
        return false;
    }
    return true;
}

KOAN(assembling_a_failing_parser)
{
    Setting s;

    KOAN_TRUE(parse_setting("timeout=30", &s));
    KOAN_EQ_STR(__STR, s.key);
    KOAN_EQ_INT(__, (int)s.value);

    /* No separator at all: the offset points past the end of the input. */
    KOAN_EQ_INT(__, parse_setting("timeout", &s));
    KOAN_EQ_INT(__, s.code);
    KOAN_EQ_SZ(__SZ, s.offset);

    /* Empty key. */
    KOAN_EQ_INT(__, parse_setting("=30", &s));
    KOAN_EQ_INT(__, s.code);

    /* Bad value: the offset points at the value, not the start of the line. */
    KOAN_EQ_INT(__, parse_setting("timeout=soon", &s));
    KOAN_EQ_INT(__, s.code);
    KOAN_EQ_SZ(__SZ, s.offset);

    /* Value out of range is a different failure from value being junk. */
    KOAN_EQ_INT(__, parse_setting("timeout=99999999999999999999", &s));
    KOAN_EQ_INT(__, s.code);

    /* An over-long key is refused rather than truncated into the buffer. */
    char  long_line[128];
    memset(long_line, 'k', 64);
    memcpy(long_line + 64, "=1", 3);
    KOAN_EQ_INT(__, parse_setting(long_line, &s));
    KOAN_EQ_INT(__, s.code);
}

KOAN_LESSON(lesson_about_error_handling, "About Error Handling",
    KOAN_CASE(errno_is_only_meaningful_after_a_failure),
    KOAN_CASE(strerror_names_the_code),
    KOAN_CASE(strtol_distinguishes_three_failures),
    KOAN_CASE(error_codes_deserve_names),
    KOAN_CASE(assert_is_for_impossible_states_not_bad_input),
    KOAN_CASE(setjmp_saves_a_place_to_return_to),
    KOAN_CASE(locals_crossing_a_longjmp_must_be_volatile),
    KOAN_CASE(assembling_a_failing_parser)
);
