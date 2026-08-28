/*
 * About Function Pointers
 *
 * A function pointer lets you pass behaviour the way you pass data. It is how
 * C does polymorphism, callbacks, plugins and virtual dispatch — all of them,
 * with one mechanism.
 *
 * The declaration syntax is the only genuinely hard part, and a typedef makes
 * it go away. Read from the identifier outwards:
 *
 *     int (*fp)(int, int)      fp is a pointer to a function
 *                              taking (int, int) and returning int
 *
 * Note the parentheses around *fp. Without them, `int *fp(int, int)` declares
 * a function returning int* — an entirely different thing.
 */
#include "koan.h"

#include <stdlib.h>
#include <string.h>

static int add(int a, int b) { return a + b; }
static int mul(int a, int b) { return a * b; }

/*
 * A function name decays to a pointer to that function, just as an array name
 * decays to a pointer to its first element. Calling through the pointer needs
 * no special syntax.
 */
KOAN(function_names_decay_to_pointers)
{
    int (*op)(int, int) = add;      /* the & is optional and usually omitted */

    KOAN_EQ_INT(/*__*/ 7, op(3, 4));
    KOAN_EQ_INT(/*__*/ 7, (*op)(3, 4));   /* the old spelling; identical */

    op = mul;
    KOAN_EQ_INT(/*__*/ 12, op(3, 4));
}

/*
 * A typedef turns the declaration into something you can read aloud, and is
 * how essentially all real code spells this.
 */
typedef int (*BinaryOp)(int, int);

KOAN(typedefs_make_the_syntax_bearable)
{
    BinaryOp op = add;
    KOAN_EQ_INT(/*__*/ 5, op(2, 3));

    /* An array of them is a dispatch table — a switch you can index. */
    BinaryOp table[] = { add, mul };
    KOAN_EQ_INT(/*__*/ 5, table[0](2, 3));
    KOAN_EQ_INT(/*__*/ 6, table[1](2, 3));
}

/*
 * Passing a function as an argument is what makes an algorithm generic over
 * behaviour. The callee decides *when* to call; the caller decides *what*.
 */
static int reduce(const int *v, size_t n, int seed, BinaryOp op)
{
    int acc = seed;
    for (size_t i = 0; i < n; i++) acc = op(acc, v[i]);
    return acc;
}

KOAN(callbacks_make_algorithms_generic)
{
    int v[] = { 1, 2, 3, 4 };

    KOAN_EQ_INT(/*__*/ 10, reduce(v, 4, 0, add));
    KOAN_EQ_INT(/*__*/ 24, reduce(v, 4, 1, mul));
}

/*
 * qsort is the standard library's sort, and it is generic in the C way: it
 * takes the element size and a comparison function, and moves bytes around.
 *
 * The comparator receives `const void *` and must cast. It returns negative,
 * zero or positive — NOT a bool, and not necessarily -1/0/1.
 */
static int cmp_int_asc(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;

    /* Subtraction would overflow for large values; compare instead. */
    return (x > y) - (x < y);
}

static int cmp_int_desc(const void *a, const void *b)
{
    return cmp_int_asc(b, a);          /* reverse by swapping the arguments */
}

KOAN(qsort_takes_a_comparator)
{
    int v[] = { 5, 2, 9, 1, 7 };
    size_t n = sizeof v / sizeof *v;

    qsort(v, n, sizeof *v, cmp_int_asc);
    KOAN_EQ_INT(/*__*/ 1, v[0]);
    KOAN_EQ_INT(/*__*/ 9, v[4]);

    qsort(v, n, sizeof *v, cmp_int_desc);
    KOAN_EQ_INT(/*__*/ 9, v[0]);
    KOAN_EQ_INT(/*__*/ 1, v[4]);
}

/*
 * Sorting strings needs one more level of indirection. The array's elements
 * are themselves `const char *`, so qsort hands the comparator the address of
 * an element — a `const char *const *`, not a `const char *`. Getting this
 * wrong compiles cleanly and then sorts garbage, because the cast silences
 * the one diagnostic that would have saved you.
 */
static int cmp_str(const void *a, const void *b)
{
    const char *const *x = a;
    const char *const *y = b;
    return strcmp(*x, *y);
}

KOAN(sorting_strings_needs_a_pointer_to_a_pointer)
{
    const char *words[] = { "pear", "apple", "fig" };

    qsort(words, 3, sizeof *words, cmp_str);

    KOAN_EQ_STR(/*__STR*/ "apple", words[0]);
    KOAN_EQ_STR(/*__STR*/ "fig", words[1]);
    KOAN_EQ_STR(/*__STR*/ "pear", words[2]);
}

