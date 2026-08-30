/*
 * About Types
 *
 * C has no runtime type information. A type is a compile-time promise about
 * how many bytes a value occupies and how those bytes are to be read. Get the
 * promise wrong and nothing warns you at runtime — the bytes are simply
 * reinterpreted.
 *
 * These koans avoid asserting absolute sizes where the standard does not fix
 * them. `sizeof(int)` is 4 on every machine you are likely to use, but the
 * standard only guarantees a minimum range. Where a size is not fixed, the
 * koan asks about the *relationship*, which is guaranteed.
 */
#include "koan.h"

#include <limits.h>
#include <stdint.h>

/*
 * `sizeof` asks how many bytes a type occupies. It is not a function — it is
 * built into the language, and the compiler works out the answer while
 * building the program.
 *
 * What it gives back is not an int but a `size_t`: the type C uses for sizes
 * and counts. It can never be negative, because nothing has a negative size.
 *
 * That is a new type, so it has its own blank: `__SZ`. You write the number
 * exactly as you would an int; the blank simply records that the value is a
 * size, so the runner compares it as one.
 *
 * By definition sizeof(char) is 1: a char is the unit C measures everything
 * else in.
 */
KOAN(char_is_the_unit_of_size)
{
    KOAN_EQ_SZ(/*__SZ*/ 1, sizeof(char));
    KOAN_EQ_SZ(/*__SZ*/ 1, sizeof(signed char));
    KOAN_EQ_SZ(/*__SZ*/ 1, sizeof(unsigned char));
}

/*
 * A byte is CHAR_BIT bits. That is 8 everywhere you will meet, but the
 * standard says only "at least 8" — which is why the macro exists.
 */
KOAN(a_byte_is_char_bit_bits)
{
    KOAN_EQ_INT(/*__*/ 8, CHAR_BIT);
}

/*
 * The standard fixes an ordering of the integer sizes, not the sizes
 * themselves. These relationships hold on every conforming implementation.
 */
KOAN(integer_sizes_are_ordered_not_fixed)
{
    KOAN_TRUE(sizeof(char) <= sizeof(short));
    KOAN_TRUE(sizeof(short) <= sizeof(int));
    KOAN_TRUE(sizeof(int) <= sizeof(long));
    KOAN_TRUE(sizeof(long) <= sizeof(long long));

    /* And the minimum widths the standard does guarantee. The cast is not
     * decoration: sizeof yields size_t, and comparing it against a signed
     * int is the conversion trap this lesson is about. */
    KOAN_TRUE(sizeof(short) * CHAR_BIT >= (size_t)/*__*/ 16);
    KOAN_TRUE(sizeof(long) * CHAR_BIT >= (size_t)/*__*/ 32);
    KOAN_TRUE(sizeof(long long) * CHAR_BIT >= (size_t)/*__*/ 64);
}

/*
 * When you need a size guaranteed exactly, ask for it by name. The types in
 * <stdint.h> are exact-width and portable, and are what you should reach for
 * whenever the width actually matters (file formats, wire protocols, hardware).
 */
KOAN(exact_widths_have_names)
{
    KOAN_EQ_SZ(/*__SZ*/ 1, sizeof(int8_t));
    KOAN_EQ_SZ(/*__SZ*/ 2, sizeof(int16_t));
    KOAN_EQ_SZ(/*__SZ*/ 4, sizeof(int32_t));
    KOAN_EQ_SZ(/*__SZ*/ 8, sizeof(int64_t));
}

/*
 * Every integer type has a range, and <limits.h> names its edges.
 * Two's complement is mandatory as of C23: the negative end always reaches
 * one further than the positive end.
 */
KOAN(signed_ranges_are_asymmetric)
{
    KOAN_EQ_INT(/*__*/ 127, SCHAR_MAX);
    KOAN_EQ_INT(/*__*/ -128, SCHAR_MIN);

    /* The asymmetry, stated generally: */
    KOAN_TRUE(INT_MIN + INT_MAX == /*__*/ -1);
}

/*
 * Unsigned arithmetic is modular. It does not overflow; it wraps, and the
 * standard defines exactly what it wraps to. This is well-defined behaviour
 * that you may rely on.
 */
KOAN(unsigned_arithmetic_wraps)
{
    unsigned char b = 255;
    b++;
    KOAN_EQ_INT(/*__*/ 0, b);

    unsigned int u = 0;
    u--;
    KOAN_TRUE(u == UINT_MAX);
}

/*
 * Signed overflow is a different matter entirely: it is undefined behaviour.
 * The compiler is permitted to assume it never happens, and optimises on that
 * assumption. Never "test" for it after the fact — check before you compute.
 *
 * (C23 gives you a safe way to do this: see about_fixed_width_ints.)
 */
KOAN(signed_overflow_must_be_prevented_not_detected)
{
    int a = INT_MAX;
    int b = 1;

    /* The check that is actually correct: performed before the addition. */
    bool would_overflow = (b > 0 && a > INT_MAX - b);
    KOAN_EQ_INT(/*__*/ 1, would_overflow);

    /* Had a been smaller, the same guard would have let the addition through. */
    int c = 10;
    KOAN_EQ_INT(/*__*/ 0, (b > 0 && c > INT_MAX - b));
}

/*
 * Small integer types are promoted to int before arithmetic. This is the
 * single most surprising rule in the language, and the source of a great many
 * bugs. Note what happens to the type, not just the value.
 */
