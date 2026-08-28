/*
 * arena.h — a region allocator.
 *
 * An arena hands out memory from large blocks and frees everything at once.
 * That single decision removes the hardest problem in C: there is no
 * per-object free, so there is nothing to double-free, nothing to leak, and
 * no ownership question to get wrong. You free the arena, or you reset it.
 *
 * It is the right structure whenever allocations share a lifetime — one
 * request, one frame, one parse. The JSON parser next door uses one, and so
 * could the web server.
 *
 * The trade: you cannot free one object. If you need that, you need malloc.
 *
 * Techniques from: about_alignment (the arithmetic), about_dynamic_memory
 * (ownership), about_multidim_arrays (flexible array members).
 */
#ifndef KOAN_ARENA_H
#define KOAN_ARENA_H

#include <stddef.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct {
    ArenaBlock *head;        /* newest block; older ones chain behind it */
    size_t      block_size;  /* size of a fresh block */
    size_t      total_bytes; /* reserved across every block */
    size_t      used_bytes;  /* handed out, including alignment padding */
    size_t      block_count;
} Arena;

/*
 * `block_size` is a hint: an allocation larger than it gets its own block.
 * Passing 0 selects a sensible default.
 */
void arena_init(Arena *a, size_t block_size);

/* Aligned allocation. Returns null only if the system is out of memory. */
void *arena_alloc(Arena *a, size_t size, size_t align);

/* Zeroed, with the multiplication overflow-checked. */
void *arena_calloc(Arena *a, size_t count, size_t size);

/* Copy a string into the arena. The result lives as long as the arena. */
char *arena_strdup(Arena *a, const char *s);

/* printf into the arena, sized exactly. */
char *arena_printf(Arena *a, const char *fmt, ...);

/*
 * Keep the memory, forget the contents: the next allocation starts over.
 * This is what makes an arena cheap in a loop — no free, no malloc.
 */
void arena_reset(Arena *a);

/* Release every block. The arena is reusable afterwards. */
void arena_free(Arena *a);

/* Allocate one object of a type, correctly aligned, without repeating it. */
#define ARENA_NEW(a, type)      ((type *)arena_alloc((a), sizeof(type), alignof(type)))
#define ARENA_ARRAY(a, type, n) ((type *)arena_alloc((a), sizeof(type) * (n), alignof(type)))

#endif /* KOAN_ARENA_H */
