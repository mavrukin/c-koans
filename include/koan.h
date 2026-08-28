/*
 * koan.h — the tiny test framework the C Koans are built on.
 *
 * There is no external dependency here on purpose. Everything you need to run
 * the koans ships in this repository, and every line of this framework is
 * itself readable C23. When you finish the koans, come back and read this
 * file: you will understand all of it, including the setjmp/longjmp control
 * flow and the X-macro registration table.
 *
 * THE BLANKS
 * ----------
 * Koans are filled in by replacing a blank with the value you believe is
 * correct. Blanks are typed, because C is typed:
 *
 *     __        an int          KOAN_EQ_INT(__, 1 + 1);
 *     __SZ      a size_t        KOAN_EQ_SZ(__SZ, sizeof(int));
 *     __DBL     a double        KOAN_EQ_DBL(__DBL, 0.5 + 0.25, 1e-9);
 *     __CHR     a char          KOAN_EQ_CHR(__CHR, "abc"[0]);
 *     __STR     a string        KOAN_EQ_STR(__STR, greet());
 *     __BOOL    a bool          KOAN_TRUE(__BOOL);
 *     __PTR     a pointer       KOAN_EQ_PTR(__PTR, nullptr);
 *
 * A blank is a poison value. If you run a koan without filling it in, the
 * runner recognises the poison and tells you which blank you still owe it,
 * rather than showing you a confusing mismatch.
 *
 * A NOTE ON THE NAME `__`
 * -----------------------
 * Identifiers containing a double underscore are reserved to the
 * implementation (C23 7.1.3). We use `__` anyway because it is the koan
 * tradition and because it reads like a blank on the page. This is a
 * deliberate, contained exception, confined to this header. Do not do it in
 * production code. Now you know why, which is the point of a koan.
 */
#ifndef KOAN_H
#define KOAN_H

#include <setjmp.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Deliberate warnings                                                 */
/* ------------------------------------------------------------------ */

/*
 * A few koans provoke a compiler warning on purpose, because the warning is
 * the lesson: `flags & MASK == MASK` really does parse the wrong way, and you
 * should see why. Wrapping those lines keeps the rest of the build silent, so
 * that any warning you *do* see while solving a koan is one you introduced.
 *
 * Outside these markers, treat every warning as a defect.
 */
#if defined(__clang__)
#  define KOAN__PRAGMA(x) _Pragma(#x)
#  define KOAN_DELIBERATE_BEGIN                                               \
      KOAN__PRAGMA(clang diagnostic push)                                     \
      KOAN__PRAGMA(clang diagnostic ignored "-Wparentheses")                  \
      KOAN__PRAGMA(clang diagnostic ignored "-Wunused-value")                 \
      KOAN__PRAGMA(clang diagnostic ignored "-Wsign-compare")                 \
      KOAN__PRAGMA(clang diagnostic ignored "-Wunevaluated-expression")       \
      KOAN__PRAGMA(clang diagnostic ignored "-Wshift-op-parentheses")         \
      KOAN__PRAGMA(clang diagnostic ignored "-Wdangling-else")              \
      KOAN__PRAGMA(clang diagnostic ignored "-Wshadow")                       \
      KOAN__PRAGMA(clang diagnostic ignored "-Wunused-but-set-parameter")     \
      KOAN__PRAGMA(clang diagnostic ignored "-Wunused-but-set-variable")      \
      KOAN__PRAGMA(clang diagnostic ignored "-Wsizeof-array-argument")        \
      KOAN__PRAGMA(clang diagnostic ignored "-Wsizeof-pointer-div")           \
      KOAN__PRAGMA(clang diagnostic ignored "-Wbitfield-constant-conversion") \
      KOAN__PRAGMA(clang diagnostic ignored "-Wdeprecated-declarations")      \
      KOAN__PRAGMA(clang diagnostic ignored "-Wvla")                          \
      KOAN__PRAGMA(clang diagnostic ignored "-Winitializer-overrides")        \
      KOAN__PRAGMA(clang diagnostic ignored "-Wimplicit-fallthrough")
#  define KOAN_DELIBERATE_END KOAN__PRAGMA(clang diagnostic pop)
#elif defined(__GNUC__)
#  define KOAN__PRAGMA(x) _Pragma(#x)
#  define KOAN_DELIBERATE_BEGIN                                               \
      KOAN__PRAGMA(GCC diagnostic push)                                       \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wparentheses")                    \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wunused-value")                   \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wsign-compare")                   \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wdangling-else")                \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wshadow")                         \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wunused-but-set-parameter")       \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wunused-but-set-variable")        \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wsizeof-array-argument")          \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wsizeof-pointer-div")             \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wdeprecated-declarations")        \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wvla")                            \
      KOAN__PRAGMA(GCC diagnostic ignored "-Woverride-init")                  \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wimplicit-fallthrough")           \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wformat-truncation")              \
      KOAN__PRAGMA(GCC diagnostic ignored "-Wstringop-truncation")            \
      KOAN__PRAGMA(GCC diagnostic ignored "-Winfinite-recursion")             \
      KOAN__PRAGMA(GCC diagnostic ignored "-Woverflow")
