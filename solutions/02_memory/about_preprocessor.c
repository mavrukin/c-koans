/*
 * About the Preprocessor
 *
 * The preprocessor is a separate language that runs before C does. It knows
 * nothing about types, scopes or expressions — it substitutes tokens. Almost
 * every macro bug comes from forgetting that.
 *
 * Modern C needs macros far less than old C did: `constexpr` replaces most
 * `#define` constants, and `static inline` replaces most function-like
 * macros. What remains is the work only the preprocessor can do — stringizing,
 * token pasting, conditional compilation, and generating repetitive code.
 */
#include "koan.h"

#include <stdio.h>
#include <string.h>

/*
 * A macro is textual substitution. Without parentheses the surrounding
 * expression re-associates the result, which is the classic macro bug.
 */
#define BAD_DOUBLE(x)  x * 2
#define GOOD_DOUBLE(x) ((x) * 2)

KOAN(macros_substitute_tokens_not_values)
{
    /* BAD_DOUBLE(1 + 1) becomes 1 + 1 * 2, which is 3. */
    KOAN_EQ_INT(/*__*/ 3, BAD_DOUBLE(1 + 1));

    /* GOOD_DOUBLE(1 + 1) becomes ((1 + 1) * 2), which is 4. */
    KOAN_EQ_INT(/*__*/ 4, GOOD_DOUBLE(1 + 1));
}

/*
 * A macro argument used twice is *evaluated* twice. If the argument has a
 * side effect, it happens twice too. This is why the standard library's
 * min/max are not macros, and why you should prefer a static inline function.
 */
#define UNSAFE_MAX(a, b) ((a) > (b) ? (a) : (b))

static inline int safe_max(int a, int b) { return a > b ? a : b; }

KOAN(macro_arguments_are_evaluated_more_than_once)
{
    int values[] = { 10, 20 };

    /*
     * The expansion is ((values[i++]) > (5) ? (values[i++]) : (5)).
     * The condition reads values[0] and leaves i at 1; the winning branch
     * then reads values[1]. So the "maximum of values[0] and 5" comes back
     * as 20 — a number that appears nowhere in the comparison.
     */
    int i = 0;
    int result = UNSAFE_MAX(values[i++], 5);
    KOAN_EQ_INT(/*__*/ 2, i);
    KOAN_EQ_INT(/*__*/ 20, result);

    /* A function evaluates each argument exactly once, as you would expect. */
    i = 0;
    KOAN_EQ_INT(/*__*/ 10, safe_max(values[i++], 5));
    KOAN_EQ_INT(/*__*/ 1, i);
}

/*
 * # stringizes an argument; ## pastes two tokens into one. These are the two
 * things no function can do, and the only reason some macros must stay macros.
 */
#define STRINGIZE(x)     #x
#define EXPAND_THEN(x)   STRINGIZE(x)
#define CONCAT(a, b)     a##b

#define WIDTH 80

KOAN(stringize_and_paste_are_macro_only)
{
    KOAN_EQ_STR(/*__STR*/ "1 + 1", STRINGIZE(1 + 1));

    /* # does not expand its argument first, which is why the two-level
     * dance exists: STRINGIZE(WIDTH) is "WIDTH", EXPAND_THEN(WIDTH) is "80". */
    KOAN_EQ_STR(/*__STR*/ "WIDTH", STRINGIZE(WIDTH));
    KOAN_EQ_STR(/*__STR*/ "80", EXPAND_THEN(WIDTH));

    int counter_1 = 42;
    KOAN_EQ_INT(/*__*/ 42, CONCAT(counter_, 1));
}

/*
 * A multi-statement macro must be wrapped in do { } while (0) so that it
 * behaves as a single statement — otherwise it breaks when used as the body
 * of an unbraced if.
 */
#define SET_BOTH(a, b, v) do { (a) = (v); (b) = (v); } while (0)

KOAN(do_while_zero_makes_a_macro_one_statement)
{
    int x = 0, y = 0;

    if (true)
        SET_BOTH(x, y, 7);      /* the trailing semicolon works correctly */
    else
        SET_BOTH(x, y, 9);

    KOAN_EQ_INT(/*__*/ 7, x);
    KOAN_EQ_INT(/*__*/ 7, y);
}

/*
 * Variadic macros forward an argument list. C23's __VA_OPT__ solves the
 * long-standing problem of the dangling comma when the list is empty — before
 * it, this required compiler extensions.
 */
#define LOG_TO(buf, cap, fmt, ...) \
    snprintf((buf), (cap), "[log] " fmt __VA_OPT__(,) __VA_ARGS__)

KOAN(va_opt_handles_the_empty_argument_list)
{
    char buf[64];

    LOG_TO(buf, sizeof buf, "starting");
    KOAN_EQ_STR(/*__STR*/ "[log] starting", buf);

    LOG_TO(buf, sizeof buf, "port %d", 8080);
    KOAN_EQ_STR(/*__STR*/ "[log] port 8080", buf);
}

