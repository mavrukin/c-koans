/*
 * About C23
 *
 * C23 (ISO/IEC 9899:2024) is the largest revision to C in twenty years. Much
 * of it is quiet cleanup — `bool` is finally a keyword, `()` finally means
 * `(void)` — but several additions genuinely change how you write the
 * language, and they are the subject of this lesson.
 *
 * You have already used some of these without comment: nullptr, binary
 * literals, digit separators, [[fallthrough]], enums with a fixed underlying
 * type, empty initialisers, and #elifdef. Here they are named.
 */
#include "koan.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * bool, true and false are keywords now. <stdbool.h> still exists so that old
 * code keeps working, but you no longer need it.
 */
KOAN(bool_is_a_keyword)
{
    bool yes = true;
    bool no  = false;

    KOAN_EQ_INT(__, yes);
    KOAN_EQ_INT(__, no);
    KOAN_EQ_SZ(__SZ, sizeof(bool));

    /* Converting to bool normalises to exactly 0 or 1, unlike a cast to int. */
    KOAN_EQ_INT(__, (bool)42);
    KOAN_EQ_INT(__, (int)42);
}

/*
 * `constexpr` declares a true compile-time constant with a type. It replaces
 * the two older workarounds — `#define` (no type, no scope) and `enum` (int
 * only) — and unlike `const`, it may be used where a constant expression is
 * required.
 */
constexpr int    BUFFER_SIZE = 64;
constexpr double GOLDEN      = 1.618;

KOAN(constexpr_is_a_typed_compile_time_constant)
{
    /* It is a constant expression, so it may size an array. */
    char buf[BUFFER_SIZE];
    KOAN_EQ_SZ(__SZ, sizeof buf);

    /* And it has a type, unlike a #define. */
    KOAN_EQ_SZ(__SZ, sizeof BUFFER_SIZE);
    KOAN_EQ_SZ(__SZ, sizeof GOLDEN);

    /* A plain `const int` is NOT a constant expression in C, which is the
     * distinction that makes constexpr worth having. */
    KOAN_EQ_INT(__, BUFFER_SIZE);
}

/*
 * `static_assert` checks a condition at compile time. As of C23 the message
 * is optional, so it reads like an ordinary assertion. It costs nothing at
 * runtime and fails the build rather than the program.
 */
static_assert(BUFFER_SIZE == 64);
static_assert(sizeof(int) >= 4, "this code assumes int is at least 32 bits");
static_assert(CHAR_BIT == 8, "byte-oriented code assumes 8-bit bytes");

KOAN(static_assert_checks_at_compile_time)
{
    /* If any assertion above were false, this file would not have compiled.
     * That the koan runs at all is the proof. */
    static_assert(sizeof(char) == 1);
    KOAN_TRUE(true);
}

/*
 * `auto` infers a variable's type from its initialiser. In C23 it is a real
 * type inference, not the storage class it used to be. Use it where the type
 * is obvious or unspeakably long — not to hide what a variable is.
 */
KOAN(auto_infers_the_type)
{
    auto n = 42;            /* int    */
    auto d = 3.5;           /* double */
    auto s = "text";        /* const char* */

    KOAN_EQ_SZ(__SZ, sizeof n);
    KOAN_EQ_SZ(__SZ, sizeof d);
    KOAN_EQ_INT(__, n);
    KOAN_EQ_STR(__STR, s);
}

/*
 * `typeof` yields the type of an expression, and `typeof_unqual` strips the
 * qualifiers. Together they make type-safe macros possible without repeating
 * a type name the caller already supplied.
 */
#define SWAP(a, b) do {          \
        typeof(a) koan_tmp = (a); \
        (a) = (b);                \
        (b) = koan_tmp;           \
    } while (0)

KOAN(typeof_reads_a_type_from_an_expression)
{
    int x = 1, y = 2;
    SWAP(x, y);
    KOAN_EQ_INT(__, x);
    KOAN_EQ_INT(__, y);

    /* The same macro works for any type, with no casts and no size argument. */
    double p = 1.5, q = 2.5;
    SWAP(p, q);
    KOAN_EQ_DBL(__DBL, p, 1e-12);

    /* typeof_unqual removes const, which typeof would have preserved. */
    const int locked = 7;
    typeof_unqual(locked) writable = locked;
    writable = 8;
    KOAN_EQ_INT(__, writable);
    KOAN_EQ_INT(__, locked);
}