#  define KOAN_DELIBERATE_END KOAN__PRAGMA(GCC diagnostic pop)
#else
#  define KOAN_DELIBERATE_BEGIN
#  define KOAN_DELIBERATE_END
#endif

/* ------------------------------------------------------------------ */
/* Lesson and case registration                                        */
/* ------------------------------------------------------------------ */

typedef void (*koan_fn)(void);

typedef struct {
    const char *name;
    koan_fn     fn;
} koan_case;

typedef struct {
    const char      *title;
    const char      *path;
    const koan_case *cases;
    size_t           ncases;
} koan_lesson;

/* Define one koan. The body is ordinary C. */
#define KOAN(name) static void name(void)

/* Reference a koan from the lesson's case list. */
#define KOAN_CASE(name) { #name, name }

/*
 * Close a koan file. `sym` is the exported lesson symbol, which must match
 * the name listed in koans/manifest.def so the runner can find it.
 */
#define KOAN_LESSON(sym, title, ...)                                          \
    static const koan_case koan__cases[] = { __VA_ARGS__ };                   \
    const koan_lesson sym = {                                                 \
        title, __FILE__, koan__cases,                                         \
        sizeof koan__cases / sizeof *koan__cases                              \
    }

/* ------------------------------------------------------------------ */
/* Blanks                                                              */
/* ------------------------------------------------------------------ */

#define KOAN_BLANK_INT  (-559038737)                 /* 0xDEADBEEF as int   */
#define KOAN_BLANK_SZ   ((size_t)0xDEADBEEFu)
#define KOAN_BLANK_CHR  ((char)0x07)                 /* BEL, never in a koan */
#define KOAN_BLANK_DBL  (-1.7976931348623157e+300)
#define KOAN_BLANK_BOOL (koan_blank_bool())

extern const char koan_blank_string[];
#define KOAN_BLANK_STR  (koan_blank_string)
#define KOAN_BLANK_PTR  ((void *)koan_blank_string)

bool koan_blank_bool(void);

#define __      KOAN_BLANK_INT
#define __SZ    KOAN_BLANK_SZ
#define __CHR   KOAN_BLANK_CHR
#define __DBL   KOAN_BLANK_DBL
#define __STR   KOAN_BLANK_STR
#define __BOOL  KOAN_BLANK_BOOL
#define __PTR   KOAN_BLANK_PTR

/* ------------------------------------------------------------------ */
/* Failure plumbing                                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    KOAN_OK = 0,
    KOAN_FAILED,        /* an assertion compared unequal          */
    KOAN_UNFILLED,      /* a blank was left in place              */
    KOAN_NEEDS_WORK     /* KOAN_PENDING() — not attempted yet     */
} koan_status;

typedef struct {
    koan_status status;
    const char *file;
    int         line;
    const char *expr;
    char        expected[512];
    char        actual[512];
    char        note[512];
} koan_result;

/* Internal: used by the macros below, not meant to be called directly. */
extern jmp_buf     koan__escape;
extern koan_result koan__current;

void koan__begin(const char *file, int line, const char *expr);
void koan__set_expected(const char *fmt, ...);
void koan__set_actual(const char *fmt, ...);
void koan__set_note(const char *fmt, ...);
[[noreturn]] void koan__abort(koan_status status);

bool koan__is_blank_str(const char *s);
bool koan__is_blank_ptr(const void *p);
bool koan__is_blank_dbl(double d);

/* Escape hatch used by every assertion macro. */
#define KOAN__FAIL(st) koan__abort(st)

#define KOAN__ENTER(expr_text) koan__begin(__FILE__, __LINE__, expr_text)

/* ------------------------------------------------------------------ */
/* Assertions                                                          */
/* ------------------------------------------------------------------ */

/*
 * Every assertion follows the same shape: record where we are, check for an
 * unfilled blank first (so the message is about the blank, not the value),
 * then compare. On mismatch we longjmp back to the runner, which means the
 * koan stops at its first failure — one lesson at a time.
 */

