/*
 * About Bit Manipulation
 *
 * C23 finally standardised the bit operations every program reinvents, in
 * <stdbit.h>. Before it, `popcount` and `bit_width` meant a compiler builtin
 * or a hand-rolled loop.
 *
 * Not every libc ships <stdbit.h> yet. include/compat.h detects that and
 * supplies conforming versions, so these koans run either way — read the shim
 * to see how each is computed.
 */
#include "koan.h"
#include "compat.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

/*
 * A mask selects bits. `1u << n` is the nth bit; subtracting one gives all
 * bits below it.
 */
KOAN(masks_select_bits)
{
    unsigned flags = 0b1011'0110;

    KOAN_EQ_INT(/*__*/ 0b0000'0100, flags & (1u << 2));   /* isolate bit 2 */
    KOAN_EQ_INT(/*__*/ 1, (flags >> 2) & 1u);             /* test bit 2 */
    KOAN_EQ_INT(/*__*/ 0, (flags >> 3) & 1u);             /* bit 3 is clear */

    /* n low bits set. Note the guard: shifting by the full width is UB. */
    KOAN_EQ_INT(/*__*/ 0b1111, (1u << 4) - 1u);
    KOAN_EQ_INT(/*__*/ 0b0110, flags & ((1u << 4) - 1u) & 0b1111);
}

/*
 * The four operations on a single bit. Learn them as a set; the XOR toggle is
 * the one people forget exists.
 */
KOAN(set_clear_toggle_test)
{
    unsigned v = 0b0000;

    v |=  (1u << 1);                        /* set   */
    KOAN_EQ_INT(/*__*/ 0b0010, v);

    v |=  (1u << 3);
    KOAN_EQ_INT(/*__*/ 0b1010, v);

    v &= ~(1u << 1);                        /* clear */
    KOAN_EQ_INT(/*__*/ 0b1000, v);

    v ^=  (1u << 3);                        /* toggle off */
    KOAN_EQ_INT(/*__*/ 0b0000, v);

    v ^=  (1u << 2);                        /* toggle on */
    KOAN_EQ_INT(/*__*/ 0b0100, v);
}

/*
 * stdc_count_ones is popcount. stdc_bit_width is the position of the highest
 * set bit plus one — that is, how many bits the value needs.
 */
KOAN(counting_and_measuring_bits)
{
    KOAN_EQ_INT(/*__*/ 4, (int)stdc_count_ones(0b1111u));
    KOAN_EQ_INT(/*__*/ 3, (int)stdc_count_ones(0b1011'0000u));
    KOAN_EQ_INT(/*__*/ 0, (int)stdc_count_ones(0u));

    KOAN_EQ_INT(/*__*/ 4, (int)stdc_bit_width(0b1111u));
    KOAN_EQ_INT(/*__*/ 1, (int)stdc_bit_width(1u));
    KOAN_EQ_INT(/*__*/ 0, (int)stdc_bit_width(0u));
    KOAN_EQ_INT(/*__*/ 8, (int)stdc_bit_width(255u));
    KOAN_EQ_INT(/*__*/ 9, (int)stdc_bit_width(256u));
}

/*
 * Leading and trailing zeros. Trailing zeros of a power of two is its
 * exponent, which is how you turn an alignment into a shift.
 */
KOAN(leading_and_trailing_zeros)
{
    KOAN_EQ_INT(/*__*/ 0, (int)stdc_trailing_zeros(0b0001u));
    KOAN_EQ_INT(/*__*/ 3, (int)stdc_trailing_zeros(0b1000u));
    KOAN_EQ_INT(/*__*/ 1, (int)stdc_trailing_zeros(0b0110u));

    /* log2 of a power of two, exactly and without floating point. */
    KOAN_EQ_INT(/*__*/ 10, (int)stdc_trailing_zeros(1024u));

    /* Leading zeros depend on the type's width. */
    KOAN_EQ_INT(/*__*/ (int)(sizeof(unsigned) * CHAR_BIT - 1),
                (int)stdc_leading_zeros(1u));
}

/*
 * Powers of two have exactly one bit set, which makes `x & (x - 1)` zero.
 * bit_floor rounds down to a power of two; bit_ceil rounds up.
 */
KOAN(powers_of_two_have_one_bit)
{
    KOAN_EQ_INT(/*__*/ 1, stdc_has_single_bit(16u));
    KOAN_EQ_INT(/*__*/ 0, stdc_has_single_bit(17u));
    KOAN_EQ_INT(/*__*/ 0, stdc_has_single_bit(0u));

    /* The identity behind it: clearing the lowest set bit leaves zero. */
    unsigned p = 16;
    KOAN_EQ_INT(/*__*/ 0, p & (p - 1));

    KOAN_EQ_INT(/*__*/ 16, (int)stdc_bit_floor(31u));
    KOAN_EQ_INT(/*__*/ 32, (int)stdc_bit_ceil(31u));
    KOAN_EQ_INT(/*__*/ 32, (int)stdc_bit_floor(32u));
    KOAN_EQ_INT(/*__*/ 32, (int)stdc_bit_ceil(32u));
}

/*
 * Shifting is only defined for a count below the operand's width, and only
 * for non-negative signed values. Manipulate bits on unsigned types.
 */
KOAN(shift_safely_on_unsigned_types)
{
    constexpr int WIDTH = (int)(sizeof(unsigned) * CHAR_BIT);
    KOAN_EQ_INT(/*__*/ 32, WIDTH);

    /* The rotate people write wrongly: shifting by 0 makes `x >> (32 - 0)`
     * a shift by the full width, which is undefined. The mask fixes it. */
    unsigned x = 0b1001u;
    unsigned n = 0;
    unsigned rotated = (x << n) | (x >> ((WIDTH - n) & (WIDTH - 1)));
    KOAN_EQ_INT(/*__*/ 0b1001, rotated);

    n = 1;
    rotated = (x << n) | (x >> ((WIDTH - n) & (WIDTH - 1)));
    KOAN_EQ_INT(/*__*/ 0b10010, rotated);
}

/*
 * Bringing it together.
 *
 * A fixed-capacity bitset: one bit per element, packed into words. This is how
 * every allocator's free list, every "seen" set and every Bloom filter is
 * built, and it needs division and modulo by the word width — done with
 * shifts and masks, because the width is a power of two.
 */
constexpr size_t BITSET_BITS  = 256;
constexpr size_t WORD_BITS    = sizeof(uint64_t) * CHAR_BIT;
constexpr size_t BITSET_WORDS = BITSET_BITS / WORD_BITS;

typedef struct { uint64_t word[BITSET_WORDS]; } Bitset;

static void bs_set(Bitset *b, size_t i)   { b->word[i / WORD_BITS] |=  (uint64_t)1 << (i % WORD_BITS); }
static void bs_clear(Bitset *b, size_t i) { b->word[i / WORD_BITS] &= ~((uint64_t)1 << (i % WORD_BITS)); }
static bool bs_test(const Bitset *b, size_t i) { return (b->word[i / WORD_BITS] >> (i % WORD_BITS)) & 1u; }

static size_t bs_count(const Bitset *b)
{
    size_t n = 0;
    for (size_t w = 0; w < BITSET_WORDS; w++) {
        uint64_t v = b->word[w];
        while (v) { v &= v - 1; n++; }      /* clear lowest set bit each pass */
    }
    return n;
}

/* Index of the lowest set bit, or BITSET_BITS if empty. */
static size_t bs_first(const Bitset *b)
{
    for (size_t w = 0; w < BITSET_WORDS; w++) {
        if (b->word[w] == 0) continue;
        uint64_t v = b->word[w];
        size_t   bit = 0;
        while (!(v & 1)) { v >>= 1; bit++; }
        return w * WORD_BITS + bit;
    }
    return BITSET_BITS;
}

KOAN(assembling_a_bitset)
{
    Bitset b = { 0 };

    KOAN_EQ_SZ(/*__SZ*/ 4, BITSET_WORDS);
    KOAN_EQ_SZ(/*__SZ*/ 32, sizeof b);        /* 256 bits in 32 bytes */
    KOAN_EQ_SZ(/*__SZ*/ 0, bs_count(&b));
    KOAN_EQ_SZ(/*__SZ*/ 256, bs_first(&b));   /* empty */

    bs_set(&b, 0);
    bs_set(&b, 63);
    bs_set(&b, 64);                           /* the first bit of word 1 */
    bs_set(&b, 255);

    KOAN_EQ_SZ(/*__SZ*/ 4, bs_count(&b));
    KOAN_EQ_INT(/*__*/ 1, bs_test(&b, 64));
    KOAN_EQ_INT(/*__*/ 0, bs_test(&b, 65));
    KOAN_EQ_SZ(/*__SZ*/ 0, bs_first(&b));

    /* Bit 63 and bit 64 live in different words — the boundary that index
     * arithmetic must get right. */
    KOAN_EQ_INT(/*__*/ 1, b.word[0] != 0);
    KOAN_EQ_INT(/*__*/ 1, b.word[1] != 0);
    KOAN_EQ_INT(/*__*/ 1, b.word[2] == 0);

    bs_clear(&b, 0);
    KOAN_EQ_SZ(/*__SZ*/ 3, bs_count(&b));
    KOAN_EQ_SZ(/*__SZ*/ 63, bs_first(&b));

    /* Setting a bit twice is idempotent; clearing a clear bit is harmless. */
    bs_set(&b, 63);
    bs_clear(&b, 100);
    KOAN_EQ_SZ(/*__SZ*/ 3, bs_count(&b));
}

KOAN_LESSON(lesson_about_bit_manipulation, "About Bit Manipulation",
    KOAN_CASE(masks_select_bits),
    KOAN_CASE(set_clear_toggle_test),
    KOAN_CASE(counting_and_measuring_bits),
    KOAN_CASE(leading_and_trailing_zeros),
    KOAN_CASE(powers_of_two_have_one_bit),
    KOAN_CASE(shift_safely_on_unsigned_types),
    KOAN_CASE(assembling_a_bitset)
);
