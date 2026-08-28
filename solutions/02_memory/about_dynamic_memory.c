/*
 * About Dynamic Memory
 *
 * malloc gives you bytes and a promise to remember nothing. Every allocation
 * raises exactly one question, and the whole discipline of C consists of
 * answering it consistently:
 *
 *     who frees this, and when?
 *
 * "Ownership" is the name for that answer. It is not a language feature; it
 * is a convention you enforce with naming, comments, and care.
 *
 * Run this lesson under `make san` once you have solved it. The sanitizers
 * will show you leaks and use-after-free the moment they happen.
 */
#include "koan.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * malloc returns uninitialised bytes, or null. Reading before writing is
 * undefined behaviour, not "reading zeros" — calloc is what zeroes.
 */
KOAN(malloc_gives_bytes_calloc_gives_zeros)
{
    int *raw = malloc(4 * sizeof *raw);
    KOAN_TRUE(raw != nullptr);

    /* We must write before we read. */
    for (int i = 0; i < 4; i++) raw[i] = i * i;
    KOAN_EQ_INT(/*__*/ 9, raw[3]);
    free(raw);

    int *zeroed = calloc(4, sizeof *zeroed);
    KOAN_TRUE(zeroed != nullptr);
    KOAN_EQ_INT(/*__*/ 0, zeroed[0] + zeroed[1] + zeroed[2] + zeroed[3]);
    free(zeroed);
}

/*
 * Size the allocation from the *object*, not from a repeated type name.
 * `sizeof *p` cannot go stale when p's type changes; `sizeof(int)` can.
 */
KOAN(size_allocations_from_the_pointer)
{
    double *d = malloc(8 * sizeof *d);      /* not sizeof(double) */
    KOAN_TRUE(d != nullptr);
    KOAN_EQ_SZ(/*__SZ*/ 8, sizeof *d);
    free(d);
}

/*
 * `malloc(n * size)` is a trap: if the multiplication overflows, it wraps to
 * a small number, the allocation succeeds, and every subsequent write runs
 * off the end. This is a well-trodden route to a security bug.
 *
 * Check before you multiply. calloc(n, size) performs this check internally,
 * which is a genuine reason to prefer it — but when you must multiply
 * yourself, the guard is this:
 */
static bool product_overflows(size_t n, size_t size)
{
    return size != 0 && n > SIZE_MAX / size;
}

KOAN(guard_the_multiplication_before_allocating)
{
    KOAN_EQ_INT(/*__*/ 0, product_overflows(1000, sizeof(int)));
    KOAN_EQ_INT(/*__*/ 1, product_overflows(SIZE_MAX / 2, SIZE_MAX / 2));

    /* The wrap it prevents: this product is small, and a lie. */
    size_t n = SIZE_MAX / 2 + 1;
    KOAN_EQ_SZ(/*__SZ*/ 0, n * 2);
    KOAN_EQ_INT(/*__*/ 1, product_overflows(n, 2));

    /* Zero is the edge case the guard must not divide by. */
    KOAN_EQ_INT(/*__*/ 0, product_overflows(SIZE_MAX, 0));
}

/*
 * free(nullptr) is defined and does nothing, which removes the need for a
 * guard before every free. Freeing the same pointer twice, on the other hand,
 * is undefined and is a classic exploitable bug.
 */
KOAN(free_null_is_safe_double_free_is_not)
{
    free(nullptr);                          /* no crash, no effect */

    char *p = malloc(16);
    free(p);
    p = nullptr;                            /* the habit that prevents it */

    free(p);                                /* safe only because of the line above */
    KOAN_TRUE(p == nullptr);
}

/*
 * realloc grows or shrinks a block, and may move it. Assigning its result
 * directly over the original pointer loses that pointer if realloc fails —
 * a leak. Use a temporary.
 */
KOAN(realloc_may_move_and_may_fail)
{
    size_t cap = 2;
    int   *v = malloc(cap * sizeof *v);
    v[0] = 1; v[1] = 2;

    size_t new_cap = 8;
    int   *tmp = realloc(v, new_cap * sizeof *v);
    KOAN_TRUE(tmp != nullptr);
    v = tmp;                                 /* only overwrite on success */

    /* The existing contents are preserved up to the smaller of the two sizes. */
    KOAN_EQ_INT(/*__*/ 1, v[0]);
    KOAN_EQ_INT(/*__*/ 2, v[1]);

    v[7] = 8;
    KOAN_EQ_INT(/*__*/ 8, v[7]);
    free(v);

    /* realloc(p, 0) is, as of C23, undefined rather than a disguised free.
     * Say what you mean: call free. */
}