KOAN(small_types_promote_to_int)
{
    unsigned char x = 200;
    unsigned char y = 100;

    /* x + y is computed as int, so it does NOT wrap at 255. */
    KOAN_EQ_INT(/*__*/ 300, x + y);

    /* Only storing it back into an unsigned char truncates. */
    unsigned char z = (unsigned char)(x + y);
    KOAN_EQ_INT(/*__*/ 44, z);
}

/*
 * When signed and unsigned meet at the same rank, the signed operand is
 * converted to unsigned. This is why comparing a signed length against an
 * unsigned size can go so badly wrong.
 */
KOAN(mixing_signed_and_unsigned_converts_toward_unsigned)
{
    int      s = -1;
    unsigned u = 1;

    /* -1 becomes UINT_MAX, which is emphatically not less than 1. */
KOAN_DELIBERATE_BEGIN
    KOAN_FALSE(s < u);
KOAN_DELIBERATE_END

    /* Cast deliberately when you mean a signed comparison. */
    KOAN_TRUE(s < (int)u);
}

/*
 * Integer division truncates toward zero, and the remainder takes the sign of
 * the dividend. C99 fixed this; older C left it implementation-defined.
 */
KOAN(division_truncates_toward_zero)
{
    KOAN_EQ_INT(/*__*/ 3, 7 / 2);
    KOAN_EQ_INT(/*__*/ -3, -7 / 2);
    KOAN_EQ_INT(/*__*/ 1, 7 % 2);
    KOAN_EQ_INT(/*__*/ -1, -7 % 2);
}

/*
 * Conversions between types happen constantly, often silently. Narrowing a
 * float to an integer truncates the fractional part rather than rounding.
 */
KOAN(float_to_int_truncates)
{
    KOAN_EQ_INT(/*__*/ 3, (int)3.99);
    KOAN_EQ_INT(/*__*/ -3, (int)-3.99);
}

/*
 * Numbers with a fractional part have the type `double`. 1.5, 0.25 and -3.75
 * are all doubles.
 *
 * A double stores its value in binary, and not every decimal fraction has an
 * exact binary form — 0.1 does not, in the same way that 1/3 has no exact
 * decimal form. Two calculations that ought to agree can therefore differ in
 * the last few bits.
 *
 * So doubles are compared with a *tolerance*: how far apart two values may be
 * while still counting as equal. The blank for a double is `__DBL`, and the
 * third argument to KOAN_EQ_DBL is that tolerance — 1e-9 means one billionth.
 */
KOAN(doubles_hold_fractions_and_compare_with_a_tolerance)
{
    /* These two are exact in binary, so the tolerance is not even needed. */
    KOAN_EQ_DBL(/*__DBL*/ 0.75, 0.5 + 0.25, 1e-9);
    KOAN_EQ_DBL(/*__DBL*/ 2.5, 5.0 / 2.0, 1e-9);

    /* This one is not exact. The sum is a hair away from 0.3, which the
     * tolerance absorbs — and which exact comparison would not. */
    KOAN_EQ_DBL(/*__DBL*/ 0.3, 0.1 + 0.2, 1e-9);
    KOAN_FALSE(0.1 + 0.2 == 0.3);

    /* Dividing two ints throws the fraction away; dividing doubles keeps it. */
    KOAN_EQ_INT(/*__*/ 2, 5 / 2);
    KOAN_EQ_DBL(/*__DBL*/ 2.5, 5.0 / 2.0, 1e-9);
}

/*
 * A character constant like 'A' has type int in C — not char. This differs
 * from C++, and it is why sizeof('A') surprises people.
 */
KOAN(character_constants_are_ints)
{
    KOAN_TRUE(sizeof('A') == sizeof(int));
    KOAN_EQ_SZ(/*__SZ*/ 1, sizeof((char)'A'));
}

/*
 * Bringing it together: predict the type and the value.
 *
 * `sizeof` returns size_t, which is unsigned. Subtracting past zero wraps to
 * an enormous number, so this classic loop never terminates as intended.
 * Reason it through before you answer.
 */
KOAN(the_size_t_trap)
{
    size_t n = 0;
    size_t last_index = n - 1;

    KOAN_TRUE(last_index == (size_t)-1);
    KOAN_TRUE(last_index > /*__SZ*/ 1000000);

    /* Which is why you guard the loop, rather than trusting the subtraction. */
    int visited = 0;
    for (size_t i = 0; i + 1 <= n; i++) visited++;
    KOAN_EQ_INT(/*__*/ 0, visited);
}

KOAN_LESSON(lesson_about_types, "About Types",
    KOAN_CASE(char_is_the_unit_of_size),
    KOAN_CASE(a_byte_is_char_bit_bits),
    KOAN_CASE(integer_sizes_are_ordered_not_fixed),
    KOAN_CASE(exact_widths_have_names),
    KOAN_CASE(signed_ranges_are_asymmetric),
    KOAN_CASE(unsigned_arithmetic_wraps),
    KOAN_CASE(signed_overflow_must_be_prevented_not_detected),
    KOAN_CASE(small_types_promote_to_int),
    KOAN_CASE(mixing_signed_and_unsigned_converts_toward_unsigned),
    KOAN_CASE(division_truncates_toward_zero),
    KOAN_CASE(float_to_int_truncates),
    KOAN_CASE(doubles_hold_fractions_and_compare_with_a_tolerance),
    KOAN_CASE(character_constants_are_ints),
    KOAN_CASE(the_size_t_trap)
);
