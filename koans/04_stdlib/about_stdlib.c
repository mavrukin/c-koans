/*
 * About <stdlib.h>
 *
 * The general-purpose header: conversions, searching and sorting, process
 * control, and random numbers. Most of it is well-behaved; the parts that are
 * not (atoi, rand's range, qsort's comparator contract) are exactly the parts
 * these koans dwell on.
 */
#include "koan.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The strto* family converts and reports. The base argument accepts 0 to mean
 * "infer from the prefix", which handles 0x and 0 octal for free.
 */
KOAN(strtol_infers_the_base_when_asked)
{
    char *end = nullptr;

    KOAN_EQ_INT(__, (int)strtol("ff", &end, 16));
    KOAN_EQ_INT(__, (int)strtol("0xff", &end, 16));

    /* Base 0 infers: 0x is hex, a leading 0 is octal, otherwise decimal. */
    KOAN_EQ_INT(__, (int)strtol("0xff", &end, 0));
    KOAN_EQ_INT(__, (int)strtol("010", &end, 0));
    KOAN_EQ_INT(__, (int)strtol("10", &end, 0));

    /* Base 2 reads binary, which is how you parse "1011" without a loop. */
    KOAN_EQ_INT(__, (int)strtol("1011", &end, 2));

    /* `end` points at the first unconsumed character. */
    long v = strtol("42rest", &end, 10);
    KOAN_EQ_INT(__, (int)v);
    KOAN_EQ_STR(__STR, end);
}

/*
 * strtod parses floating point, and accepts more than you expect: infinity,
 * nan, and hexadecimal floats are all valid input.
 */
KOAN(strtod_accepts_more_than_digits)
{
    char *end = nullptr;

    KOAN_EQ_DBL(__DBL, strtod("1.5", &end), 1e-12);
    KOAN_EQ_DBL(__DBL, strtod("1.5e3", &end), 1e-12);
    KOAN_EQ_DBL(__DBL, strtod("-0.25", &end), 1e-12);

    /* Which is a reason to validate input rather than trust the parse. */
    KOAN_TRUE(strtod("inf", &end) > 1e308);
    KOAN_TRUE(strtod("nan", &end) != strtod("nan", &end));
}

/*
 * qsort's comparator must be a *strict weak ordering*: consistent,
 * antisymmetric and transitive. Returning a subtraction is the classic bug,
 * because it overflows for values far apart.
 */
static int cmp_by_subtraction(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;      /* WRONG for large values */
}