#define KOAN_TRUE(cond)                                                       \
    do {                                                                      \
        KOAN__ENTER(#cond);                                                   \
        bool koan__c = (cond);                                                \
        if (koan__blank_bool_seen) {                                          \
            koan__set_note("fill in __BOOL with true or false");              \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (!koan__c) {                                                       \
            koan__set_expected("true");                                       \
            koan__set_actual("false");                                        \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

#define KOAN_FALSE(cond)                                                      \
    do {                                                                      \
        KOAN__ENTER(#cond);                                                   \
        bool koan__c = (cond);                                                \
        if (koan__blank_bool_seen) {                                          \
            koan__set_note("fill in __BOOL with true or false");              \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (koan__c) {                                                        \
            koan__set_expected("false");                                      \
            koan__set_actual("true");                                         \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

#define KOAN_EQ_INT(expected, actual)                                         \
    do {                                                                      \
        KOAN__ENTER(#expected " == " #actual);                                \
        long long koan__e = (long long)(expected);                            \
        long long koan__a = (long long)(actual);                              \
        if (koan__e == (long long)KOAN_BLANK_INT) {                           \
            koan__set_note("replace __ with the int you expect");             \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (koan__e != koan__a) {                                             \
            koan__set_expected("%lld", koan__e);                              \
            koan__set_actual("%lld", koan__a);                                \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

#define KOAN_EQ_SZ(expected, actual)                                          \
    do {                                                                      \
        KOAN__ENTER(#expected " == " #actual);                                \
        size_t koan__e = (size_t)(expected);                                  \
        size_t koan__a = (size_t)(actual);                                    \
        if (koan__e == KOAN_BLANK_SZ) {                                       \
            koan__set_note("replace __SZ with the size_t you expect");        \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (koan__e != koan__a) {                                             \
            koan__set_expected("%zu", koan__e);                               \
            koan__set_actual("%zu", koan__a);                                 \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

#define KOAN_EQ_CHR(expected, actual)                                         \
    do {                                                                      \
        KOAN__ENTER(#expected " == " #actual);                                \
        char koan__e = (char)(expected);                                      \
        char koan__a = (char)(actual);                                        \
        if (koan__e == KOAN_BLANK_CHR) {                                      \
            koan__set_note("replace __CHR with the char you expect");         \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (koan__e != koan__a) {                                             \
            koan__set_expected("'%c' (%d)", koan__e, (int)koan__e);           \
            koan__set_actual("'%c' (%d)", koan__a, (int)koan__a);             \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

#define KOAN_EQ_DBL(expected, actual, eps)                                    \
    do {                                                                      \
        KOAN__ENTER(#expected " ~= " #actual);                                \
        double koan__e = (double)(expected);                                  \
        double koan__a = (double)(actual);                                    \
        if (koan__is_blank_dbl(koan__e)) {                                    \
            koan__set_note("replace __DBL with the double you expect");       \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (!koan__near(koan__e, koan__a, (double)(eps))) {                   \
            koan__set_expected("%.17g", koan__e);                             \
            koan__set_actual("%.17g", koan__a);                               \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

#define KOAN_EQ_STR(expected, actual)                                         \
    do {                                                                      \
        KOAN__ENTER(#expected " == " #actual);                                \
        const char *koan__e = (expected);                                     \
        const char *koan__a = (actual);                                       \
        if (koan__is_blank_str(koan__e)) {                                    \
            koan__set_note("replace __STR with the string you expect");       \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (!koan__streq(koan__e, koan__a)) {                                 \
            koan__set_expected("\"%s\"", koan__e ? koan__e : "(null)");       \
            koan__set_actual("\"%s\"", koan__a ? koan__a : "(null)");         \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

#define KOAN_EQ_PTR(expected, actual)                                         \
    do {                                                                      \
        KOAN__ENTER(#expected " == " #actual);                                \
        const void *koan__e = (const void *)(expected);                       \
        const void *koan__a = (const void *)(actual);                         \
        if (koan__is_blank_ptr(koan__e)) {                                    \
            koan__set_note("replace __PTR with the pointer you expect");      \
            KOAN__FAIL(KOAN_UNFILLED);                                        \
        }                                                                     \
        if (koan__e != koan__a) {                                             \
            koan__set_expected("%p", koan__e);                                \
            koan__set_actual("%p", koan__a);                                  \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

/* Compare n bytes. Useful once you reach memcpy, structs and serialisation. */
#define KOAN_EQ_MEM(expected, actual, n)                                      \
    do {                                                                      \
        KOAN__ENTER(#expected " == " #actual);                                \
        if (!koan__memeq((expected), (actual), (n))) {                        \
            koan__describe_mem((expected), (actual), (n));                    \
            KOAN__FAIL(KOAN_FAILED);                                          \
        }                                                                     \
    } while (0)

/* Mark a koan you have not started. The runner treats it as the next task. */
#define KOAN_PENDING(why)                                                     \
    do {                                                                      \
        KOAN__ENTER("KOAN_PENDING");                                          \
        koan__set_note("%s", (why));                                          \
        KOAN__FAIL(KOAN_NEEDS_WORK);                                          \
    } while (0)

/* Fail on purpose, with a message. Handy inside error-path koans. */
#define KOAN_FAIL(...)                                                        \
    do {                                                                      \
        KOAN__ENTER("KOAN_FAIL");                                             \
        koan__set_note(__VA_ARGS__);                                          \
        KOAN__FAIL(KOAN_FAILED);                                              \
    } while (0)

/* Helpers the macros lean on. */
bool koan__streq(const char *a, const char *b);
bool koan__near(double a, double b, double eps);
bool koan__memeq(const void *a, const void *b, size_t n);
void koan__describe_mem(const void *a, const void *b, size_t n);

/* Set when __BOOL is evaluated, so KOAN_TRUE can report an unfilled blank. */
extern bool koan__blank_bool_seen;

#endif /* KOAN_H */
