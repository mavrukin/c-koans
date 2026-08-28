/*
 * compat.h — small, honest bridges over the gaps between what ISO C23 says
 * and what real libc implementations ship today (2026).
 *
 * C23 is fully implemented at the *language* level by GCC 14+ and Clang 18+.
 * The *library* is a different story, and the gaps differ per platform. The
 * Apple SDK, for example, ships neither <stdbit.h> nor <threads.h>.
 *
 * Rather than pretend those koans do not exist, this header detects what is
 * present and supplies a conforming fallback where it is not. Each fallback
 * is written the way you would write it yourself, so it is worth reading.
 *
 * Every symbol here is a genuine C23 library feature. Nothing is invented.
 */
#ifndef KOAN_COMPAT_H
#define KOAN_COMPAT_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Feature detection                                                   */
/* ------------------------------------------------------------------ */

#if defined(__has_include)
#  if __has_include(<stdbit.h>)
#    define KOAN_HAS_STDBIT 1
#  endif
#  if __has_include(<threads.h>) && !defined(__STDC_NO_THREADS__)
#    define KOAN_HAS_THREADS_H 1
#  endif
#  if __has_include(<stdckdint.h>)
#    define KOAN_HAS_CKDINT 1
#  endif
#  if __has_include(<uchar.h>)
#    define KOAN_HAS_UCHAR 1
#  endif
#endif

#ifndef KOAN_HAS_STDBIT
#  define KOAN_HAS_STDBIT 0
#endif
#ifndef KOAN_HAS_THREADS_H
#  define KOAN_HAS_THREADS_H 0
#endif
#ifndef KOAN_HAS_CKDINT
#  define KOAN_HAS_CKDINT 0
#endif
#ifndef KOAN_HAS_UCHAR
#  define KOAN_HAS_UCHAR 0
#endif

/* ------------------------------------------------------------------ */
/* <stdbit.h> (C23 §7.18) — bit utilities                              */
/* ------------------------------------------------------------------ */

#if KOAN_HAS_STDBIT
#  include <stdbit.h>
#else

/*
 * A subset of <stdbit.h> for the unsigned types the koans use. The real
 * header defines a full family (uc/us/ui/ul/ull) plus type-generic macros.
 * These implementations are the straightforward ones; the point of the koan
 * is to understand the operation, not to micro-optimise it.
 */

static inline unsigned koan_stdc_leading_zeros_ui(unsigned x)
{
    unsigned n = 0;
    for (int i = (int)(sizeof(unsigned) * CHAR_BIT) - 1; i >= 0; i--) {
        if (x & (1u << i)) break;
        n++;
    }
    return n;
}

static inline unsigned koan_stdc_trailing_zeros_ui(unsigned x)
{
    if (x == 0) return (unsigned)(sizeof(unsigned) * CHAR_BIT);
    unsigned n = 0;
    while (!(x & 1u)) { x >>= 1; n++; }
    return n;
}

static inline unsigned koan_stdc_count_ones_ui(unsigned x)
{
    unsigned n = 0;
    while (x) { n += x & 1u; x >>= 1; }
    return n;
}

static inline bool koan_stdc_has_single_bit_ui(unsigned x)
{
    return x != 0 && (x & (x - 1u)) == 0;
}

static inline unsigned koan_stdc_bit_width_ui(unsigned x)
{
    unsigned n = 0;
    while (x) { x >>= 1; n++; }
    return n;
}

static inline unsigned koan_stdc_bit_floor_ui(unsigned x)
{
    if (x == 0) return 0;
    return 1u << (koan_stdc_bit_width_ui(x) - 1u);
}

static inline unsigned koan_stdc_bit_ceil_ui(unsigned x)
{
    if (x <= 1) return 1;
    return 1u << koan_stdc_bit_width_ui(x - 1u);
}

#  define stdc_leading_zeros(x)   koan_stdc_leading_zeros_ui((unsigned)(x))
#  define stdc_trailing_zeros(x)  koan_stdc_trailing_zeros_ui((unsigned)(x))
#  define stdc_count_ones(x)      koan_stdc_count_ones_ui((unsigned)(x))
#  define stdc_has_single_bit(x)  koan_stdc_has_single_bit_ui((unsigned)(x))
#  define stdc_bit_width(x)       koan_stdc_bit_width_ui((unsigned)(x))
#  define stdc_bit_floor(x)       koan_stdc_bit_floor_ui((unsigned)(x))
#  define stdc_bit_ceil(x)        koan_stdc_bit_ceil_ui((unsigned)(x))

#endif /* KOAN_HAS_STDBIT */

/* ------------------------------------------------------------------ */
/* <threads.h> (C23 §7.28) — mapped onto pthreads where absent         */
/* ------------------------------------------------------------------ */

#if KOAN_HAS_THREADS_H
#  include <threads.h>
#else
#  include <errno.h>
#  include <pthread.h>

/*
 * C11/C23 threads are an *optional* part of the standard: an implementation
 * may define __STDC_NO_THREADS__ and omit them entirely, which is exactly
 * what Apple's libc does. The API is a thin renaming of pthreads, so we can
 * supply it faithfully. Reading this shim tells you most of what you need to
 * know about how the two APIs correspond.
 */

enum {
    thrd_success  = 0,
    thrd_busy     = 1,
    thrd_error    = 2,
    thrd_nomem    = 3,
    thrd_timedout = 4
};

