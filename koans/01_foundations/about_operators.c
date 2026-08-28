/*
 * About Operators
 *
 * C's operators are famously terse and famously easy to misread. Precedence
 * is the usual culprit; evaluation order is the subtler one. The koans below
 * separate the two, because they are genuinely different questions:
 *
 *   precedence      — which operands bind to which operator
 *   evaluation order — when each subexpression is actually computed
 *
 * C fixes the first completely and the second barely at all.
 */
#include "koan.h"

#include <limits.h>
#include <stdint.h>

/*
 * Multiplicative binds tighter than additive; additive tighter than shift;
 * shift tighter than relational; relational tighter than bitwise-and.
 *
 * That last one is the trap. `a & b == c` parses as `a & (b == c)`.
 */
KOAN(precedence_is_not_left_to_right)
{
    KOAN_EQ_INT(__, 2 + 3 * 4);
    KOAN_EQ_INT(__, (2 + 3) * 4);

    /* Shift binds looser than addition: 1 << (2 + 3), not (1 << 2) + 3.
     * Equality binds tighter than bitwise AND. This is almost never what
     * anyone means, which is why compilers warn about it. */
    int flags = 0b0110;
KOAN_DELIBERATE_BEGIN
    KOAN_EQ_INT(__, flags & 0b0100 == 0b0100);
    KOAN_EQ_INT(__, 1 << 2 + 3);
KOAN_DELIBERATE_END
    KOAN_EQ_INT(__, (flags & 0b0100) == 0b0100 ? 4 : 0);
}

/*
 * The bitwise operators work on the value's representation. C23 made binary
 * literals standard, which makes these far easier to read than hex.
 */
KOAN(bitwise_operators_work_on_representation)
{
    unsigned a = 0b1100;
    unsigned b = 0b1010;

    KOAN_EQ_INT(__, a & b);
    KOAN_EQ_INT(__, a | b);
    KOAN_EQ_INT(__, a ^ b);

    /* Complement flips every bit, including the ones you forgot about. */
    unsigned char narrow = 0b00001111;
    KOAN_EQ_INT(__, (unsigned char)~narrow);
}

/*
 * Digit separators, also new in C23, are permitted inside any numeric literal
 * and are ignored entirely. They exist purely so you can read the number.
 */