/*
 * bsearch is qsort's companion and requires the array to be sorted by the
 * *same* comparator. It returns a pointer to the element, or null.
 */
KOAN(bsearch_requires_the_same_ordering)
{
    int v[] = { 1, 3, 5, 7, 9 };
    int key = 7;

    int *found = bsearch(&key, v, 5, sizeof *v, cmp_int_asc);
    KOAN_TRUE(found != nullptr);
    KOAN_EQ_INT(/*__*/ 7, *found);
    KOAN_EQ_SZ(/*__SZ*/ 3, (size_t)(found - v));

    int missing = 4;
    KOAN_TRUE(bsearch(&missing, v, 5, sizeof *v, cmp_int_asc) == nullptr);
}

/*
 * Callbacks usually need context, and C has no closures. The universal
 * solution is a `void *user` parameter that the caller sets and the callback
 * casts back. Every callback API you meet will have one; now you know why.
 */
typedef bool (*Predicate)(int value, void *user);

static bool greater_than(int value, void *user)
{
    int threshold = *(const int *)user;
    return value > threshold;
}

static size_t count_if(const int *v, size_t n, Predicate p, void *user)
{
    size_t hits = 0;
    for (size_t i = 0; i < n; i++)
        if (p(v[i], user)) hits++;
    return hits;
}

KOAN(void_user_data_substitutes_for_closures)
{
    int v[] = { 1, 5, 3, 8, 2 };
    int threshold = 3;

    KOAN_EQ_SZ(/*__SZ*/ 2, count_if(v, 5, greater_than, &threshold));

    threshold = 0;
    KOAN_EQ_SZ(/*__SZ*/ 5, count_if(v, 5, greater_than, &threshold));
}

/*
 * Bringing it together.
 *
 * A vtable: a struct of function pointers that gives several unrelated types
 * a common interface. This is exactly how C++ virtual functions work
 * underneath, and how every C plugin system is built — including the request
 * handlers in the web server at the end of the path.
 */
typedef struct {
    const char *name;
    int  (*area)(const void *shape);
    int  (*perimeter)(const void *shape);
} ShapeOps;

typedef struct { int w, h; } Rect;
typedef struct { int side; }  Square;

static int rect_area(const void *s)      { const Rect *r = s; return r->w * r->h; }
static int rect_perim(const void *s)     { const Rect *r = s; return 2 * (r->w + r->h); }
static int square_area(const void *s)    { const Square *q = s; return q->side * q->side; }
static int square_perim(const void *s)   { const Square *q = s; return 4 * q->side; }

static const ShapeOps RECT_OPS   = { "rect",   rect_area,   rect_perim   };
static const ShapeOps SQUARE_OPS = { "square", square_area, square_perim };

typedef struct {
    const ShapeOps *ops;
    const void     *data;
} Shape;

KOAN(assembling_a_vtable)
{
    Rect   r = { .w = 3, .h = 4 };
    Square q = { .side = 5 };

    Shape shapes[] = {
        { .ops = &RECT_OPS,   .data = &r },
        { .ops = &SQUARE_OPS, .data = &q },
    };

    /* One loop, two types, no switch. */
    int total_area = 0;
    for (size_t i = 0; i < 2; i++)
        total_area += shapes[i].ops->area(shapes[i].data);

    KOAN_EQ_INT(/*__*/ 37, total_area);
    KOAN_EQ_INT(/*__*/ 12, shapes[0].ops->area(shapes[0].data));
    KOAN_EQ_INT(/*__*/ 20, shapes[1].ops->perimeter(shapes[1].data));
    KOAN_EQ_STR(/*__STR*/ "square", shapes[1].ops->name);
}

KOAN_LESSON(lesson_about_function_pointers, "About Function Pointers",
    KOAN_CASE(function_names_decay_to_pointers),
    KOAN_CASE(typedefs_make_the_syntax_bearable),
    KOAN_CASE(callbacks_make_algorithms_generic),
    KOAN_CASE(qsort_takes_a_comparator),
    KOAN_CASE(sorting_strings_needs_a_pointer_to_a_pointer),
    KOAN_CASE(bsearch_requires_the_same_ordering),
    KOAN_CASE(void_user_data_substitutes_for_closures),
    KOAN_CASE(assembling_a_vtable)
);