enum { mtx_plain = 0, mtx_recursive = 1, mtx_timed = 2 };

typedef pthread_t       thrd_t;
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t  cnd_t;
typedef int (*thrd_start_t)(void *);

/* pthreads returns void*, C threads returns int. Bridge the two. */
typedef struct {
    thrd_start_t fn;
    void        *arg;
} koan_thrd_shim;

static inline void *koan_thrd_trampoline(void *raw)
{
    koan_thrd_shim *s = raw;
    thrd_start_t fn = s->fn;
    void *arg = s->arg;
    free(s);
    return (void *)(intptr_t)fn(arg);
}

static inline int thrd_create(thrd_t *t, thrd_start_t fn, void *arg)
{
    koan_thrd_shim *s = malloc(sizeof *s);
    if (!s) return thrd_nomem;
    s->fn = fn;
    s->arg = arg;
    if (pthread_create(t, nullptr, koan_thrd_trampoline, s) != 0) {
        free(s);
        return thrd_error;
    }
    return thrd_success;
}

static inline int thrd_join(thrd_t t, int *res)
{
    void *ret = nullptr;
    if (pthread_join(t, &ret) != 0) return thrd_error;
    if (res) *res = (int)(intptr_t)ret;
    return thrd_success;
}

static inline int thrd_detach(thrd_t t)
{
    return pthread_detach(t) == 0 ? thrd_success : thrd_error;
}

static inline thrd_t thrd_current(void) { return pthread_self(); }

static inline int thrd_equal(thrd_t a, thrd_t b)
{
    return pthread_equal(a, b);
}

static inline int mtx_init(mtx_t *m, int type)
{
    if (type & mtx_recursive) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        int rc = pthread_mutex_init(m, &attr);
        pthread_mutexattr_destroy(&attr);
        return rc == 0 ? thrd_success : thrd_error;
    }
    return pthread_mutex_init(m, nullptr) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_lock(mtx_t *m)
{
    return pthread_mutex_lock(m) == 0 ? thrd_success : thrd_error;
}

static inline int mtx_trylock(mtx_t *m)
{
    int rc = pthread_mutex_trylock(m);
    if (rc == 0)     return thrd_success;
    if (rc == EBUSY) return thrd_busy;
    return thrd_error;
}

static inline int mtx_unlock(mtx_t *m)
{
    return pthread_mutex_unlock(m) == 0 ? thrd_success : thrd_error;
}

static inline void mtx_destroy(mtx_t *m) { pthread_mutex_destroy(m); }

static inline int cnd_init(cnd_t *c)
{
    return pthread_cond_init(c, nullptr) == 0 ? thrd_success : thrd_error;
}

static inline int cnd_wait(cnd_t *c, mtx_t *m)
{
    return pthread_cond_wait(c, m) == 0 ? thrd_success : thrd_error;
}

static inline int cnd_signal(cnd_t *c)
{
    return pthread_cond_signal(c) == 0 ? thrd_success : thrd_error;
}

static inline int cnd_broadcast(cnd_t *c)
{
    return pthread_cond_broadcast(c) == 0 ? thrd_success : thrd_error;
}

static inline void cnd_destroy(cnd_t *c) { pthread_cond_destroy(c); }

#endif /* KOAN_HAS_THREADS_H */

/* ------------------------------------------------------------------ */
/* <uchar.h> (C23 §7.30) — Unicode character types                     */
/* ------------------------------------------------------------------ */

#if KOAN_HAS_UCHAR
#  include <uchar.h>
#else
/*
 * The Apple SDK does not ship <uchar.h>. The types it declares are defined by
 * the standard in terms of types we already have, so declaring them here is
 * exact rather than approximate:
 *
 *   char8_t   is unsigned char          (C23 7.30)
 *   char16_t  is uint_least16_t
 *   char32_t  is uint_least32_t
 *
 * These match the types the compiler already gives u8"", u"" and U"" literals,
 * so the literals and the typedefs agree.
 */
typedef unsigned char  char8_t;
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif

/* ------------------------------------------------------------------ */
/* printf("%b") (C23 §7.23.6.1) — binary conversion                    */
/* ------------------------------------------------------------------ */

/*
 * C23 added the %b conversion specifier for binary output. Several libcs,
 * including Apple's, do not implement it yet: printf("%b", 10) prints a bare
 * "b". Since a koan about %b that silently prints the wrong thing teaches the
 * wrong lesson, format binary explicitly instead.
 *
 * Writes an unsigned value in base 2 into `out` (which must hold at least
 * 65 bytes) and returns it.
 */
static inline char *koan_to_binary(char *out, size_t cap, unsigned long long v)
{
    char tmp[65];
    size_t n = 0;

    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = (char)('0' + (v & 1u)); v >>= 1; }

    size_t written = 0;
    while (n > 0 && written + 1 < cap) out[written++] = tmp[--n];
    out[written] = '\0';
    return out;
}

/* True when this libc actually implements %b, discovered at runtime. */
static inline bool koan_libc_has_binary_printf(void)
{
    char buf[8];
    int n = snprintf(buf, sizeof buf, "%b", 5u);
    return n == 3 && strcmp(buf, "101") == 0;
}

#endif /* KOAN_COMPAT_H */
