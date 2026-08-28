/*
 * About Alignment, restrict and volatile
 *
 * Three qualifiers that describe not what a value *is* but how the machine may
 * touch it. Each exists because the compiler cannot work it out alone:
 *
 *   alignas/alignof  where an object may sit in memory
 *   restrict         a promise that two pointers do not overlap
 *   volatile         a warning that memory may change behind your back
 *
 * Misusing `restrict` is undefined behaviour with no diagnostic. Misusing
 * `volatile` merely makes things slow. Know which is which.
 */
#include "koan.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Every type has an alignment: the addresses at which it may legally live.
 * It is always a power of two, and never larger than the type's size.
 */
KOAN(every_type_has_an_alignment)
{
    KOAN_EQ_SZ(/*__SZ*/ 1, alignof(char));
    KOAN_EQ_SZ(/*__SZ*/ 4, alignof(int32_t));
    KOAN_EQ_SZ(/*__SZ*/ 8, alignof(double));

    /* A struct's alignment is the strictest of its members. */
    struct Mixed { char c; double d; };
    KOAN_EQ_SZ(/*__SZ*/ 8, alignof(struct Mixed));

    /* Which, with padding, is why its size is a multiple of that alignment. */
    KOAN_EQ_SZ(/*__SZ*/ 16, sizeof(struct Mixed));
    KOAN_EQ_SZ(/*__SZ*/ 0, sizeof(struct Mixed) % alignof(struct Mixed));
}

/*
 * `alignas` raises an object's alignment. The usual reasons are cache lines
 * (64 bytes) and SIMD; the usual mistake is assuming malloc gives you it.
 */
KOAN(alignas_raises_alignment)
{
    alignas(64) static unsigned char cache_line[64];
    alignas(16) static int           vector[4];

    KOAN_EQ_SZ(/*__SZ*/ 0, (uintptr_t)cache_line % 64);
    KOAN_EQ_SZ(/*__SZ*/ 0, (uintptr_t)vector % 16);

    /* Note that standard alignof takes a *type*, not an expression. There is
     * no portable way to ask an alignas-declared object for its alignment;
     * checking the address, as above, is the honest test. */
    KOAN_EQ_SZ(/*__SZ*/ 1, alignof(unsigned char));
}

/*
 * malloc returns memory suitable for any type with fundamental alignment —
 * but not for an over-aligned one. C11 added aligned_alloc for that, and its
 * size must be a multiple of the alignment.
 */
KOAN(malloc_guarantees_only_fundamental_alignment)
{
    void *ordinary = malloc(64);
    KOAN_TRUE(ordinary != nullptr);
    KOAN_EQ_SZ(/*__SZ*/ 0, (uintptr_t)ordinary % alignof(max_align_t));
    free(ordinary);

    void *wide = aligned_alloc(64, 128);    /* size must be a multiple of 64 */
    KOAN_TRUE(wide != nullptr);
    KOAN_EQ_SZ(/*__SZ*/ 0, (uintptr_t)wide % 64);
    free(wide);
}

/*
 * Rounding an address or size up to an alignment is the arithmetic every
 * allocator needs. Because alignments are powers of two, it is done with a
 * mask rather than a division.
 */
static size_t align_up(size_t n, size_t to)
{
    return (n + to - 1) & ~(to - 1);
}

KOAN(rounding_up_to_an_alignment)
{
    KOAN_EQ_SZ(/*__SZ*/ 8, align_up(1, 8));
    KOAN_EQ_SZ(/*__SZ*/ 8, align_up(8, 8));
    KOAN_EQ_SZ(/*__SZ*/ 16, align_up(9, 8));
    KOAN_EQ_SZ(/*__SZ*/ 0, align_up(0, 8));

    /* The same expression, written the slow way, agrees. */
    KOAN_EQ_SZ(/*__SZ*/ 16, ((9 + 8 - 1) / 8) * 8);
}

/*
 * `restrict` promises that, for the lifetime of the pointer, the object it
 * points at is reached only through it. That licenses the compiler to keep
 * values in registers across writes.
 *
 * The promise is yours to keep. Break it and the result is undefined — the
 * koan below shows the aliasing case you must not pass to such a function.
 */
static void add_arrays(int *restrict out, const int *restrict a,
                       const int *restrict b, size_t n)
{
    for (size_t i = 0; i < n; i++) out[i] = a[i] + b[i];
}