/*
 * Attributes annotate declarations in a standard, portable syntax, replacing
 * a thicket of __attribute__ and __declspec spellings.
 *
 * [[nodiscard]] is the one you should reach for constantly: it makes ignoring
 * a return value a diagnostic, which is exactly what you want for any
 * function that can fail.
 */
[[nodiscard]] static int must_use(int n) { return n * 2; }

[[deprecated("use must_use instead")]]
static int old_way(int n) { return n * 2; }

static int unused_parameter_ok([[maybe_unused]] int ignored) { return 1; }

KOAN(attributes_are_standard_now)
{
    /* Ignoring this would warn, so we use it. */
    int doubled = must_use(21);
    KOAN_EQ_INT(__, doubled);

    /* Calling a deprecated function warns but still works. */
KOAN_DELIBERATE_BEGIN
    KOAN_EQ_INT(__, old_way(21));
KOAN_DELIBERATE_END

    KOAN_EQ_INT(__, unused_parameter_ok(0));

    [[maybe_unused]] int scratch = 0;
}

/*
 * [[noreturn]] tells the compiler a function never comes back, which lets it
 * reason correctly about the code after a call. Its C11 spelling was the
 * clumsy _Noreturn.
 */
[[noreturn]] static void always_escapes(void)
{
    KOAN_FAIL("this function never returns normally");
}

KOAN(noreturn_functions_do_not_come_back)
{
    /* We do not call always_escapes — it would fail the koan by design.
     * Its existence is the point: [[noreturn]] is part of its type, and the
     * compiler uses that to reason about the code following any call. */
    void (*escape)(void) = always_escapes;
    KOAN_TRUE(escape != nullptr);
}

/*
 * `_BitInt(N)` is an integer of exactly N bits — any N, not just the handful
 * the machine offers. It is exact-width arithmetic for protocol fields,
 * fixed-point maths and hardware registers, with defined wrapping for the
 * unsigned form.
 */
KOAN(bitint_gives_you_any_width)
{
    _BitInt(4)           nibble = 7;
    unsigned _BitInt(12) twelve = 4095;

    KOAN_EQ_INT(__, (int)nibble);
    KOAN_EQ_INT(__, (int)twelve);

    /* A 4-bit signed integer holds -8..7, and that is the whole range. */
    _BitInt(4) most = 7;
    _BitInt(4) least = -8;
    KOAN_EQ_INT(__, (int)most);
    KOAN_EQ_INT(__, (int)least);

    /* Unsigned _BitInt wraps modulo 2^N, exactly like any unsigned type. */
    twelve = (unsigned _BitInt(12))(twelve + 1u);
    KOAN_EQ_INT(__, (int)twelve);
}

/*
 * `<stdckdint.h>` provides checked arithmetic: the operation is performed and
 * you are told whether it overflowed, in one step. This is the correct way to
 * handle signed overflow, which you cannot legally detect after the fact.
 */
#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#define KOAN_HAVE_CKDINT 1
#else
#define KOAN_HAVE_CKDINT 0
#endif

KOAN(checked_arithmetic_reports_overflow)
{
#if KOAN_HAVE_CKDINT
    int result = 0;

    /* Returns false when the result is exact. */
    KOAN_EQ_INT(__, ckd_add(&result, 20, 22));
    KOAN_EQ_INT(__, result);

    /* Returns true when it overflowed — and result holds the wrapped value,
     * which you must then not use. */
    KOAN_EQ_INT(__, ckd_add(&result, INT_MAX, 1));

    KOAN_EQ_INT(__, ckd_mul(&result, 1000, 1000));
    KOAN_EQ_INT(__, result);
    KOAN_EQ_INT(__, ckd_mul(&result, INT_MAX, 2));

    KOAN_EQ_INT(__, ckd_sub(&result, INT_MIN, 1));
#else
    /* Your libc has not shipped <stdckdint.h> yet. The koan still holds:
     * the guard you would write by hand is the one from about_types. */
    int a = INT_MAX;
    KOAN_EQ_INT(__, a > INT_MAX - 1);
#endif
}

