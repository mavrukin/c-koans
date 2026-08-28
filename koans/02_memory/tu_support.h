/*
 * tu_support.h — the interface to tu_support.c.
 *
 * A header is a promise, not a definition. It declares what exists elsewhere
 * so that each translation unit can be compiled without seeing the others.
 */
#ifndef TU_SUPPORT_H          /* the include guard: this file may be read
                               * many times, but only take effect once */
#define TU_SUPPORT_H

/*
 * `extern` on a variable declaration says "this exists, defined elsewhere".
 * Without it, a header included by two files would define the object twice
 * and the link would fail.
 */
extern int tu_shared_counter;

const char *tu_describe(void);
int         tu_bump_shared(void);
int         tu_read_private(void);
int         tu_next_ticket(void);

/*
 * `inline` lets the definition live in the header so it can be substituted at
 * each call site. Exactly one translation unit must then provide an
 * `extern inline` declaration to emit the callable copy — tu_support.c does.
 */
inline int tu_double(int n) { return n * 2; }

#endif /* TU_SUPPORT_H */
