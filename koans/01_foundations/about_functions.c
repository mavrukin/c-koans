/*
 * About Functions
 *
 * C passes everything by value. Every argument is copied into the callee, and
 * the callee cannot reach back to the caller's variables — unless you hand it
 * an address. Once you have internalised that single rule, the whole
 * pointer-heavy style of C stops looking like decoration and starts looking
 * like the only way to say what you mean.
 */
#include "koan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The callee receives a copy. Modifying it cannot affect the caller.
 */
KOAN_DELIBERATE_BEGIN
static void try_to_modify(int copy)
{
    copy = 99;    /* writes to the copy, which nobody will ever read again */
}
KOAN_DELIBERATE_END

KOAN(arguments_are_copied)
{
    int original = 1;
    try_to_modify(original);
    KOAN_EQ_INT(__, original);
}

/*
 * To let a function change a caller's variable, pass its address. The
 * parameter is still copied — but what is copied is a pointer, and both
 * copies point at the same object.
 */
static void actually_modify(int *target)
{
    *target = 99;
}

KOAN(pass_an_address_to_modify)
{
    int original = 1;
    actually_modify(&original);
    KOAN_EQ_INT(__, original);
}

/*
 * The same rule explains the classic broken swap. Reassigning the *pointer*
 * inside the callee changes only the callee's copy of the pointer.
 */
static void broken_swap(int *a, int *b)
{
    int *tmp = a;
    a = b;
    b = tmp;              /* rearranges local copies, nothing else */
}

static void working_swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;             /* rearranges what they point at */
}

KOAN(swapping_requires_dereferencing)
{
    int x = 1, y = 2;

    broken_swap(&x, &y);
    KOAN_EQ_INT(__, x);
    KOAN_EQ_INT(__, y);

    working_swap(&x, &y);
    KOAN_EQ_INT(__, x);
    KOAN_EQ_INT(__, y);
}

/*
 * A function returning nothing still has a return type. A function taking no
 * arguments must say `(void)`: in C an empty `()` historically meant
 * "unspecified arguments", not "none". C23 finally made `()` mean `(void)`,
 * but writing `(void)` remains clearer and works everywhere.
 */
static int no_arguments(void) { return 7; }

KOAN(void_means_no_arguments)
{
    KOAN_EQ_INT(__, no_arguments());
}

/*
 * `static` at file scope means internal linkage: the name is not visible to
 * other translation units. It is the closest thing C has to `private`, and
 * almost every helper you write should have it.
 */
static int hidden_helper(int n) { return n * 2; }

KOAN(static_limits_visibility)
{
    KOAN_EQ_INT(__, hidden_helper(21));
}

/*
 * `static` on a *local* variable means something entirely different: static
 * storage duration. The variable is initialised once and persists across
 * calls, which makes the function stateful — and not thread-safe.
 */
static int next_id(void)
{
    static int counter = 0;   /* initialised once, not on every call */
    return ++counter;
}

KOAN(static_locals_persist_between_calls)
{
    KOAN_EQ_INT(__, next_id());
    KOAN_EQ_INT(__, next_id());
    KOAN_EQ_INT(__, next_id());
}

/*
 * A block introduces a scope, and an inner declaration shadows an outer one
 * for the rest of that block. The outer variable is untouched.
 */
KOAN(inner_scopes_shadow_outer_ones)
{
    int value = 1;
KOAN_DELIBERATE_BEGIN
    {
        /* A genuine shadow: same name, new object. -Wshadow exists to catch
         * this, because it is far more often a mistake than an intention. */
        int value = 2;
        KOAN_EQ_INT(__, value);
    }
KOAN_DELIBERATE_END
    KOAN_EQ_INT(__, value);
}

/*
 * Recursion needs a base case that is actually reachable. Each call gets its
 * own copy of every parameter and local, on the stack.
 */
static unsigned long long factorial(unsigned n)
{
    return n <= 1 ? 1ull : n * factorial(n - 1);
}

KOAN(recursion_gives_each_call_its_own_frame)
{
    KOAN_EQ_INT(__, (int)factorial(0));
    KOAN_EQ_INT(__, (int)factorial(5));
}

/*
 * Returning multiple values is done with out-parameters, and the convention
 * is to return a status and write the results through pointers. Check the
 * status before trusting the outputs.
 */
static bool divide(int num, int den, int *quotient, int *remainder)
{
    if (den == 0) return false;
    if (quotient)  *quotient  = num / den;
    if (remainder) *remainder = num % den;
    return true;
}