KOAN(restrict_promises_no_overlap)
{
    int a[4] = { 1, 2, 3, 4 };
    int b[4] = { 10, 20, 30, 40 };
    int out[4] = {};

    add_arrays(out, a, b, 4);
    KOAN_EQ_INT(/*__*/ 11, out[0]);
    KOAN_EQ_INT(/*__*/ 44, out[3]);

    /* memcpy is declared with restrict; memmove is not. That difference IS
     * the difference between them, and it is why overlapping copies must
     * use memmove. */
    char overlap[] = "abcdef";
    memmove(overlap + 1, overlap, 5);
    overlap[6] = '\0';
    KOAN_EQ_STR(/*__STR*/ "aabcde", overlap);
}

/*
 * `volatile` tells the compiler that an object may change by means it cannot
 * see — hardware, another thread's signal handler — so every read must
 * actually happen and may not be cached or reordered away.
 *
 * It is NOT a synchronisation primitive. It gives no atomicity and no memory
 * ordering; for threads you want <stdatomic.h>. Its two correct uses are
 * memory-mapped hardware and sig_atomic_t flags.
 */
KOAN(volatile_forbids_caching_a_read)
{
    volatile int sensor = 1;

    /* Each read is a genuine load, so the compiler may not fold these. */
    int first  = sensor;
    sensor = 2;
    int second = sensor;

    KOAN_EQ_INT(/*__*/ 1, first);
    KOAN_EQ_INT(/*__*/ 2, second);

    /* volatile changes the type, so it participates in _Generic selection
     * and in const-correctness like any other qualifier. */
    KOAN_EQ_SZ(/*__SZ*/ sizeof(int), sizeof(volatile int));
}

/*
 * Bringing it together.
 *
 * A bump allocator: hand out correctly aligned slices of one buffer, and
 * refuse rather than overrun. Every real arena starts exactly like this, and
 * the whole difficulty is the alignment arithmetic.
 *
 * You will build a fuller version of this in the arena capstone.
 */
typedef struct {
    unsigned char *base;
    size_t         cap;
    size_t         used;
} Bump;

static void bump_init(Bump *b, void *storage, size_t cap)
{
    b->base = storage;
    b->cap  = cap;
    b->used = 0;
}

static void *bump_alloc(Bump *b, size_t size, size_t align)
{
    /* An alignment must be a power of two, or the mask arithmetic is wrong. */
    if (align == 0 || (align & (align - 1)) != 0) return nullptr;

    size_t offset = align_up(b->used, align);
    if (offset > b->cap || size > b->cap - offset) return nullptr;  /* no wrap */

    void *p = b->base + offset;
    b->used = offset + size;
    return p;
}

#define BUMP_NEW(b, type) ((type *)bump_alloc((b), sizeof(type), alignof(type)))

KOAN(assembling_a_bump_allocator)
{
    alignas(max_align_t) static unsigned char storage[128];
    Bump b;
    bump_init(&b, storage, sizeof storage);

    /* One byte, then a double: the double must skip forward to alignment. */
    char *c = bump_alloc(&b, 1, 1);
    KOAN_EQ_SZ(/*__SZ*/ 1, b.used);

    double *d = BUMP_NEW(&b, double);
    KOAN_TRUE(d != nullptr);
    KOAN_EQ_SZ(/*__SZ*/ 0, (uintptr_t)d % alignof(double));

    /* Seven bytes of padding were skipped, then eight bytes taken. */
    KOAN_EQ_SZ(/*__SZ*/ 16, b.used);

    *d = 1.5;
    *c = 'x';
    KOAN_EQ_DBL(/*__DBL*/ 1.5, *d, 1e-12);
    KOAN_EQ_CHR(/*__CHR*/ 'x', *c);

    /* An over-large request is refused, and leaves the arena untouched. */
    KOAN_TRUE(bump_alloc(&b, 1000, 8) == nullptr);
    KOAN_EQ_SZ(/*__SZ*/ 16, b.used);

    /* So is a nonsensical alignment. */
    KOAN_TRUE(bump_alloc(&b, 4, 3) == nullptr);

    /* The remaining space is exactly what is left. */
    KOAN_EQ_SZ(/*__SZ*/ 112, b.cap - b.used);
}

KOAN_LESSON(lesson_about_alignment, "About Alignment, restrict and volatile",
    KOAN_CASE(every_type_has_an_alignment),
    KOAN_CASE(alignas_raises_alignment),
    KOAN_CASE(malloc_guarantees_only_fundamental_alignment),
    KOAN_CASE(rounding_up_to_an_alignment),
    KOAN_CASE(restrict_promises_no_overlap),
    KOAN_CASE(volatile_forbids_caching_a_read),
    KOAN_CASE(assembling_a_bump_allocator)
);
