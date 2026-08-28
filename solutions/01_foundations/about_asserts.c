/*
 * About Asserts
 *
 * Every koan in this repository is a small claim about C that is currently
 * false. Your work is to make it true, one koan at a time.
 *
 * The runner stops at the first koan that is not yet true. Fix it, run
 * `make` again, and it will carry you to the next one.
 *
 * Start here. Read the koan, replace the blank, run `make`.
 */
#include "koan.h"

/*
 * KOAN_TRUE asserts that an expression is true.
 *
 * Replace __BOOL below with `true`.
 */
KOAN(asserting_truth)
{
    KOAN_TRUE(/*__BOOL*/ true);
}

/*
 * The value you supply is ordinary C. Any expression that evaluates to true
 * will satisfy the koan — but prefer the one that says what you mean.
 */
KOAN(asserting_a_claim)
{
    KOAN_TRUE(1 + 1 == /*__*/ 2);
}

/*
 * KOAN_FALSE is the mirror of KOAN_TRUE.
 */
KOAN(asserting_falsehood)
{
    KOAN_FALSE(/*__BOOL*/ false);
}

/*
 * Most koans compare a value you provide against a value C produces.
 * The blank always comes first: it is what *you* claim the answer is.
 */
KOAN(filling_in_values)
{
    KOAN_EQ_INT(/*__*/ 21, 3 * 7);
}

/*
 * Blanks are typed, because C is typed. A size_t blank is written __SZ.
 * sizeof yields a size_t, so this koan needs one.
 *
 * (If you are unsure what sizeof(char) is, the standard settles it: 1, always.)
 */
KOAN(blanks_have_types)
{
    KOAN_EQ_SZ(/*__SZ*/ 1, sizeof(char));
}

/*
 * Strings compare by content, not by address.
 */
KOAN(comparing_strings)
{
    const char *greeting = "hello";
    KOAN_EQ_STR(/*__STR*/ "hello", greeting);
}

/*
 * Characters are small integers. Both of these assertions describe the same
 * byte, and both must pass.
 */
KOAN(characters_are_integers)
{
    KOAN_EQ_CHR(/*__CHR*/ 'k', "koan"[0]);
    KOAN_EQ_INT(/*__*/ 107, 'k');
}

/*
 * Floating point rarely compares exactly, so KOAN_EQ_DBL takes a tolerance.
 * You will meet the reason for that tolerance in about_floating_point.
 */
KOAN(doubles_need_tolerance)
{
    KOAN_EQ_DBL(/*__DBL*/ 0.75, 0.5 + 0.25, 1e-9);
}

/*
 * When you have understood a koan, the assertion reads as a plain statement
 * of fact. That is the whole trick: the koans are documentation that fails
 * when you are wrong.
 */
KOAN(the_shape_of_the_path)
{
    const char *what_you_are_editing = "a koan";
    KOAN_EQ_STR(/*__STR*/ "a koan", what_you_are_editing);
    KOAN_TRUE(/*__BOOL*/ true);
}

KOAN_LESSON(lesson_about_asserts, "About Asserts",
    KOAN_CASE(asserting_truth),
    KOAN_CASE(asserting_a_claim),
    KOAN_CASE(asserting_falsehood),
    KOAN_CASE(filling_in_values),
    KOAN_CASE(blanks_have_types),
    KOAN_CASE(comparing_strings),
    KOAN_CASE(characters_are_integers),
    KOAN_CASE(doubles_need_tolerance),
    KOAN_CASE(the_shape_of_the_path)
);
