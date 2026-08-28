/*
 * Support file for about_translation_units.
 *
 * This is a second translation unit, compiled separately and linked with the
 * lesson. It exists so the koans can observe what actually crosses that
 * boundary and what does not.
 *
 * Nothing here has blanks; the koans are next door.
 */
#include "tu_support.h"

/* External linkage: one definition here, visible to anything that declares it. */
int tu_shared_counter = 100;

/* Internal linkage: this name is private to this file. The lesson defines its
 * own `tu_private_value` with a different value, and the two never collide. */
static int tu_private_value = 1;

const char *tu_describe(void)
{
    return "defined in tu_support.c";
}

int tu_bump_shared(void)
{
    return ++tu_shared_counter;
}

int tu_read_private(void)
{
    return tu_private_value;
}

/* A function-local static: one object, shared across every call, but reachable
 * only through this function. */
int tu_next_ticket(void)
{
    static int ticket = 0;
    return ++ticket;
}

/* The definition backing the inline declaration in the header. */
extern inline int tu_double(int n);
