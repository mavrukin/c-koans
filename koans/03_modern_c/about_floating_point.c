/*
 * About Floating Point
 *
 * A double stores a sign, an exponent and 53 bits of significand. Anything
 * that cannot be written as a sum of powers of two within 53 bits is stored
 * approximately — including 0.1.
 *
 * None of this is a defect. It is a fixed-width representation of an infinite
 * set, and the koans below are about knowing exactly where the edges are.
 */
#include "koan.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Values that are sums of powers of two are exact. Everything else is not.
 */
KOAN(some_values_are_exact_and_some_are_not)
{
    /* 0.5, 0.25, 0.75 are exactly representable. */
    KOAN_TRUE(0.5 + 0.25 == 0.75);

    /* 0.1 and 0.2 are not, and their sum misses 0.3. */
    KOAN_FALSE(0.1 + 0.2 == 0.3);

    /* By a tiny amount, but not by zero. */
    double error = fabs((0.1 + 0.2) - 0.3);
    KOAN_TRUE(error > 0.0);
    KOAN_TRUE(error < 1e-15);
}

/*
 * So compare with a tolerance. An absolute epsilon works near zero; for large
 * values you want a relative one, because the gap between representable
 * doubles grows with magnitude.
 */
static bool close_enough(double a, double b, double rel)
{
    double diff = fabs(a - b);
    double scale = fmax(fabs(a), fabs(b));
    return diff <= rel * fmax(scale, 1.0);
}

KOAN(compare_with_a_tolerance)
{
    KOAN_EQ_INT(__, close_enough(0.1 + 0.2, 0.3, 1e-12));
    KOAN_EQ_INT(__, close_enough(1.0, 1.5, 1e-12));

    /* A relative test still works where an absolute one would fail: these two
     * differ by 0.125, which is enormous in absolute terms and negligible
     * beside 1e15. */
    KOAN_EQ_INT(__, close_enough(1e15, 1e15 + 0.125, 1e-12));
}

/*
 * DBL_EPSILON is the gap between 1.0 and the next representable double. It is
 * NOT a universal tolerance: the gap at 1e15 is far larger.
 */
KOAN(epsilon_is_the_gap_at_one)
{
    KOAN_TRUE(1.0 + DBL_EPSILON != 1.0);
    KOAN_TRUE(1.0 + DBL_EPSILON / 2.0 == 1.0);      /* rounds back down */

    KOAN_EQ_INT(__, DBL_MANT_DIG);           /* bits of significand */
    KOAN_EQ_INT(__, FLT_MANT_DIG);

    /* Beyond 2^53 the gap exceeds 1, so consecutive integers stop existing. */
    double big = 9007199254740992.0;                 /* 2^53 */
    KOAN_TRUE(big + 1.0 == big);
    KOAN_TRUE(big - 1.0 != big);
}

/*
 * Integers are exact in a double up to 2^53. Past that, an int64_t and a
 * double disagree — which is why money and ids are never stored as doubles.
 */
KOAN(integers_are_exact_only_up_to_two_to_the_53)
{
    KOAN_EQ_INT(__, (double)9007199254740992LL == 9007199254740992.0);

    /* One past it collides with its neighbour. */
    int64_t n = 9007199254740993LL;                  /* 2^53 + 1 */
    KOAN_TRUE((int64_t)(double)n != n);
    KOAN_EQ_INT(__, (double)n == 9007199254740992.0);
}

/*
 * Special values. NaN is not equal to anything, including itself — which is
 * the only portable way to detect it without isnan.
 */
KOAN(nan_and_infinity_have_rules)
{
    double nan_value = NAN;
    double inf       = INFINITY;

    KOAN_FALSE(nan_value == nan_value);
    KOAN_EQ_INT(__, isnan(nan_value));
    KOAN_EQ_INT(__, isnan(1.0));

    /* Every comparison with NaN is false, including >=. */
    KOAN_FALSE(nan_value < 1.0);
    KOAN_FALSE(nan_value >= 1.0);

    KOAN_EQ_INT(__, isinf(inf));
    KOAN_TRUE(inf > DBL_MAX);
    KOAN_TRUE(1.0 / inf == 0.0);

    /* Division by zero yields infinity, not a trap — and 0.0/0.0 yields NaN. */
    double zero = 0.0;
    KOAN_EQ_INT(__, isinf(1.0 / zero));
    KOAN_EQ_INT(__, isnan(zero / zero));
}

/*
 * There are two zeros, and they compare equal while behaving differently.
 * signbit is the way to tell them apart.
 */