KOAN(out_parameters_return_extra_values)
{
    int q = -1, r = -1;

    KOAN_EQ_INT(__, divide(17, 5, &q, &r));
    KOAN_EQ_INT(__, q);
    KOAN_EQ_INT(__, r);

    /* On failure the outputs are left alone, so the caller must check first. */
    q = -1; r = -1;
    KOAN_EQ_INT(__, divide(17, 0, &q, &r));
    KOAN_EQ_INT(__, q);

    /* Passing null for an output you do not want is a common courtesy. */
    int only_q = 0;
    divide(9, 2, &only_q, nullptr);
    KOAN_EQ_INT(__, only_q);
}

/*
 * A `const` parameter is a promise to the caller that the function will not
 * write through that pointer. It costs nothing and documents intent
 * precisely; take const pointers whenever you only read.
 */
static size_t count_char(const char *s, char needle)
{
    size_t n = 0;
    for (size_t i = 0; s[i] != '\0'; i++)
        if (s[i] == needle) n++;
    return n;
}

KOAN(const_parameters_promise_not_to_write)
{
    KOAN_EQ_SZ(__SZ, count_char("mississippi", 's'));
    KOAN_EQ_SZ(__SZ, count_char("mississippi", 'i'));
    KOAN_EQ_SZ(__SZ, count_char("mississippi", 'p'));
    KOAN_EQ_SZ(__SZ, count_char("mississippi", 'z'));
}

/*
 * Returning a pointer to a local is a use-after-return: the storage is gone
 * the moment the function returns. There are three correct alternatives, and
 * knowing which to reach for is most of the skill.
 */
static const char *safe_static(void)
{
    static const char msg[] = "static storage outlives the call";
    return msg;                      /* fine: static duration */
}

static void safe_caller_buffer(char *out, size_t cap)
{
    snprintf(out, cap, "the caller owns this");   /* fine: caller's memory */
}

KOAN(never_return_a_pointer_to_a_local)
{
    KOAN_EQ_STR(__STR, safe_static());

    char buf[64];
    safe_caller_buffer(buf, sizeof buf);
    KOAN_EQ_STR(__STR, buf);
}

/*
 * Bringing it together.
 *
 * A tokeniser that writes into caller-supplied storage, reports how many
 * tokens it found, and never allocates. This is the shape of a great many
 * real C APIs: caller owns the memory, callee fills it, status comes back as
 * the return value.
 *
 * Note the cap check — refusing to overflow the caller's array is the
 * function's responsibility, not the caller's.
 */
static size_t split(const char *s, char sep, char out[][16], size_t cap)
{
    size_t count = 0, len = 0;
    char   current[16];

    for (size_t i = 0; ; i++) {
        char c = s[i];
        if (c == sep || c == '\0') {
            if (count < cap) {
                current[len] = '\0';
                memcpy(out[count], current, len + 1);
                count++;
            }
            len = 0;
            if (c == '\0') break;
        } else if (len + 1 < sizeof current) {
            current[len++] = c;
        }
    }
    return count;
}

KOAN(assembling_a_tokeniser)
{
    char parts[4][16];

    KOAN_EQ_SZ(__SZ, split("alpha,beta,gamma", ',', parts, 4));
    KOAN_EQ_STR(__STR, parts[0]);
    KOAN_EQ_STR(__STR, parts[2]);

    /* An empty field between two separators is still a field. */
    KOAN_EQ_SZ(__SZ, split("a,,c", ',', parts, 4));
    KOAN_EQ_STR(__STR, parts[1]);

    /* And the cap is respected rather than overrun. */
    KOAN_EQ_SZ(__SZ, split("1,2,3,4,5,6", ',', parts, 4));
}

KOAN_LESSON(lesson_about_functions, "About Functions",
    KOAN_CASE(arguments_are_copied),
    KOAN_CASE(pass_an_address_to_modify),
    KOAN_CASE(swapping_requires_dereferencing),
    KOAN_CASE(void_means_no_arguments),
    KOAN_CASE(static_limits_visibility),
    KOAN_CASE(static_locals_persist_between_calls),
    KOAN_CASE(inner_scopes_shadow_outer_ones),
    KOAN_CASE(recursion_gives_each_call_its_own_frame),
    KOAN_CASE(out_parameters_return_extra_values),
    KOAN_CASE(const_parameters_promise_not_to_write),
    KOAN_CASE(never_return_a_pointer_to_a_local),
    KOAN_CASE(assembling_a_tokeniser)
);