/*
 * `#embed` puts the contents of a file into your program as an initialiser
 * list, at compile time. Before C23 this needed a build step and a generated
 * .c file; now it is one directive.
 *
 * It is also the newest thing in this lesson and the last to be implemented:
 * it needs GCC 15+ or Clang 19+. `__has_embed` is how you ask before relying
 * on it — the same pattern as `__has_include`, and the reason both exist.
 */
#ifdef __has_embed
#  if __has_embed("assets/greeting.txt")
#    define KOAN_HAVE_EMBED 1
#  endif
#endif

KOAN(embed_includes_binary_data)
{
#ifdef KOAN_HAVE_EMBED
    static const unsigned char greeting[] = {
#embed "assets/greeting.txt"
        , '\0'                      /* #embed does not add a terminator */
    };

    KOAN_EQ_STR(__STR, (const char *)greeting);
    KOAN_EQ_SZ(__SZ, sizeof greeting);   /* 17 bytes plus our NUL */
#else
    /*
     * Your compiler predates #embed, so the koan cannot be asked and passes
     * unanswered. The feature is real and worth reading about above; a newer
     * compiler will turn this into a genuine exercise.
     */
    KOAN_TRUE(true);
#endif
}

/*
 * Bringing it together.
 *
 * A fixed-point decimal type built on _BitInt, with constexpr scaling, a
 * static_assert guarding the invariant, checked arithmetic on the way in, and
 * nodiscard so a failed operation cannot be ignored. Every feature above,
 * doing one job.
 */
constexpr int FIXED_SCALE = 100;          /* two decimal places */

typedef struct { _BitInt(40) raw; } Fixed;

static_assert(FIXED_SCALE == 100, "the formatter below assumes two places");

[[nodiscard]] static bool fixed_from_parts(Fixed *out, int whole, int hundredths)
{
    if (hundredths < 0 || hundredths >= FIXED_SCALE) return false;
    out->raw = (_BitInt(40))whole * FIXED_SCALE + hundredths;
    return true;
}

[[nodiscard]] static bool fixed_add(Fixed *out, Fixed a, Fixed b)
{
    out->raw = a.raw + b.raw;
    return true;
}

static const char *fixed_text(Fixed f, char *buf, size_t cap)
{
    long long v = (long long)f.raw;
    snprintf(buf, cap, "%lld.%02lld", v / FIXED_SCALE, v % FIXED_SCALE);
    return buf;
}

KOAN(assembling_a_fixed_point_type)
{
    char  buf[32];
    Fixed price, tax, total;

    KOAN_EQ_INT(__, fixed_from_parts(&price, 19, 99));
    KOAN_EQ_STR(__STR, fixed_text(price, buf, sizeof buf));

    KOAN_EQ_INT(__, fixed_from_parts(&tax, 1, 60));
    KOAN_EQ_INT(__, fixed_add(&total, price, tax));
    KOAN_EQ_STR(__STR, fixed_text(total, buf, sizeof buf));

    /* The invariant is enforced rather than assumed. */
    KOAN_EQ_INT(__, fixed_from_parts(&price, 5, 100));
    KOAN_EQ_INT(__, fixed_from_parts(&price, 5, -1));
}

KOAN_LESSON(lesson_about_c23_features, "About C23",
    KOAN_CASE(bool_is_a_keyword),
    KOAN_CASE(constexpr_is_a_typed_compile_time_constant),
    KOAN_CASE(static_assert_checks_at_compile_time),
    KOAN_CASE(auto_infers_the_type),
    KOAN_CASE(typeof_reads_a_type_from_an_expression),
    KOAN_CASE(attributes_are_standard_now),
    KOAN_CASE(noreturn_functions_do_not_come_back),
    KOAN_CASE(bitint_gives_you_any_width),
    KOAN_CASE(checked_arithmetic_reports_overflow),
    KOAN_CASE(embed_includes_binary_data),
    KOAN_CASE(assembling_a_fixed_point_type)
);
