/*
 * About Initialisation and Lifetime
 *
 * C distinguishes *when* an object comes into existence from *how* it is
 * given a value. Getting the two confused produces the two classic bugs:
 * reading uninitialised memory, and holding a pointer past the end of the
 * object's life.
 *
 * The four storage durations: static, automatic, allocated, thread.
 */
#include "koan.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Static-duration objects are zeroed before main runs. Automatic ones are
 * not — an uninitialised local holds whatever was on the stack.
 */
static int file_scope_int;              /* zeroed, guaranteed */
static char file_scope_buf[8];

KOAN(static_duration_starts_zeroed)
{
    KOAN_EQ_INT(__, file_scope_int);
    KOAN_EQ_SZ(__SZ, strlen(file_scope_buf));

    /* A function-local static is zeroed once, not on each call. */
    static int counter;
    counter++;
    KOAN_TRUE(counter >= 1);
}

/*
 * A static initialiser must be a constant expression: it is written into the
 * program image, not computed at startup. `constexpr` is how you build one
 * with a name.
 */
constexpr int TABLE_SIZE = 4;
static const int SQUARES[TABLE_SIZE] = { 0, 1, 4, 9 };

KOAN(static_initialisers_are_constant_expressions)
{
    KOAN_EQ_INT(__, SQUARES[3]);
    KOAN_EQ_SZ(__SZ, sizeof SQUARES / sizeof *SQUARES);

    /* An automatic object has no such restriction — it may be initialised
     * from anything in scope, including a function call. */
    int computed[TABLE_SIZE] = { [0] = SQUARES[1] + SQUARES[2] };
    KOAN_EQ_INT(__, computed[0]);
    KOAN_EQ_INT(__, computed[3]);
}

/*
 * A compound literal creates an unnamed object *and* gives you its address.
 * It is not a constant: it is a real object with a storage duration, which is
 * the whole subtlety.
 */
typedef struct { int x, y; } Point;

static int manhattan(const Point *p) { return abs(p->x) + abs(p->y); }

KOAN(compound_literals_are_objects_not_constants)
{
    /* Passing an address without naming a variable first. */
    KOAN_EQ_INT(__, manhattan(&(Point){ .x = 3, .y = -4 }));

    /* It is modifiable, unlike a string literal. */
    Point *p = &(Point){ .x = 1, .y = 2 };
    p->x = 10;
    KOAN_EQ_INT(__, manhattan(p));

    /* An array compound literal decays like any other array. */
    int sum = 0;
    for (size_t i = 0; i < 4; i++) sum += (int[]){ 1, 2, 3, 4 }[i];
    KOAN_EQ_INT(__, sum);
}

/*
 * Here is the trap. A compound literal at block scope has *automatic*
 * duration: it dies at the end of the enclosing block, exactly like a local.
 * Returning its address is a use-after-return.
 */
static const Point *safe_static_literal(void)
{
    /* At file scope, or declared static, the literal has static duration. */
    static const Point origin = { .x = 0, .y = 0 };
    return &origin;
}

KOAN(compound_literal_lifetime_is_its_block)
{
    const Point *outer = nullptr;

    {
        Point *inner = &(Point){ .x = 5, .y = 5 };
        KOAN_EQ_INT(__, manhattan(inner));
        outer = inner;
    }
    /* `outer` is now dangling. We do not dereference it; we retire it. */
    outer = nullptr;
    KOAN_TRUE(outer == nullptr);

    /* The version that is safe to return. */
    KOAN_EQ_INT(__, manhattan(safe_static_literal()));
}

/*
 * Compound literals shine as anonymous arguments and as a way to reset a
 * struct to a known state in one assignment.
 */
typedef struct {
    const char *name;
    int         retries;
    bool        verbose;
} Options;

static int total_retries(Options o) { return o.retries; }

KOAN(compound_literals_make_clean_arguments)
{
    /* Named arguments, in effect: the reader can see what each value means. */
    KOAN_EQ_INT(__, total_retries((Options){ .name = "fetch", .retries = 3 }));

    /* Unmentioned members are zeroed, so defaults are free. */
    Options o = (Options){ .name = "x" };
    KOAN_EQ_INT(__, o.retries);
    KOAN_EQ_INT(__, o.verbose);

    /* Reset in one assignment, with no memset and no member-by-member reset. */
    o.retries = 9;
    o = (Options){ 0 };
    KOAN_EQ_INT(__, o.retries);
    KOAN_TRUE(o.name == nullptr);
}

/*
 * Designated initialisers may be combined with ranges of ordinary ones, and
 * later entries overwrite earlier ones. The last mention of an index wins.
 */
