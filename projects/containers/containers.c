/*
 * containers.c — implementation.
 *
 * The Vec is the growable array from about_dynamic_memory, generalised over
 * element size. The Map is open addressing with linear probing and tombstones
 * — the simplest hash table that handles deletion correctly.
 */
#include "containers.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Vec                                                                 */
/* ------------------------------------------------------------------ */

void vec_init(Vec *v, size_t elem_size)
{
    *v = (Vec){ .elem_size = elem_size ? elem_size : 1 };
}

void vec_free(Vec *v)
{
    free(v->data);
    size_t elem = v->elem_size;
    *v = (Vec){ .elem_size = elem };
}

bool vec_reserve(Vec *v, size_t want)
{
    if (want <= v->cap) return true;

    size_t next = v->cap ? v->cap : 4;
    while (next < want) {
        if (next > SIZE_MAX / 2) return false;      /* would overflow */
        next *= 2;
    }
    if (next > SIZE_MAX / v->elem_size) return false;

    unsigned char *grown = realloc(v->data, next * v->elem_size);
    if (!grown) return false;                       /* v->data still valid */

    v->data = grown;
    v->cap  = next;
    return true;
}

bool vec_push(Vec *v, const void *elem)
{
    if (v->len == v->cap && !vec_reserve(v, v->len + 1)) return false;
    memcpy(v->data + v->len * v->elem_size, elem, v->elem_size);
    v->len++;
    return true;
}

bool vec_pop(Vec *v, void *out)
{
    if (v->len == 0) return false;
    v->len--;
    if (out) memcpy(out, v->data + v->len * v->elem_size, v->elem_size);
    return true;
}

void *vec_at(const Vec *v, size_t i)
{
    return i < v->len ? v->data + i * v->elem_size : nullptr;
}

void vec_clear(Vec *v) { v->len = 0; }

void vec_sort(Vec *v, int (*cmp)(const void *, const void *))
{
    if (v->len > 1) qsort(v->data, v->len, v->elem_size, cmp);
}

size_t vec_find(const Vec *v, bool (*pred)(const void *, void *), void *user)
{
    for (size_t i = 0; i < v->len; i++)
        if (pred(v->data + i * v->elem_size, user)) return i;
    return SIZE_MAX;
}

/* ------------------------------------------------------------------ */
/* Map                                                                 */
/* ------------------------------------------------------------------ */

constexpr size_t MAP_INITIAL_CAP = 16;

/*
 * A deleted slot cannot simply be emptied: a probe for a later key may have
 * walked past it, and emptying it would end that probe early. The standard
 * fix is a tombstone — occupied for probing, free for insertion.
 */
static char TOMBSTONE[] = "";
#define IS_TOMBSTONE(e) ((e)->key == TOMBSTONE)
#define IS_LIVE(e)      ((e)->key != nullptr && (e)->key != TOMBSTONE)

/* FNV-1a: short, fast, good enough for a hash table that is not adversarial. */
static size_t hash_string(const char *s)
{
    size_t h = 1469598103934665603ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 1099511628211ULL;
    }
    return h;
}

void map_init(Map *m) { *m = (Map){ 0 }; }

void map_free(Map *m)
{
    if (m->slots) {
        for (size_t i = 0; i < m->cap; i++)
            if (IS_LIVE(&m->slots[i])) free(m->slots[i].key);
        free(m->slots);
    }
    *m = (Map){ 0 };
}

/* Find the slot for `key`, or the first place it could be inserted. */
static MapEntry *find_slot(MapEntry *slots, size_t cap,
                           const char *key, size_t hash)
{
    size_t    mask = cap - 1;            /* cap is a power of two */
    size_t    i = hash & mask;
    MapEntry *first_free = nullptr;

    for (size_t probe = 0; probe < cap; probe++) {
        MapEntry *e = &slots[i];

        if (e->key == nullptr)
            return first_free ? first_free : e;   /* a true empty ends the probe */

        if (IS_TOMBSTONE(e)) {
            if (!first_free) first_free = e;      /* reusable, but keep looking */
        } else if (e->hash == hash && strcmp(e->key, key) == 0) {
            return e;                             /* found it */
        }

        i = (i + 1) & mask;                       /* linear probing */
    }
    return first_free;
}

static bool grow(Map *m)
{
    size_t next = m->cap ? m->cap * 2 : MAP_INITIAL_CAP;
    if (next > SIZE_MAX / sizeof(MapEntry)) return false;

    MapEntry *slots = calloc(next, sizeof *slots);
    if (!slots) return false;

    /* Rehash: positions depend on the capacity, so nothing can be copied. */
    for (size_t i = 0; i < m->cap; i++) {
        MapEntry *e = &m->slots[i];
        if (!IS_LIVE(e)) continue;
        MapEntry *dst = find_slot(slots, next, e->key, e->hash);
        *dst = *e;
    }

    free(m->slots);
    m->slots = slots;
    m->cap   = next;
    return true;
}

bool map_put(Map *m, const char *key, void *value)
{
    /* Grow past 70% load: linear probing degrades badly when it gets full. */
    if (m->cap == 0 || (m->len + 1) * 10 >= m->cap * 7)
        if (!grow(m)) return false;

    size_t    h = hash_string(key);
    MapEntry *e = find_slot(m->slots, m->cap, key, h);
    if (!e) return false;

    if (IS_LIVE(e)) {                 /* replacing an existing value */
        e->value = value;
        return true;
    }

    char *owned = malloc(strlen(key) + 1);
    if (!owned) return false;
    memcpy(owned, key, strlen(key) + 1);

    e->key   = owned;
    e->value = value;
    e->hash  = h;
    m->len++;
    return true;
}

void *map_get(const Map *m, const char *key)
{
    if (m->cap == 0) return nullptr;

    size_t    h = hash_string(key);
    MapEntry *e = find_slot(m->slots, m->cap, key, h);
    return (e && IS_LIVE(e)) ? e->value : nullptr;
}

bool map_has(const Map *m, const char *key)
{
    if (m->cap == 0) return false;
    size_t    h = hash_string(key);
    MapEntry *e = find_slot(m->slots, m->cap, key, h);
    return e && IS_LIVE(e);
}

bool map_remove(Map *m, const char *key)
{
    if (m->cap == 0) return false;

    size_t    h = hash_string(key);
    MapEntry *e = find_slot(m->slots, m->cap, key, h);
    if (!e || !IS_LIVE(e)) return false;

    free(e->key);
    e->key   = TOMBSTONE;      /* not nullptr — see the note above */
    e->value = nullptr;
    m->len--;
    return true;
}

void map_each(const Map *m,
              bool (*visit)(const char *, void *, void *), void *user)
{
    for (size_t i = 0; i < m->cap; i++) {
        MapEntry *e = &m->slots[i];
        if (IS_LIVE(e) && !visit(e->key, e->value, user)) return;
    }
}