static int cmp_correctly(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

KOAN(comparators_must_not_subtract)
{
    /* For small values the buggy version happens to work. */
    int small[] = { 3, 1, 2 };
    qsort(small, 3, sizeof *small, cmp_by_subtraction);
    KOAN_EQ_INT(__, small[0]);

    /* The subtraction INT_MIN - INT_MAX overflows, so the comparator claims
     * the wrong order. The correct form never subtracts. */
    KOAN_TRUE(cmp_correctly(&(int){ INT_MIN }, &(int){ INT_MAX }) < 0);

    int wide[] = { INT_MAX, INT_MIN, 0 };
    qsort(wide, 3, sizeof *wide, cmp_correctly);
    KOAN_EQ_INT(__, wide[0] == INT_MIN);
    KOAN_EQ_INT(__, wide[2] == INT_MAX);
}

/*
 * qsort is not stable: equal elements may be reordered. When order among
 * equals matters, include a tiebreaker in the comparator.
 */
typedef struct { const char *name; int score; int seq; } Player;

static int by_score_then_seq(const void *a, const void *b)
{
    const Player *x = a, *y = b;
    if (x->score != y->score) return (y->score > x->score) - (y->score < x->score);
    return (x->seq > y->seq) - (x->seq < y->seq);   /* the tiebreaker */
}

KOAN(qsort_is_not_stable_so_add_a_tiebreaker)
{
    Player players[] = {
        { "ada",  90, 0 },
        { "alan", 95, 1 },
        { "grace",90, 2 },
        { "edsger",95, 3 },
    };

    qsort(players, 4, sizeof *players, by_score_then_seq);

    /* Descending by score, and within a score by original position. */
    KOAN_EQ_STR(__STR, players[0].name);
    KOAN_EQ_STR(__STR, players[1].name);
    KOAN_EQ_STR(__STR, players[2].name);
    KOAN_EQ_STR(__STR, players[3].name);
}

/*
 * bsearch needs the array sorted by the same comparator, and returns a
 * pointer to *some* matching element — not necessarily the first.
 */
KOAN(bsearch_needs_the_same_ordering)
{
    int sorted[] = { 1, 3, 5, 7, 9, 11 };

    int  key = 7;
    int *hit = bsearch(&key, sorted, 6, sizeof *sorted, cmp_correctly);
    KOAN_TRUE(hit != nullptr);
    KOAN_EQ_SZ(__SZ, (size_t)(hit - sorted));

    key = 8;
    KOAN_TRUE(bsearch(&key, sorted, 6, sizeof *sorted, cmp_correctly) == nullptr);

    /* Searching an unsorted array is undefined, not merely unsuccessful. */
    KOAN_TRUE(sorted[0] < sorted[5]);
}

/*
 * rand() returns 0..RAND_MAX. Taking a modulus biases the result unless the
 * range divides evenly — the low values become slightly more likely. Reject
 * the unfair tail instead.
 */
static int bounded_random(int limit)
{
    int reject_above = RAND_MAX - (RAND_MAX % limit);
    int r;
    do { r = rand(); } while (r >= reject_above);
    return r % limit;
}

KOAN(rand_modulo_is_biased)
{
    srand(1);                       /* a fixed seed makes this deterministic */

    KOAN_TRUE(RAND_MAX >= 32767);

    /* Every draw is in range. */
    for (int i = 0; i < 1000; i++) {
        int r = bounded_random(6);
        KOAN_TRUE(r >= 0 && r < 6);
    }

    /* The same seed gives the same sequence — which is why rand() is for
     * simulations, never for anything security-related. */
    srand(42);
    int first = rand();
    srand(42);
    KOAN_EQ_INT(__, rand() == first);
}

/*
 * div returns quotient and remainder together, with a guaranteed
 * truncate-toward-zero rounding. abs and labs are the integer absolutes —
 * and abs(INT_MIN) is undefined, because its negation is not representable.
 */
KOAN(div_returns_both_halves)
{
    div_t d = div(17, 5);
    KOAN_EQ_INT(__, d.quot);
    KOAN_EQ_INT(__, d.rem);

    div_t negative = div(-17, 5);
    KOAN_EQ_INT(__, negative.quot);
    KOAN_EQ_INT(__, negative.rem);

    KOAN_EQ_INT(__, abs(-7));
    KOAN_EQ_INT(__, abs(7));

    /* The edge case: |INT_MIN| does not fit in an int. Guard before calling. */
    long safe = labs((long)INT_MIN);
    KOAN_TRUE(safe > 0);
}

/*
 * getenv reads the environment and returns null when unset. The returned
 * pointer belongs to the environment — copy it before you keep it.
 */
KOAN(getenv_returns_null_when_unset)
{
    KOAN_TRUE(getenv("KOAN_DEFINITELY_NOT_SET_12345") == nullptr);

    /* PATH is present on every system these koans run on. */
    const char *path = getenv("PATH");
    KOAN_TRUE(path != nullptr);
    KOAN_TRUE(strlen(path) > 0);
}

/*
 * Bringing it together.
 *
 * A frequency table: count occurrences, then rank them. This needs a
 * comparator with a tiebreaker for determinism, bsearch for lookup, and
 * careful handling of the fact that qsort moves your elements — so an index
 * captured before the sort is meaningless after it.
 */
typedef struct { char letter; int count; } Tally;

static int by_count_desc_then_letter(const void *a, const void *b)
{
    const Tally *x = a, *y = b;
    if (x->count != y->count)
        return (y->count > x->count) - (y->count < x->count);
    return (x->letter > y->letter) - (x->letter < y->letter);
}

static size_t tally_letters(const char *text, Tally *out, size_t cap)
{
    int counts[26] = { 0 };
    for (const char *p = text; *p; p++)
        if (*p >= 'a' && *p <= 'z') counts[*p - 'a']++;

    size_t n = 0;
    for (int i = 0; i < 26 && n < cap; i++)
        if (counts[i] > 0)
            out[n++] = (Tally){ .letter = (char)('a' + i), .count = counts[i] };

    qsort(out, n, sizeof *out, by_count_desc_then_letter);
    return n;
}

KOAN(assembling_a_frequency_table)
{
    Tally  table[26];
    size_t n = tally_letters("mississippi", table, 26);

    /* m=1, i=4, s=4, p=2 -> four distinct letters. */
    KOAN_EQ_SZ(__SZ, n);

    /* Ties are broken alphabetically, so i comes before s at count 4. */
    KOAN_EQ_CHR(__CHR, table[0].letter);
    KOAN_EQ_INT(__, table[0].count);
    KOAN_EQ_CHR(__CHR, table[1].letter);
    KOAN_EQ_CHR(__CHR, table[2].letter);
    KOAN_EQ_INT(__, table[2].count);
    KOAN_EQ_CHR(__CHR, table[3].letter);

    /* Non-letters are ignored entirely. */
    n = tally_letters("a-b-a!", table, 26);
    KOAN_EQ_SZ(__SZ, n);
    KOAN_EQ_CHR(__CHR, table[0].letter);
    KOAN_EQ_INT(__, table[0].count);

    /* Empty input yields an empty table, not a crash. */
    KOAN_EQ_SZ(__SZ, tally_letters("", table, 26));
}

KOAN_LESSON(lesson_about_stdlib, "About <stdlib.h>",
    KOAN_CASE(strtol_infers_the_base_when_asked),
    KOAN_CASE(strtod_accepts_more_than_digits),
    KOAN_CASE(comparators_must_not_subtract),
    KOAN_CASE(qsort_is_not_stable_so_add_a_tiebreaker),
    KOAN_CASE(bsearch_needs_the_same_ordering),
    KOAN_CASE(rand_modulo_is_biased),
    KOAN_CASE(div_returns_both_halves),
    KOAN_CASE(getenv_returns_null_when_unset),
    KOAN_CASE(assembling_a_frequency_table)
);