KOAN(there_are_two_zeros)
{
    double pos = 0.0;
    double neg = -0.0;

    KOAN_TRUE(pos == neg);
    KOAN_EQ_INT(__, signbit(pos));
    KOAN_EQ_INT(__, signbit(neg) != 0);

    /* They differ where the sign survives the operation. */
    KOAN_TRUE(1.0 / pos > 0);
    KOAN_TRUE(1.0 / neg < 0);
}

/*
 * Rounding: the default mode is round-to-nearest, ties-to-even, which is why
 * 0.5 and 1.5 round to the same value under rint but not under round.
 */
KOAN(rounding_has_several_meanings)
{
    KOAN_EQ_DBL(__DBL, ceil(1.1), 1e-12);
    KOAN_EQ_DBL(__DBL, floor(1.9), 1e-12);
    KOAN_EQ_DBL(__DBL, trunc(1.9), 1e-12);
    KOAN_EQ_DBL(__DBL, trunc(-1.9), 1e-12);
    KOAN_EQ_DBL(__DBL, floor(-1.9), 1e-12);

    /* round() goes away from zero on a tie. */
    KOAN_EQ_DBL(__DBL, round(0.5), 1e-12);
    KOAN_EQ_DBL(__DBL, round(1.5), 1e-12);
    KOAN_EQ_DBL(__DBL, round(-0.5), 1e-12);

    /* rint() honours the current mode, which by default is ties-to-even. */
    KOAN_EQ_DBL(__DBL, rint(0.5), 1e-12);
    KOAN_EQ_DBL(__DBL, rint(1.5), 1e-12);
    KOAN_EQ_DBL(__DBL, rint(2.5), 1e-12);
}

/*
 * Addition is not associative, because each step rounds. Summing in a
 * different order gives a different answer — the reason numerical code cares
 * about ordering.
 */
KOAN(addition_is_not_associative)
{
    double big = 1e16, one = 1.0;

    /* Adding 1 to 1e16 is lost; adding the two 1s first is not. */
    KOAN_TRUE((big + one) - big == 0.0);
    KOAN_TRUE((one + one) + big - big == 2.0);

    /* Which means these two sums of the same three numbers differ. */
    double left  = (big + one) + one;
    double right = big + (one + one);
    KOAN_TRUE(left != right);
}

/*
 * Bringing it together.
 *
 * Kahan summation: track the rounding error in a second variable and feed it
 * back. It recovers most of the precision a naive sum loses, and it is a
 * genuinely useful thing to carry with you.
 *
 * Summing 0.1 ten thousand times should give 1000.0.
 */
static double naive_sum(const double *v, size_t n)
{
    double total = 0.0;
    for (size_t i = 0; i < n; i++) total += v[i];
    return total;
}

static double kahan_sum(const double *v, size_t n)
{
    double total = 0.0;
    double lost  = 0.0;               /* the error carried forward */

    for (size_t i = 0; i < n; i++) {
        double adjusted = v[i] - lost;
        double next     = total + adjusted;
        lost  = (next - total) - adjusted;   /* what the addition dropped */
        total = next;
    }
    return total;
}

KOAN(assembling_kahan_summation)
{
    constexpr size_t N = 10000;
    static double    values[N];
    for (size_t i = 0; i < N; i++) values[i] = 0.1;

    double naive = naive_sum(values, N);
    double kahan = kahan_sum(values, N);

    /* Both are close, but only one is exact. */
    KOAN_EQ_DBL(__DBL, kahan, 1e-12);
    KOAN_TRUE(kahan == 1000.0);
    KOAN_FALSE(naive == 1000.0);

    /* The naive sum has drifted by a small but real amount. */
    double drift = fabs(naive - 1000.0);
    KOAN_TRUE(drift > 0.0);
    KOAN_TRUE(drift < 1e-6);

    /* Kahan is strictly better here, which is the whole point. */
    KOAN_TRUE(fabs(kahan - 1000.0) < drift);
}

KOAN_LESSON(lesson_about_floating_point, "About Floating Point",
    KOAN_CASE(some_values_are_exact_and_some_are_not),
    KOAN_CASE(compare_with_a_tolerance),
    KOAN_CASE(epsilon_is_the_gap_at_one),
    KOAN_CASE(integers_are_exact_only_up_to_two_to_the_53),
    KOAN_CASE(nan_and_infinity_have_rules),
    KOAN_CASE(there_are_two_zeros),
    KOAN_CASE(rounding_has_several_meanings),
    KOAN_CASE(addition_is_not_associative),
    KOAN_CASE(assembling_kahan_summation)
);