KOAN(digit_separators_are_ignored)
{
    KOAN_EQ_INT(__, 1'000'000);
    KOAN_EQ_INT(__, 0b1111'1111);
}

/*
 * Shifting left by n multiplies by 2^n. Shifting a signed value left into or
 * past the sign bit is undefined behaviour; shift unsigned values when you
 * mean to manipulate bits.
 */
KOAN(shifts_scale_by_powers_of_two)
{
    KOAN_EQ_INT(__, 5u << 3);
    KOAN_EQ_INT(__, 40u >> 3);

    /* Right-shifting an unsigned value always brings in zeros. */
    unsigned u = 0b1000'0000;
    KOAN_EQ_INT(__, u >> 4);

    /* Shifting by >= the operand's width is undefined. Not "zero" — undefined.
     * The guard you want, expressed as a value: */
    int shift = 40;
    bool safe = (shift >= 0 && (size_t)shift < sizeof(unsigned) * CHAR_BIT);
    KOAN_EQ_INT(__, safe);
}

/*
 * && and || short-circuit: the right operand is not evaluated at all if the
 * left settles the answer. This is a guarantee, and it is how you write a
 * safe null check.
 */
KOAN(logical_operators_short_circuit)
{
    int  calls = 0;
    int  side_effect_count = 0;   /* incremented only if the right side runs */
    bool left_is_false = false;

    /* Because the left operand is false, the right is never evaluated. */
    if (left_is_false && (++side_effect_count, true)) calls++;
    KOAN_EQ_INT(__, side_effect_count);
    KOAN_EQ_INT(__, calls);

    /* Because the left operand is true, || stops immediately. */
    if (true || (++side_effect_count, true)) calls++;
    KOAN_EQ_INT(__, side_effect_count);
    KOAN_EQ_INT(__, calls);
}

/*
 * The guarded dereference this makes possible is the reason it matters.
 */
KOAN(short_circuit_guards_dereference)
{
    const char *maybe = nullptr;

    /* Safe: if maybe is null, *maybe is never evaluated. */
    bool starts_with_k = (maybe != nullptr && maybe[0] == 'k');
    KOAN_EQ_INT(__, starts_with_k);

    maybe = "koan";
    starts_with_k = (maybe != nullptr && maybe[0] == 'k');
    KOAN_EQ_INT(__, starts_with_k);
}

/*
 * The conditional operator yields a value, and its two arms are subject to
 * the usual conversions — which can change the type of the whole expression.
 */
KOAN(the_conditional_operator_yields_a_value)
{
    int n = 7;
    const char *parity = (n % 2 == 0) ? "even" : "odd";
    KOAN_EQ_STR(__STR, parity);

    /* Only one arm is evaluated, so this is also a guard. */
    int denom = 0;
    int safe_div = (denom != 0) ? 100 / denom : -1;
    KOAN_EQ_INT(__, safe_div);
}

/*
 * The comma operator evaluates its left operand, discards the result, then
 * yields the right. It is rare outside for-loop headers, and it is not the
 * same comma that separates function arguments.
 */
KOAN(the_comma_operator_discards_the_left)
{
KOAN_DELIBERATE_BEGIN
    int x = (1, 2, 3);
KOAN_DELIBERATE_END
    KOAN_EQ_INT(__, x);

    int i = 0, j = 10, steps = 0;
    for (; i < j; i++, j--) steps++;
    KOAN_EQ_INT(__, steps);
    KOAN_EQ_INT(__, i);
}

/*
 * Prefix and postfix increment differ in what they *yield*, not in what they
 * do to the variable. Both leave the variable incremented.
 */
KOAN(increment_position_changes_the_value_yielded)
{
    int n = 5;
    KOAN_EQ_INT(__, n++);
    KOAN_EQ_INT(__, n);

    int m = 5;
    KOAN_EQ_INT(__, ++m);
    KOAN_EQ_INT(__, m);
}

/*
 * Compound assignment evaluates its left operand exactly once. That is not a
 * stylistic detail; it is the reason `arr[f()] += 1` is safe where
 * `arr[f()] = arr[f()] + 1` is not.
 */
KOAN(compound_assignment_evaluates_the_target_once)
{
    int arr[3] = { 10, 20, 30 };
    int calls = 0;

    /* Simulating "an index expression with a side effect": */
    int idx = (calls++, 1);
    arr[idx] += 5;

    KOAN_EQ_INT(__, arr[1]);
    KOAN_EQ_INT(__, calls);
}

/*
 * Assignment is an expression and yields the value assigned, which is why
 * chaining works — and why `if (x = 0)` compiles when you meant `==`.
 */
KOAN(assignment_is_an_expression)
{
    int a, b, c;
    a = b = c = 4;
    KOAN_EQ_INT(__, a);

    int v;
    KOAN_EQ_INT(__, (v = 9));

    /* The classic typo, made harmless by writing the constant on the left.
     * Had this been `= 0`, it would not compile. */
    int status = 0;
    KOAN_TRUE(0 == status);
}

/*
 * sizeof is an operator, not a function, and its operand is not evaluated
 * (unless it is a variable-length array). The side effect below never happens.
 */
KOAN(sizeof_does_not_evaluate_its_operand)
{
    int n = 0;
KOAN_DELIBERATE_BEGIN
    size_t sz = sizeof(n++);
KOAN_DELIBERATE_END

    KOAN_EQ_SZ(__SZ, sz);
    KOAN_EQ_INT(__, n);
}

/*
 * Bringing it together.
 *
 * Extract a bitfield: take bits [hi:lo] inclusive out of a word and return
 * them right-aligned. This uses shifting, masking, complement and precedence
 * all at once, and it is a genuinely useful thing to know how to write.
 */
static unsigned extract_bits(unsigned word, unsigned hi, unsigned lo)
{
    unsigned width = hi - lo + 1u;
    /* Build a mask of `width` ones without shifting by the full word size. */
    unsigned mask = (width >= sizeof(unsigned) * CHAR_BIT)
                        ? ~0u
                        : (1u << width) - 1u;
    return (word >> lo) & mask;
}

KOAN(assembling_a_bitfield_extractor)
{
    /* 0xDEAD = 1101 1110 1010 1101 */
    unsigned word = 0xDEAD;

    KOAN_EQ_INT(__, extract_bits(word, 3, 0));
    KOAN_EQ_INT(__, extract_bits(word, 7, 4));
    KOAN_EQ_INT(__, extract_bits(word, 7, 0));
    KOAN_EQ_INT(__, extract_bits(word, 15, 8));

    /* One bit at a time: is bit 0 set? */
    KOAN_EQ_INT(__, extract_bits(word, 0, 0));
    KOAN_EQ_INT(__, extract_bits(word, 1, 1));
}

KOAN_LESSON(lesson_about_operators, "About Operators",
    KOAN_CASE(precedence_is_not_left_to_right),
    KOAN_CASE(bitwise_operators_work_on_representation),
    KOAN_CASE(digit_separators_are_ignored),
    KOAN_CASE(shifts_scale_by_powers_of_two),
    KOAN_CASE(logical_operators_short_circuit),
    KOAN_CASE(short_circuit_guards_dereference),
    KOAN_CASE(the_conditional_operator_yields_a_value),
    KOAN_CASE(the_comma_operator_discards_the_left),
    KOAN_CASE(increment_position_changes_the_value_yielded),
    KOAN_CASE(compound_assignment_evaluates_the_target_once),
    KOAN_CASE(assignment_is_an_expression),
    KOAN_CASE(sizeof_does_not_evaluate_its_operand),
    KOAN_CASE(assembling_a_bitfield_extractor)
);
