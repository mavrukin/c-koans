/*
 * About _Generic
 *
 * `_Generic` chooses an expression based on the *type* of a controlling
 * expression, at compile time. It is C's only form of overloading, and it is
 * how <tgmath.h> makes one `sqrt` work for float, double and long double.
 *
 * The selection happens during translation. Only the chosen branch is
 * compiled; the others need not even be valid for that type.
 */
#include "koan.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * The controlling expression is not evaluated — only its type is examined.
 */
#define TYPE_NAME(x) _Generic((x),      \
    int:          "int",                \
    unsigned:     "unsigned",           \
    long:         "long",               \
    double:       "double",             \
    float:        "float",              \
    char:         "char",               \
    char *:       "char *",             \
    const char *: "const char *",       \
    default:      "something else")

KOAN(generic_selects_on_type)
{
    int         i = 0;
    double      d = 0;
    char       *s = nullptr;
    const char *cs = nullptr;

    KOAN_EQ_STR(/*__STR*/ "int", TYPE_NAME(i));
    KOAN_EQ_STR(/*__STR*/ "double", TYPE_NAME(d));
    KOAN_EQ_STR(/*__STR*/ "char *", TYPE_NAME(s));
    KOAN_EQ_STR(/*__STR*/ "const char *", TYPE_NAME(cs));

    /* No match, so `default` wins. */
    struct Unrelated { int x; } u = { 0 };
    KOAN_EQ_STR(/*__STR*/ "something else", TYPE_NAME(u));
}

/*
 * The type is the *expression's* type after the usual conversions, not the
 * type you might have written. Literals and promoted operands surprise people.
 */
KOAN(the_type_is_the_expressions_type)
{
    char c = 'x';

    /* A char is promoted to int by arithmetic, so `c + 0` is an int. */
    KOAN_EQ_STR(/*__STR*/ "char", TYPE_NAME(c));
    KOAN_EQ_STR(/*__STR*/ "int", TYPE_NAME(c + 0));

    /* An unsuffixed floating literal is double, not float. */
    KOAN_EQ_STR(/*__STR*/ "double", TYPE_NAME(1.5));
    KOAN_EQ_STR(/*__STR*/ "float", TYPE_NAME(1.5f));

    /* A string literal is char[N], which decays to char * here. */
    KOAN_EQ_STR(/*__STR*/ "char *", TYPE_NAME("text"));

    /* An unsuffixed integer literal is int; the suffix changes that. */
    KOAN_EQ_STR(/*__STR*/ "int", TYPE_NAME(42));
    KOAN_EQ_STR(/*__STR*/ "unsigned", TYPE_NAME(42u));
    KOAN_EQ_STR(/*__STR*/ "long", TYPE_NAME(42L));
}

/*
 * The unselected branches are discarded before they are checked for meaning,
 * so each branch may call a function that only accepts its own type. This is
 * what makes real overloading possible.
 */
static int    abs_int(int n)       { return n < 0 ? -n : n; }
static double abs_double(double d) { return d < 0 ? -d : d; }

#define ABS(x) _Generic((x), int: abs_int, double: abs_double)(x)

KOAN(only_the_selected_branch_is_compiled)
{
    KOAN_EQ_INT(/*__*/ 7, ABS(-7));
    KOAN_EQ_DBL(/*__DBL*/ 2.5, ABS(-2.5), 1e-12);

    /* Had both branches been compiled, abs_int(-2.5) would not have type-checked. */
    KOAN_EQ_INT(/*__*/ 7, ABS(7));
}

/*
 * A generic macro can dispatch to a formatter, giving one call site for many
 * types. Note that the macro must produce a *complete* expression, so the
 * argument is passed after the selection.
 */
static void fmt_int(char *buf, size_t cap, int v)
{
    snprintf(buf, cap, "int(%d)", v);
}
static void fmt_double(char *buf, size_t cap, double v)
{
    snprintf(buf, cap, "double(%.2f)", v);
}
static void fmt_str(char *buf, size_t cap, const char *v)
{
    snprintf(buf, cap, "str(%s)", v);
}

#define FORMAT(buf, cap, x) _Generic((x),   \
    int:          fmt_int,                  \
    double:       fmt_double,               \
    char *:       fmt_str,                  \
    const char *: fmt_str)(buf, cap, x)

KOAN(generic_dispatch_gives_one_call_site)
{
    char buf[64];

    FORMAT(buf, sizeof buf, 42);
    KOAN_EQ_STR(/*__STR*/ "int(42)", buf);

    FORMAT(buf, sizeof buf, 3.14159);
    KOAN_EQ_STR(/*__STR*/ "double(3.14)", buf);

    FORMAT(buf, sizeof buf, "koan");
    KOAN_EQ_STR(/*__STR*/ "str(koan)", buf);
}

/*
 * Pointer types are distinct: `int *` and `const int *` do not match each
 * other, and arrays decay before selection. Getting a `_Generic` to accept
 * both const and non-const usually means listing both.
 */
#define IS_POINTER_TO_INT(x) _Generic((x), \
    int *:       1,                        \
    const int *: 1,                        \
    default:     0)

