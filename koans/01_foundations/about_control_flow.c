/*
 * About Control Flow
 *
 * C's control flow is small: two conditionals, three loops, a jump, and a
 * switch that behaves unlike the switch in any language written since. The
 * subtleties are in the edges — fallthrough, the dangling else, what `break`
 * actually breaks out of, and the one legitimate use of goto.
 */
#include "koan.h"

#include <string.h>

/*
 * There is no dedicated boolean test: any scalar works, and zero is the only
 * false value. Null pointers are false; every other pointer is true.
 */
KOAN(any_scalar_is_a_condition)
{
    int taken = 0;

    if (1)        taken += 1;
    if (0)        taken += 10;
    if (-1)       taken += 100;   /* nonzero is true, including negatives */
    if (0.0)      taken += 1000;
    if ("")       taken += 10000; /* a pointer to the empty string, not null */

    KOAN_EQ_INT(__, taken);
}

/*
 * `else` binds to the nearest unmatched `if`, regardless of how the code is
 * indented. This is the dangling-else problem, and braces are the cure.
 */
KOAN(else_binds_to_the_nearest_if)
{
    int a = 1, b = 0;
    const char *result = "none";

    /* Written without braces, the else belongs to the *inner* if. */
KOAN_DELIBERATE_BEGIN
    if (a)
        if (b)
            result = "both";
        else
            result = "a but not b";
KOAN_DELIBERATE_END

    KOAN_EQ_STR(__STR, result);
}

/*
 * A while loop tests before the body; a do-while tests after, so its body
 * always runs at least once. That single difference decides which to use.
 */
KOAN(do_while_always_runs_once)
{
    int while_runs = 0;
    while (false) while_runs++;

    int do_runs = 0;
    do { do_runs++; } while (false);

    KOAN_EQ_INT(__, while_runs);
    KOAN_EQ_INT(__, do_runs);
}

/*
 * A for loop is a while loop with its three clauses gathered in one place.
 * Any of the three may be omitted; omitting the condition means "true".
 */
KOAN(for_is_while_with_the_parts_collected)
{
    int sum = 0;
    for (int i = 1; i <= 4; i++) sum += i;
    KOAN_EQ_INT(__, sum);

    /* The loop variable declared in the header is scoped to the loop. */
    int count = 0;
    for (int i = 0; i < 3; i++) count++;
    KOAN_EQ_INT(__, count);

    /* An empty condition loops forever, so the exit must be in the body. */
    int n = 0;
    for (;;) { if (++n == 5) break; }
    KOAN_EQ_INT(__, n);
}

/*
 * `break` leaves the innermost loop or switch. `continue` skips to the next
 * iteration — which, in a for loop, still runs the increment clause.
 */
KOAN(break_and_continue_affect_the_innermost_loop)
{
    int evens = 0;
    for (int i = 0; i < 10; i++) {
        if (i % 2 != 0) continue;
        evens++;
    }
    KOAN_EQ_INT(__, evens);

    /* break escapes only one level, so this inner break leaves 3 outer passes. */
    int inner_iterations = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 100; j++) {
            if (j == 2) break;
            inner_iterations++;
        }
    KOAN_EQ_INT(__, inner_iterations);
}

/*
 * switch compares against integer constant expressions and jumps to the
 * matching label. Execution then *continues through* the following labels
 * until a break. This is the famous fallthrough.
 */
KOAN(switch_cases_fall_through)
{
    int reached = 0;

KOAN_DELIBERATE_BEGIN
    switch (2) {
        case 1: reached += 1;      /* not reached */
        case 2: reached += 10;     /* entry point, then falls through */
        case 3: reached += 100;    /* also runs, with no break above */
                break;
        case 4: reached += 1000;
    }
KOAN_DELIBERATE_END

    KOAN_EQ_INT(__, reached);
}

/*
 * Deliberate fallthrough should be announced. C23 standardised the attribute
 * for exactly this, replacing a generation of hand-written FALLTHROUGH
 * comments that no compiler ever checked.
 */
KOAN(fallthrough_can_be_declared)
{
    int weight = 0;

    switch (1) {
        case 1:
            weight += 1;
            [[fallthrough]];
        case 2:
            weight += 10;
            break;
        case 3:
            weight += 100;
            break;
    }

    KOAN_EQ_INT(__, weight);
}

