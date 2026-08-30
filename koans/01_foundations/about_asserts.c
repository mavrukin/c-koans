/*
 * About Asserts
 *
 * This is the first lesson. It assumes you have never written a line of C, and
 * it introduces nothing that it does not explain.
 *
 * WHAT YOU ARE LOOKING AT
 * -----------------------
 * A C program is text. The compiler reads it, turns it into a program, and the
 * program runs. This file is part of one.
 *
 * Text between slash-star and star-slash is a *comment*. The compiler ignores
 * it completely; it is there for you. Everything you are reading now is a
 * comment.
 *
 * Below, you will see blocks that look like this:
 *
 *     KOAN(some_name)
 *     {
 *         ...
 *     }
 *
 * Each one is a *koan*: a named piece of code the runner executes. The braces
 * { and } mark where it begins and ends. Inside are *statements*, and every
 * statement ends with a semicolon — that is how C knows one has finished.
 *
 * WHAT AN ASSERTION IS
 * --------------------
 * An assertion is a claim that something is true. If it is true, nothing
 * happens and the program carries on. If it is false, the program stops and
 * says so.
 *
 * That is the whole mechanism of this course. Each koan makes a claim with a
 * hole in it. You fill the hole with what you believe makes the claim true.
 *
 * THE BLANK
 * ---------
 * A hole is written `__` (two underscores), sometimes with letters after it.
 * Replace the blank — delete it, type your answer in its place — then save the
 * file and run `make` again.
 *
 * If you are wrong, the runner tells you what it expected and what it got. If
 * you leave a blank untouched, it tells you that instead. Neither is a
 * setback; being told exactly what is wrong is the fastest way to learn.
 *
 * Start with the first koan below.
 */
#include "koan.h"

/*
 * C has a type for things that are either true or false. It is called `bool`,
 * and it has exactly two values, written `true` and `false`. There is nothing
 * else a bool can be.
 *
 * KOAN_TRUE claims that what you give it is true.
 *
 * The blank `__BOOL` marks a hole that wants a bool. Replace it with `true`.
 */
KOAN(a_claim_that_is_true)
{
    KOAN_TRUE(__BOOL);
}

/*
 * KOAN_FALSE is its mirror: it claims that what you give it is false.
 *
 * There is only one other bool value, so there is only one answer.
 */
KOAN(a_claim_that_is_false)
{
    KOAN_FALSE(__BOOL);
}

/*
 * Whole numbers in C have the type `int` — short for integer. 0, 7 and -12 are
 * all ints.
 *
 * KOAN_EQ_INT takes two ints and claims they are equal. The blank always comes
 * first: it is what *you* say the answer is. The second value is what C
 * actually produces.
 *
 * Here C produces 7, so 7 is what you must claim.
 */
KOAN(two_numbers_that_are_equal)
{
    KOAN_EQ_INT(__, 7);
}

/*
 * C can calculate. `+` adds, `-` subtracts, `*` multiplies and `/` divides.
 *
 * The koan below asks you to do the multiplication yourself and claim the
 * result. You are predicting what the program will compute — which is the
 * skill this whole course is built on.
 */
KOAN(c_can_calculate)
{
    KOAN_EQ_INT(__, 3 * 7);
    KOAN_EQ_INT(__, 4 + 6);
    KOAN_EQ_INT(__, 8 - 6);
}

/*
 * `==` asks a question: are these two things equal? It produces a bool — true
 * or false — so it fits anywhere a bool fits, including inside KOAN_TRUE.
 *
 * Note that `==` (two equals signs) asks a question. A single `=` means
 * something else entirely, which you will see when variables appear. Keep them
 * apart from the start: `==` compares.
 */
KOAN(asking_whether_things_are_equal)
{
    KOAN_TRUE(1 + 1 == __);
    KOAN_FALSE(2 + 2 == __);
}

/*
 * A single character is written between single quotes: 'a', 'Z', '?'.
 *
 * The blank for a character is `__CHR`. Give it the character that makes the
 * claim true — including the single quotes.
 */
KOAN(a_single_character)
{
    KOAN_EQ_CHR(__CHR, 'k');
}

/*
 * A piece of text is written between double quotes: "hello". C calls this a
 * *string*. Note the difference from the koan above: single quotes hold one
 * character, double quotes hold text of any length.
 *
 * The blank for text is `__STR`. KOAN_EQ_STR compares two pieces of text
 * character by character, so the answer must match exactly — including
 * capitals and spaces.
 */
KOAN(a_piece_of_text)
{
    KOAN_EQ_STR(__STR, "hello");
    KOAN_EQ_STR(__STR, "two words");
}

/*
 * Blanks are typed because C is typed: every value in C has a type, and the
 * type decides what can be done with it. You have now met three.
 *
 *     __       an int, a whole number        KOAN_EQ_INT(__, 42)
 *     __BOOL   a bool, true or false         KOAN_TRUE(__BOOL)
 *     __CHR    a char, one character         KOAN_EQ_CHR(__CHR, 'k')
 *     __STR    text                          KOAN_EQ_STR(__STR, "hi")
 *
 * More types exist, and more blanks with them. Each is introduced in the
 * lesson that first needs it — you will never meet a blank before the idea
 * behind it.
 */
KOAN(the_types_you_have_met)
{
    KOAN_EQ_INT(__, 1);
    KOAN_TRUE(__BOOL);
    KOAN_EQ_CHR(__CHR, 'c');
    KOAN_EQ_STR(__STR, "text");
}

/*
 * One koan can make several claims. All of them must be true. The runner stops
 * at the first one that is not, so you fix them in the order you meet them.
 *
 * This is also how a koan can build an idea in steps: each line adds a little
 * to the one before it.
 */
KOAN(several_claims_at_once)
{
    KOAN_EQ_INT(__, 2 + 2);
    KOAN_EQ_INT(__, 4 * 2);
    KOAN_TRUE(2 + 2 == __);
    KOAN_FALSE(4 * 2 == __);
}

/*
 * That is the entire mechanism. From here on the koans are about C itself
 * rather than about the koans.
 *
 * The loop, one last time:
 *
 *     1. read the comment above the koan
 *     2. replace the blank with what you believe is true
 *     3. save, and run `make`
 *     4. if it disagrees with you, read what it says and try again
 *
 * The next lesson is about what actually happens when you run `make` — how the
 * text in this file becomes a program.
 */
KOAN(you_know_how_this_works_now)
{
    KOAN_TRUE(__BOOL);
    KOAN_EQ_STR(__STR, "ready");
}

KOAN_LESSON(lesson_about_asserts, "About Asserts",
    KOAN_CASE(a_claim_that_is_true),
    KOAN_CASE(a_claim_that_is_false),
    KOAN_CASE(two_numbers_that_are_equal),
    KOAN_CASE(c_can_calculate),
    KOAN_CASE(asking_whether_things_are_equal),
    KOAN_CASE(a_single_character),
    KOAN_CASE(a_piece_of_text),
    KOAN_CASE(the_types_you_have_met),
    KOAN_CASE(several_claims_at_once),
    KOAN_CASE(you_know_how_this_works_now)
);