/*
 * Conditional compilation selects code before the compiler sees it. C23 added
 * #elifdef and #elifndef, tidying up a chain that previously needed
 * `#elif defined(...)`.
 */
#define FEATURE_B

KOAN(conditional_compilation_selects_before_compiling)
{
    const char *chosen;

#ifdef FEATURE_A
    chosen = "a";
#elifdef FEATURE_B
    chosen = "b";
#else
    chosen = "none";
#endif

    KOAN_EQ_STR(/*__STR*/ "b", chosen);

    /* __has_include lets you probe for a header rather than guess from
     * platform macros — far more robust, and standard as of C23. */
#if __has_include(<string.h>)
    KOAN_TRUE(true);
#else
    KOAN_FAIL("string.h should exist everywhere");
#endif
}

/*
 * Predefined macros describe the translation itself. __STDC_VERSION__ is how
 * you detect the language level: 202311L is C23, though compilers that shipped
 * it before ratification report the draft value 202000L — see about_compiling.
 */
KOAN(predefined_macros_describe_the_translation)
{
    KOAN_TRUE(__STDC_VERSION__ >= 202000L);

    /* __LINE__ and __FILE__ are how the koan runner reports where you are. */
    int line_here = __LINE__;
    int line_next = __LINE__;
    KOAN_EQ_INT(/*__*/ 1, line_next - line_here);

    KOAN_TRUE(strstr(__FILE__, "about_preprocessor") != nullptr);

    /* __func__ is not a macro at all — it is a predefined identifier, so it
     * behaves like a local variable holding the function's name. */
    KOAN_EQ_STR(/*__STR*/ "predefined_macros_describe_the_translation", __func__);
}

/*
 * Bringing it together.
 *
 * The X-macro: a single list, expanded several times with different meanings.
 * It is how you keep an enum, its names, and its behaviour from ever drifting
 * apart — the compiler generates all three from one source.
 *
 * This is not a curiosity. The koan runner's own manifest is an X-macro, and
 * you will use one again in the JSON parser capstone.
 */
#define HTTP_STATUS_LIST                    \
    X(OK,        200, "OK")                 \
    X(NOT_FOUND, 404, "Not Found")          \
    X(TEAPOT,    418, "I'm a teapot")       \
    X(SERVER,    500, "Internal Server Error")

/* First expansion: build the enum. */
#define X(name, code, text) HTTP_##name = code,
enum HttpStatus { HTTP_STATUS_LIST };
#undef X

/* Second expansion: build the lookup table. No chance of it going stale. */
typedef struct { int code; const char *text; } StatusEntry;

#define X(name, code, text) { code, text },
static const StatusEntry STATUS_TABLE[] = { HTTP_STATUS_LIST };
#undef X

/* Third expansion: count the entries, again from the same list. */
#define X(name, code, text) + 1
static const size_t STATUS_COUNT = 0 HTTP_STATUS_LIST;
#undef X

static const char *status_text(int code)
{
    for (size_t i = 0; i < STATUS_COUNT; i++)
        if (STATUS_TABLE[i].code == code) return STATUS_TABLE[i].text;
    return "Unknown";
}

KOAN(assembling_an_x_macro_table)
{
    /* The enum came from the list. */
    KOAN_EQ_INT(/*__*/ 200, HTTP_OK);
    KOAN_EQ_INT(/*__*/ 404, HTTP_NOT_FOUND);
    KOAN_EQ_INT(/*__*/ 418, HTTP_TEAPOT);

    /* So did the table, and so did its length. */
    KOAN_EQ_SZ(/*__SZ*/ 4, STATUS_COUNT);
    KOAN_EQ_STR(/*__STR*/ "Not Found", status_text(404));
    KOAN_EQ_STR(/*__STR*/ "I'm a teapot", status_text(HTTP_TEAPOT));
    KOAN_EQ_STR(/*__STR*/ "Unknown", status_text(999));

    /* Adding a status to HTTP_STATUS_LIST would update all three at once.
     * That is the entire point: one list, no drift. */
}

KOAN_LESSON(lesson_about_preprocessor, "About the Preprocessor",
    KOAN_CASE(macros_substitute_tokens_not_values),
    KOAN_CASE(macro_arguments_are_evaluated_more_than_once),
    KOAN_CASE(stringize_and_paste_are_macro_only),
    KOAN_CASE(do_while_zero_makes_a_macro_one_statement),
    KOAN_CASE(va_opt_handles_the_empty_argument_list),
    KOAN_CASE(conditional_compilation_selects_before_compiling),
    KOAN_CASE(predefined_macros_describe_the_translation),
    KOAN_CASE(assembling_an_x_macro_table)
);