KOAN(later_initialisers_overwrite_earlier_ones)
{
KOAN_DELIBERATE_BEGIN
    int v[5] = { [0] = 1, [1] = 2, [0] = 99 };
KOAN_DELIBERATE_END
    KOAN_EQ_INT(__, v[0]);
    KOAN_EQ_INT(__, v[1]);

    /* Positional entries resume from the last designated index. */
    int w[6] = { [2] = 30, 40, 50 };
    KOAN_EQ_INT(__, w[1]);
    KOAN_EQ_INT(__, w[2]);
    KOAN_EQ_INT(__, w[3]);
    KOAN_EQ_INT(__, w[4]);
    KOAN_EQ_INT(__, w[5]);
}

/*
 * Nested designators reach into sub-objects, which keeps a table of structs
 * readable without a pile of braces.
 */
typedef struct { Point topleft, size; } Rect;

KOAN(designators_nest)
{
    Rect r = { .topleft = { .x = 1, .y = 2 }, .size.x = 10, .size.y = 20 };

    KOAN_EQ_INT(__, r.topleft.x);
    KOAN_EQ_INT(__, r.size.y);

    /* Anything not mentioned is still zeroed, at any depth. */
    Rect partial = { .size.x = 5 };
    KOAN_EQ_INT(__, partial.topleft.x);
    KOAN_EQ_INT(__, partial.size.y);
}

/*
 * Bringing it together.
 *
 * A configuration table built entirely from designated initialisers and
 * compound literals, with static duration so the whole thing lives in the
 * program image and costs nothing to construct at runtime.
 *
 * The lookup is by enum, and the static_assert guarantees the table has an
 * entry for every enumerator — so adding a mode without a row fails the build
 * rather than returning a zeroed entry at runtime.
 */
typedef enum { MODE_FAST, MODE_SAFE, MODE_DEBUG, MODE_COUNT } Mode;

typedef struct {
    const char *name;
    int         retries;
    int         timeout_ms;
    bool        verbose;
} Profile;

static const Profile PROFILES[MODE_COUNT] = {
    [MODE_FAST]  = { .name = "fast",  .retries = 0, .timeout_ms = 100 },
    [MODE_SAFE]  = { .name = "safe",  .retries = 5, .timeout_ms = 5000 },
    [MODE_DEBUG] = { .name = "debug", .retries = 1, .timeout_ms = 0,
                     .verbose = true },
};

static_assert(sizeof PROFILES / sizeof *PROFILES == MODE_COUNT,
              "every Mode needs a Profile");

/* Overlay: start from a profile, then apply only what the caller overrode. */
static Profile with_timeout(Mode m, int timeout_ms)
{
    Profile p = PROFILES[m];        /* struct assignment copies the whole row */
    p.timeout_ms = timeout_ms;
    return p;
}

KOAN(assembling_a_configuration_table)
{
    KOAN_EQ_STR(__STR, PROFILES[MODE_SAFE].name);
    KOAN_EQ_INT(__, PROFILES[MODE_SAFE].retries);

    /* Unmentioned members are zeroed even inside a designated table. */
    KOAN_EQ_INT(__, PROFILES[MODE_FAST].verbose);
    KOAN_EQ_INT(__, PROFILES[MODE_DEBUG].verbose);
    KOAN_EQ_INT(__, PROFILES[MODE_DEBUG].timeout_ms);

    /* The overlay copies rather than aliases: the table is untouched. */
    Profile tuned = with_timeout(MODE_SAFE, 250);
    KOAN_EQ_INT(__, tuned.timeout_ms);
    KOAN_EQ_INT(__, PROFILES[MODE_SAFE].timeout_ms);
    KOAN_EQ_STR(__STR, tuned.name);

    /* A compound literal supplies a one-off profile with no table entry. */
    Profile ad_hoc = (Profile){ .name = "once", .retries = 2 };
    KOAN_EQ_INT(__, ad_hoc.retries);
    KOAN_EQ_INT(__, ad_hoc.timeout_ms);
}

KOAN_LESSON(lesson_about_initialisation, "About Initialisation and Lifetime",
    KOAN_CASE(static_duration_starts_zeroed),
    KOAN_CASE(static_initialisers_are_constant_expressions),
    KOAN_CASE(compound_literals_are_objects_not_constants),
    KOAN_CASE(compound_literal_lifetime_is_its_block),
    KOAN_CASE(compound_literals_make_clean_arguments),
    KOAN_CASE(later_initialisers_overwrite_earlier_ones),
    KOAN_CASE(designators_nest),
    KOAN_CASE(assembling_a_configuration_table)
);