/*
 * `default` need not come last, and a switch with no match simply does
 * nothing. Grouped labels are the idiomatic way to share a body.
 */
KOAN(switch_grouping_and_default)
{
    /* Classify a character without a chain of ifs. */
    const char *input = "a1!z";
    int letters = 0, digits = 0, other = 0;

    for (size_t i = 0; input[i] != '\0'; i++) {
        switch (input[i]) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                digits++;
                break;
            case 'a': case 'z':
                letters++;
                break;
            default:
                other++;
        }
    }

    KOAN_EQ_INT(__, letters);
    KOAN_EQ_INT(__, digits);
    KOAN_EQ_INT(__, other);
}

/*
 * goto is restricted to the current function, and there is one pattern where
 * it is not merely acceptable but clearly best: unwinding a partially built
 * set of resources on an error path, without repeating the cleanup.
 *
 * You will use this shape constantly once you reach malloc and open().
 */
static int acquire_three(bool third_fails, int *unwound)
{
    int held = 0;

    held++;                       /* acquired resource A */
    held++;                       /* acquired resource B */
    if (third_fails) goto release_b;
    held++;                       /* acquired resource C */

    /* success: release everything in reverse order */
    held--;
release_b:
    held--;
    held--;
    *unwound = held;
    return third_fails ? -1 : 0;
}

KOAN(goto_unwinds_error_paths)
{
    int unwound = -1;

    KOAN_EQ_INT(__, acquire_three(false, &unwound));
    KOAN_EQ_INT(__, unwound);

    KOAN_EQ_INT(__, acquire_three(true, &unwound));
    KOAN_EQ_INT(__, unwound);
}

/*
 * C23 allows a label at the end of a compound statement, where previously a
 * null statement was required (`done: ;`). A small fix to a long-standing
 * annoyance.
 */
KOAN(labels_may_end_a_block)
{
    int n = 0;
    for (int i = 0; i < 5; i++) {
        if (i == 3) goto done;
        n++;
    }
done:
    KOAN_EQ_INT(__, n);
}

/*
 * Bringing it together.
 *
 * Run-length encode a string: "aaabbc" becomes "a3b2c1". This needs a loop
 * with lookahead, a nested loop, careful bounds, and a decision about what to
 * do at the end of the input. Predict what the encoder produces.
 */
static size_t rle(const char *in, char *out, size_t cap)
{
    size_t w = 0;

    for (size_t i = 0; in[i] != '\0'; ) {
        char c = in[i];
        size_t run = 0;
        while (in[i + run] == c) run++;

        if (w + 2 >= cap) break;          /* leave room for the NUL */
        out[w++] = c;
        out[w++] = (char)('0' + (run > 9 ? 9 : run));
        i += run;
    }

    out[w] = '\0';
    return w;
}

KOAN(assembling_a_run_length_encoder)
{
    char buf[32];

    KOAN_EQ_SZ(__SZ, rle("aaabbc", buf, sizeof buf));
    KOAN_EQ_STR(__STR, buf);

    rle("", buf, sizeof buf);
    KOAN_EQ_STR(__STR, buf);

    rle("xyz", buf, sizeof buf);
    KOAN_EQ_STR(__STR, buf);

    /* A run longer than nine is clamped by the single-digit encoding. */
    rle("aaaaaaaaaaaa", buf, sizeof buf);
    KOAN_EQ_STR(__STR, buf);
}

KOAN_LESSON(lesson_about_control_flow, "About Control Flow",
    KOAN_CASE(any_scalar_is_a_condition),
    KOAN_CASE(else_binds_to_the_nearest_if),
    KOAN_CASE(do_while_always_runs_once),
    KOAN_CASE(for_is_while_with_the_parts_collected),
    KOAN_CASE(break_and_continue_affect_the_innermost_loop),
    KOAN_CASE(switch_cases_fall_through),
    KOAN_CASE(fallthrough_can_be_declared),
    KOAN_CASE(switch_grouping_and_default),
    KOAN_CASE(goto_unwinds_error_paths),
    KOAN_CASE(labels_may_end_a_block),
    KOAN_CASE(assembling_a_run_length_encoder)
);