KOAN(qualifiers_make_distinct_types)
{
    int        n = 0;
    int       *p = &n;
    const int *cp = &n;
    double     d = 0;

    KOAN_EQ_INT(/*__*/ 1, IS_POINTER_TO_INT(p));
    KOAN_EQ_INT(/*__*/ 1, IS_POINTER_TO_INT(cp));
    KOAN_EQ_INT(/*__*/ 0, IS_POINTER_TO_INT(&d));

    /* An array of int decays to int *, so it matches. */
    int arr[4] = {};
    KOAN_EQ_INT(/*__*/ 1, IS_POINTER_TO_INT(arr));
}

/*
 * <tgmath.h> is built entirely from this mechanism: one name, dispatched to
 * the float, double or long double variant. You can build the same thing.
 */
#define SQUARE_ROOT(x) _Generic((x), \
    float:  sqrtf,                   \
    double: sqrt,                    \
    long double: sqrtl)(x)

KOAN(type_generic_maths_is_just_generic)
{
    KOAN_EQ_DBL(/*__DBL*/ 3.0, SQUARE_ROOT(9.0), 1e-12);
    KOAN_EQ_DBL(/*__DBL*/ 2.0, SQUARE_ROOT(4.0f), 1e-6);

    /* The float branch really did use sqrtf: its result is a float, so it
     * carries float precision, not double. */
    KOAN_EQ_SZ(/*__SZ*/ sizeof(float), sizeof(SQUARE_ROOT(2.0f)));
    KOAN_EQ_SZ(/*__SZ*/ sizeof(double), sizeof(SQUARE_ROOT(2.0)));
}

/*
 * Bringing it together.
 *
 * A type-safe dynamic value: a tagged union whose tag is set by _Generic, so
 * the caller cannot label an int as a string. The tag and the payload are
 * filled by one macro, which is the only way to build one — the invariant is
 * enforced by construction rather than by convention.
 *
 * This combines _Generic, tagged unions, compound literals and X-macros.
 */
#define VALUE_KINDS       \
    X(V_INT,    int)      \
    X(V_DOUBLE, double)   \
    X(V_STRING, const char *)

#define X(tag, type) tag,
typedef enum { VALUE_KINDS V_KIND_COUNT } ValueKind;
#undef X

typedef struct {
    ValueKind kind;
    union {
        int         as_int;
        double      as_double;
        const char *as_string;
    };
} Value;

/* The tag is derived from the type, so the two cannot disagree. */
#define KIND_OF(x) _Generic((x),   \
    int:          V_INT,           \
    double:       V_DOUBLE,        \
    char *:       V_STRING,        \
    const char *: V_STRING)

static Value value_int(int v)          { return (Value){ .kind = V_INT,    .as_int = v }; }
static Value value_double(double v)    { return (Value){ .kind = V_DOUBLE, .as_double = v }; }
static Value value_string(const char *v) { return (Value){ .kind = V_STRING, .as_string = v }; }

#define VALUE(x) _Generic((x),   \
    int:          value_int,     \
    double:       value_double,  \
    char *:       value_string,  \
    const char *: value_string)(x)

static const char *value_render(Value v, char *buf, size_t cap)
{
    switch (v.kind) {
        case V_INT:    snprintf(buf, cap, "%d", v.as_int); break;
        case V_DOUBLE: snprintf(buf, cap, "%.1f", v.as_double); break;
        case V_STRING: snprintf(buf, cap, "\"%s\"", v.as_string); break;
        default:       snprintf(buf, cap, "?"); break;
    }
    return buf;
}

KOAN(assembling_a_type_safe_dynamic_value)
{
    char buf[64];

    /* The tag comes from the type at the call site — it cannot be wrong. */
    KOAN_EQ_INT(/*__*/ V_INT, KIND_OF(1));
    KOAN_EQ_INT(/*__*/ V_DOUBLE, KIND_OF(1.0));
    KOAN_EQ_INT(/*__*/ V_STRING, KIND_OF("x"));

    Value a = VALUE(42);
    Value b = VALUE(2.5);
    Value c = VALUE("koan");

    KOAN_EQ_INT(/*__*/ V_INT, a.kind);
    KOAN_EQ_STR(/*__STR*/ "42", value_render(a, buf, sizeof buf));
    KOAN_EQ_STR(/*__STR*/ "2.5", value_render(b, buf, sizeof buf));
    KOAN_EQ_STR(/*__STR*/ "\"koan\"", value_render(c, buf, sizeof buf));

    /* The X-macro list also produced the count, so adding a kind updates
     * the enum and the count together. */
    KOAN_EQ_INT(/*__*/ 3, V_KIND_COUNT);

    /* A heterogeneous array, built with one macro and no casts. */
    Value mixed[] = { VALUE(1), VALUE(2.0), VALUE("three") };
    int   ints = 0;
    for (size_t i = 0; i < 3; i++) if (mixed[i].kind == V_INT) ints++;
    KOAN_EQ_INT(/*__*/ 1, ints);
}

KOAN_LESSON(lesson_about_generic, "About _Generic",
    KOAN_CASE(generic_selects_on_type),
    KOAN_CASE(the_type_is_the_expressions_type),
    KOAN_CASE(only_the_selected_branch_is_compiled),
    KOAN_CASE(generic_dispatch_gives_one_call_site),
    KOAN_CASE(qualifiers_make_distinct_types),
    KOAN_CASE(type_generic_maths_is_just_generic),
    KOAN_CASE(assembling_a_type_safe_dynamic_value)
);
