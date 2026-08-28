/*
 * About Translation Units
 *
 * A translation unit is one .c file plus everything it includes. The compiler
 * sees exactly one at a time and knows nothing of the others; the linker joins
 * them afterwards by matching names.
 *
 * That split is why C needs headers, `extern` and `static` at all. This lesson
 * is compiled alongside tu_support.c — open it and tu_support.h as you go.
 */
#include "koan.h"
#include "tu_support.h"

#include <string.h>

/*
 * A name with external linkage is one object across the whole program. The
 * header declared it; tu_support.c defined it; we are looking at the same int.
 */
KOAN(external_linkage_shares_one_object)
{
    KOAN_EQ_INT(/*__*/ 100, tu_shared_counter);

    /* Modified through the other file's function... */
    KOAN_EQ_INT(/*__*/ 101, tu_bump_shared());
    KOAN_EQ_INT(/*__*/ 101, tu_shared_counter);

    /* ...or directly. It is one object, not a copy. */
    tu_shared_counter = 500;
    KOAN_EQ_INT(/*__*/ 501, tu_bump_shared());

    tu_shared_counter = 100;            /* leave it as we found it */
}

/*
 * `static` at file scope means internal linkage: the name is invisible to the
 * linker. Two files may each have their own, with no collision — which is
 * exactly why every helper that need not be shared should be static.
 */
static int tu_private_value = 2;        /* tu_support.c has its own, set to 1 */

KOAN(internal_linkage_keeps_names_private)
{
    KOAN_EQ_INT(/*__*/ 2, tu_private_value);
    KOAN_EQ_INT(/*__*/ 1, tu_read_private());

    /* Same identifier, two distinct objects, and the link still succeeds. */
    KOAN_TRUE(tu_private_value != tu_read_private());
}

/*
 * A function's definition lives in exactly one translation unit. Calling it
 * from another needs only the declaration, which is what the header supplies.
 */
KOAN(declarations_are_enough_to_call)
{
    KOAN_EQ_STR(/*__STR*/ "defined in tu_support.c", tu_describe());
}

/*
 * A function-local static has static storage duration but no linkage: it
 * persists between calls, yet nothing outside that function can name it.
 * Three different meanings for one keyword, and this is the third.
 */
KOAN(function_local_statics_persist_without_linkage)
{
    KOAN_EQ_INT(/*__*/ 1, tu_next_ticket());
    KOAN_EQ_INT(/*__*/ 2, tu_next_ticket());
    KOAN_EQ_INT(/*__*/ 3, tu_next_ticket());
}

/*
 * `inline` in a header lets the body be substituted at each call site. The
 * rule people trip over: one translation unit must also emit a real copy, via
 * `extern inline`, or the link fails when the compiler declines to inline.
 */
KOAN(inline_needs_exactly_one_definition)
{
    KOAN_EQ_INT(/*__*/ 84, tu_double(42));

    /* Taking its address forces the out-of-line copy to be used. */
    int (*fp)(int) = tu_double;
    KOAN_EQ_INT(/*__*/ 20, fp(10));
}

/*
 * Include guards make a header idempotent. Without one, a header included
 * twice — easily done indirectly — would redefine its types and fail to
 * compile. The guard macro is defined on the first pass and skips the rest.
 */
KOAN(include_guards_make_headers_idempotent)
{
#ifdef TU_SUPPORT_H
    KOAN_TRUE(true);                    /* the guard is defined, so it ran */
#else
    KOAN_FAIL("the header should have defined its guard");
#endif

    /* Including it again changes nothing: the guard short-circuits it. */
#include "tu_support.h"
    KOAN_EQ_INT(/*__*/ 100, tu_shared_counter);
}

/*
 * Bringing it together.
 *
 * A registry that two translation units contribute to. The counter lives in
 * tu_support.c, the policy lives here, and neither file can see the other's
 * private state — only what the header promises.
 *
 * This is the shape of every C module boundary: a header of promises, a
 * private implementation, and a discipline of keeping everything else static.
 */
typedef struct {
    const char *label;
    int         ticket;
    int         shared_at_issue;
} Issued;

static Issued issue(const char *label)
{
    return (Issued){
        .label           = label,
        .ticket          = tu_next_ticket(),
        .shared_at_issue = tu_bump_shared(),
    };
}

KOAN(assembling_a_module_boundary)
{
    tu_shared_counter = 0;

    Issued first  = issue("alpha");
    Issued second = issue("beta");

    /* The ticket counter is one object shared by the whole program, so its
     * absolute value depends on who called first. What is guaranteed is that
     * it only ever moves forward, one step at a time. */
    KOAN_EQ_INT(/*__*/ 1, second.ticket - first.ticket);
    KOAN_TRUE(first.ticket > 0);

    /* The shared counter advances once per issue. */
    KOAN_EQ_INT(/*__*/ 1, first.shared_at_issue);
    KOAN_EQ_INT(/*__*/ 2, second.shared_at_issue);
    KOAN_EQ_INT(/*__*/ 2, tu_shared_counter);

    KOAN_EQ_STR(/*__STR*/ "beta", second.label);

    /* Our private value is untouched by any of it. */
    KOAN_EQ_INT(/*__*/ 2, tu_private_value);

    tu_shared_counter = 100;
}

KOAN_LESSON(lesson_about_translation_units, "About Translation Units",
    KOAN_CASE(external_linkage_shares_one_object),
    KOAN_CASE(internal_linkage_keeps_names_private),
    KOAN_CASE(declarations_are_enough_to_call),
    KOAN_CASE(function_local_statics_persist_without_linkage),
    KOAN_CASE(inline_needs_exactly_one_definition),
    KOAN_CASE(include_guards_make_headers_idempotent),
    KOAN_CASE(assembling_a_module_boundary)
);
