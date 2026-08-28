/*
 * arena.c — implementation.
 *
 * The whole allocator is a pointer that moves forward. Everything else is
 * bookkeeping for when it runs off the end of a block.
 */
#include "arena.h"

#include <stdalign.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t DEFAULT_BLOCK = 64 * 1024;

/*
 * A block is a header followed by its bytes, in one allocation — the flexible
 * array member from about_multidim_arrays. One malloc per block, not two.
 */
struct ArenaBlock {
    ArenaBlock   *prev;
    size_t        cap;
    size_t        used;
    alignas(max_align_t) unsigned char data[];
};

static size_t align_up(size_t n, size_t to)
{
    return (n + to - 1) & ~(to - 1);
}

void arena_init(Arena *a, size_t block_size)
{
    *a = (Arena){ .block_size = block_size ? block_size : DEFAULT_BLOCK };
}

static ArenaBlock *new_block(Arena *a, size_t need)
{
    size_t cap = a->block_size > need ? a->block_size : need;

    /* Refuse rather than wrap (about_dynamic_memory). */
    if (cap > SIZE_MAX - sizeof(ArenaBlock)) return nullptr;

    ArenaBlock *b = malloc(sizeof *b + cap);
    if (!b) return nullptr;

    b->prev = a->head;
    b->cap  = cap;
    b->used = 0;

    a->head = b;
    a->total_bytes += cap;
    a->block_count++;
    return b;
}

void *arena_alloc(Arena *a, size_t size, size_t align)
{
    if (align == 0 || (align & (align - 1)) != 0) return nullptr;  /* not a power of two */
    if (a->block_size == 0) arena_init(a, 0);                      /* used uninitialised */

    if (a->head) {
        size_t offset = align_up(a->head->used, align);
        if (offset <= a->head->cap && size <= a->head->cap - offset) {
            void *p = a->head->data + offset;
            a->used_bytes += (offset - a->head->used) + size;
            a->head->used = offset + size;
            return p;
        }
    }

    /* The current block is full. Take a new one sized for this request. */
    if (size > SIZE_MAX - align) return nullptr;
    ArenaBlock *b = new_block(a, size + align);
    if (!b) return nullptr;

    size_t offset = align_up(0, align);
    void  *p = b->data + offset;
    b->used = offset + size;
    a->used_bytes += b->used;
    return p;
}

void *arena_calloc(Arena *a, size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) return nullptr;

    size_t total = count * size;
    void  *p = arena_alloc(a, total, alignof(max_align_t));
    if (p) memset(p, 0, total);
    return p;
}

char *arena_strdup(Arena *a, const char *s)
{
    size_t n = strlen(s) + 1;
    char  *copy = arena_alloc(a, n, 1);
    if (copy) memcpy(copy, s, n);
    return copy;
}

char *arena_printf(Arena *a, const char *fmt, ...)
{
    /* Measure, then format — va_copy, from about_variadic. */
    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);

    int needed = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    char *out = nullptr;
    if (needed >= 0) {
        out = arena_alloc(a, (size_t)needed + 1, 1);
        if (out) vsnprintf(out, (size_t)needed + 1, fmt, copy);
    }
    va_end(copy);
    return out;
}

void arena_reset(Arena *a)
{
    /*
     * Keep one block and rewind it; release the rest. Keeping everything
     * would let an arena that is reset in a loop grow without bound, because
     * allocation only ever draws from the newest block.
     */
    ArenaBlock *keep = a->head;
    if (keep) {
        for (ArenaBlock *b = keep->prev; b; ) {
            ArenaBlock *prev = b->prev;
            a->total_bytes -= b->cap;
            a->block_count--;
            free(b);
            b = prev;
        }
        keep->prev = nullptr;
        keep->used = 0;
    }
    a->used_bytes = 0;
}

void arena_free(Arena *a)
{
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *prev = b->prev;
        free(b);
        b = prev;
    }
    size_t block_size = a->block_size;
    *a = (Arena){ .block_size = block_size };
}