/*
 * A function that allocates must document who frees. The two conventions are
 * "caller frees the returned pointer" and "caller supplies the buffer". Mixing
 * them without saying so is how leaks are born.
 */

/* Ownership: the caller must free the result. */
static char *duplicate(const char *s)
{
    size_t n = strlen(s) + 1;
    char  *copy = malloc(n);
    if (!copy) return nullptr;
    memcpy(copy, s, n);
    return copy;
}

KOAN(allocating_functions_must_state_ownership)
{
    char *copy = duplicate("borrowed");

    KOAN_TRUE(copy != nullptr);
    KOAN_EQ_STR(/*__STR*/ "borrowed", copy);

    /* A genuine copy, not a shared pointer. */
    copy[0] = 'B';
    KOAN_EQ_STR(/*__STR*/ "Borrowed", copy);

    free(copy);      /* our responsibility, because duplicate said so */
}

/*
 * Every allocation must be freed on every path, including the error paths.
 * This is where `goto cleanup` earns its place: one exit, one unwind, no
 * duplicated frees.
 */
typedef struct {
    char *name;
    int  *scores;
} Record;

static bool record_init(Record *r, const char *name, size_t n_scores)
{
    r->name   = nullptr;
    r->scores = nullptr;

    r->name = duplicate(name);
    if (!r->name) goto fail;

    r->scores = calloc(n_scores, sizeof *r->scores);
    if (!r->scores) goto fail;

    return true;

fail:
    free(r->name);
    free(r->scores);
    r->name = nullptr;
    r->scores = nullptr;
    return false;
}

static void record_free(Record *r)
{
    free(r->name);
    free(r->scores);
    r->name   = nullptr;
    r->scores = nullptr;
}

KOAN(error_paths_must_free_too)
{
    Record r;

    KOAN_EQ_INT(/*__*/ 1, record_init(&r, "ada", 3));
    KOAN_EQ_STR(/*__STR*/ "ada", r.name);
    KOAN_EQ_INT(/*__*/ 0, r.scores[2]);

    record_free(&r);

    /* After freeing, the struct is safe to inspect and safe to free again. */
    KOAN_TRUE(r.name == nullptr);
    record_free(&r);
}

/*
 * Bringing it together.
 *
 * A growable array — the data structure you will otherwise rewrite by hand a
 * hundred times. It owns its buffer, doubles its capacity when full, and
 * reports failure rather than crashing.
 *
 * Doubling matters: it makes n appends cost O(n) in total rather than O(n^2).
 */
typedef struct {
    int   *data;
    size_t len;
    size_t cap;
} Vec;

static bool vec_push(Vec *v, int value)
{
    if (v->len == v->cap) {
        size_t next = v->cap ? v->cap * 2 : 4;

        /* Refuse rather than wrap if the multiplication would overflow. */
        if (next > SIZE_MAX / sizeof *v->data) return false;

        int *tmp = realloc(v->data, next * sizeof *v->data);
        if (!tmp) return false;             /* v->data still valid, not leaked */

        v->data = tmp;
        v->cap  = next;
    }
    v->data[v->len++] = value;
    return true;
}

static void vec_free(Vec *v)
{
    free(v->data);
    *v = (Vec){ 0 };
}

KOAN(assembling_a_growable_array)
{
    Vec v = { 0 };

    /* An empty vector owns nothing, so it needs no special-casing. */
    KOAN_EQ_SZ(/*__SZ*/ 0, v.len);
    KOAN_TRUE(v.data == nullptr);

    for (int i = 0; i < 10; i++) KOAN_TRUE(vec_push(&v, i * i));

    KOAN_EQ_SZ(/*__SZ*/ 10, v.len);
    KOAN_EQ_INT(/*__*/ 81, v.data[9]);

    /* Capacity doubled from 4: 4, 8, 16. It is >= len, and a power of two. */
    KOAN_EQ_SZ(/*__SZ*/ 16, v.cap);
    KOAN_TRUE(v.cap >= v.len);

    vec_free(&v);
    KOAN_EQ_SZ(/*__SZ*/ 0, v.cap);
    KOAN_TRUE(v.data == nullptr);
}

KOAN_LESSON(lesson_about_dynamic_memory, "About Dynamic Memory",
    KOAN_CASE(malloc_gives_bytes_calloc_gives_zeros),
    KOAN_CASE(size_allocations_from_the_pointer),
    KOAN_CASE(guard_the_multiplication_before_allocating),
    KOAN_CASE(free_null_is_safe_double_free_is_not),
    KOAN_CASE(realloc_may_move_and_may_fail),
    KOAN_CASE(allocating_functions_must_state_ownership),
    KOAN_CASE(error_paths_must_free_too),
    KOAN_CASE(assembling_a_growable_array)
);
