/*
 * About Atomics and the Memory Model
 *
 * about_threads used atomics as a faster counter. This lesson is about what
 * they actually guarantee, which is two separate things people conflate:
 *
 *   atomicity  the operation happens all at once; no thread sees it half done
 *   ordering   what else is guaranteed visible when this operation is
 *
 * The second is the hard one, and it is why `volatile` is not a substitute:
 * volatile gives neither.
 *
 * Read the C11/C23 memory model as a contract about *reordering*. Compilers
 * and CPUs both reorder memory operations; the model says how much you may
 * rely on them not to.
 */
#include "koan.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * An atomic object is read and written indivisibly. Initialise with
 * atomic_init or ATOMIC_VAR_INIT-style assignment before it is shared.
 */
KOAN(atomic_load_and_store_are_indivisible)
{
    atomic_int counter;
    atomic_init(&counter, 10);

    KOAN_EQ_INT(/*__*/ 10, atomic_load(&counter));

    atomic_store(&counter, 20);
    KOAN_EQ_INT(/*__*/ 20, atomic_load(&counter));

    /* The plain operators work too, and are atomic on an atomic type. */
    counter += 5;
    KOAN_EQ_INT(/*__*/ 25, counter);
}

/*
 * The read-modify-write operations return the value *before* the change.
 * That return is what makes them useful: it is how a thread learns whether it
 * was the one that did something.
 */
KOAN(fetch_operations_return_the_previous_value)
{
    atomic_int n;
    atomic_init(&n, 100);

    KOAN_EQ_INT(/*__*/ 100, atomic_fetch_add(&n, 5));
    KOAN_EQ_INT(/*__*/ 105, atomic_load(&n));

    KOAN_EQ_INT(/*__*/ 105, atomic_fetch_sub(&n, 5));
    KOAN_EQ_INT(/*__*/ 100, atomic_load(&n));

    atomic_int flags;
    atomic_init(&flags, 0b1010);
    KOAN_EQ_INT(/*__*/ 0b1010, atomic_fetch_or(&flags, 0b0101));
    KOAN_EQ_INT(/*__*/ 0b1111, atomic_load(&flags));

    KOAN_EQ_INT(/*__*/ 0b1111, atomic_fetch_and(&flags, 0b0011));
    KOAN_EQ_INT(/*__*/ 0b0011, atomic_load(&flags));

    /* atomic_exchange sets a new value and hands back the old one. */
    KOAN_EQ_INT(/*__*/ 0b0011, atomic_exchange(&flags, 0));
    KOAN_EQ_INT(/*__*/ 0, atomic_load(&flags));
}

/*
 * Compare-and-swap is the primitive everything else is built from: write only
 * if the value is still what you last saw. On failure it updates `expected`
 * with what it actually found, which is what makes a retry loop work.
 */
KOAN(compare_exchange_reports_what_it_found)
{
    atomic_int value;
    atomic_init(&value, 10);

    int expected = 10;
    KOAN_EQ_INT(/*__*/ 1, atomic_compare_exchange_strong(&value, &expected, 99));
    KOAN_EQ_INT(/*__*/ 99, atomic_load(&value));

    /* Now the value is 99, so an expectation of 10 fails — and `expected` is
     * overwritten with 99 so the caller can decide what to do. */
    expected = 10;
    KOAN_EQ_INT(/*__*/ 0, atomic_compare_exchange_strong(&value, &expected, 5));
    KOAN_EQ_INT(/*__*/ 99, expected);
    KOAN_EQ_INT(/*__*/ 99, atomic_load(&value));

    /* Which makes the retry loop trivial: it already holds the fresh value. */
    while (!atomic_compare_exchange_weak(&value, &expected, expected * 2)) { }
    KOAN_EQ_INT(/*__*/ 198, atomic_load(&value));
}

/*
 * The _weak form may fail spuriously — return false even when the value did
 * match — because on some CPUs that is much cheaper. It is correct only
 * inside a loop, which is where you almost always want it.
 */
KOAN(weak_may_fail_spuriously_so_loop)
{
    atomic_int value;
    atomic_init(&value, 0);

    int   expected = 0;
    int   attempts = 0;
    while (!atomic_compare_exchange_weak(&value, &expected, 42)) attempts++;

    KOAN_EQ_INT(/*__*/ 42, atomic_load(&value));
    KOAN_TRUE(attempts >= 0);      /* zero on most runs; more is legal */
}

/*
 * Not every type is lock-free. If the hardware cannot do it in one
 * instruction, the implementation uses a hidden lock — still correct, but not
 * safe in a signal handler and not fast. Ask before assuming.
 */
KOAN(some_atomics_are_implemented_with_locks)
{
    atomic_int small;
    atomic_init(&small, 0);
    KOAN_EQ_INT(/*__*/ 1, atomic_is_lock_free(&small));

    /* The compile-time answer, for choosing a design before writing it. */
    KOAN_TRUE(ATOMIC_INT_LOCK_FREE == 2);      /* 2 means always lock-free */

    /* atomic_flag is the one type guaranteed lock-free everywhere, which is
     * why it is the only one usable from a signal handler. */
    atomic_flag gate = ATOMIC_FLAG_INIT;

    /* test_and_set returns the previous state, so false means "I claimed it". */
    KOAN_EQ_INT(/*__*/ 0, atomic_flag_test_and_set(&gate));
    KOAN_EQ_INT(/*__*/ 1, atomic_flag_test_and_set(&gate));

    atomic_flag_clear(&gate);
    KOAN_EQ_INT(/*__*/ 0, atomic_flag_test_and_set(&gate));
}

/*
 * Memory ordering. Every atomic operation takes an optional order; the
 * default, seq_cst, is the strongest and the one to use until you can prove
 * you need less.
 *
 *   relaxed   atomic, but no ordering at all — safe only for counters
 *   acquire   a load; nothing after it may be reordered before it
 *   release   a store; nothing before it may be reordered after it
 *   acq_rel   both, for read-modify-write
 *   seq_cst   as above, plus a single total order all threads agree on
 *
 * A release store paired with an acquire load on the same object is the
 * classic handoff: everything the writer did before the store is visible to
 * whoever sees it.
 */
typedef struct {
    int          payload;        /* an ordinary, non-atomic field */
    atomic_bool  ready;
} Handoff;

static void *producer(void *arg)
{
    Handoff *h = arg;

    h->payload = 42;                                    /* ordinary write */
    atomic_store_explicit(&h->ready, true, memory_order_release);
    return nullptr;
}

KOAN(release_and_acquire_publish_ordinary_writes)
{
    Handoff h = { .payload = 0 };
    atomic_init(&h.ready, false);

    pthread_t t;
    pthread_create(&t, nullptr, producer, &h);

    /* Spin until the flag is set with an acquire load. */
    while (!atomic_load_explicit(&h.ready, memory_order_acquire)) { }

    /* Because the store was a release and this load was an acquire, the
     * ordinary write to payload is guaranteed visible. Reading it without
     * that pairing would be a data race. */
    KOAN_EQ_INT(/*__*/ 42, h.payload);

    pthread_join(t, nullptr);
}

/*
 * Relaxed ordering is atomic but promises nothing about anything else. It is
 * correct for a statistics counter, where only the final total matters, and
 * wrong for anything that publishes data.
 */
typedef struct { atomic_long hits; int iterations; } Counter;

static void *bump_relaxed(void *arg)
{
    Counter *c = arg;
    for (int i = 0; i < c->iterations; i++)
        atomic_fetch_add_explicit(&c->hits, 1, memory_order_relaxed);
    return nullptr;
}

KOAN(relaxed_is_atomic_but_unordered)
{
    Counter c = { .iterations = 10000 };
    atomic_init(&c.hits, 0);

    pthread_t threads[4];
    for (int i = 0; i < 4; i++) pthread_create(&threads[i], nullptr, bump_relaxed, &c);
    for (int i = 0; i < 4; i++) pthread_join(threads[i], nullptr);

    /* Every increment is still counted — relaxed gives up ordering, not
     * atomicity. */
    KOAN_EQ_INT(/*__*/ 40000, (int)atomic_load(&c.hits));
}

/*
 * A fence orders operations without being attached to one object. It is the
 * tool for the rare case where the ordering you need spans several variables.
 */
KOAN(fences_order_without_an_object)
{
    atomic_int a, b;
    atomic_init(&a, 0);
    atomic_init(&b, 0);

    atomic_store_explicit(&a, 1, memory_order_relaxed);
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&b, 1, memory_order_relaxed);

    atomic_thread_fence(memory_order_acquire);

    KOAN_EQ_INT(/*__*/ 1, atomic_load_explicit(&a, memory_order_relaxed));
    KOAN_EQ_INT(/*__*/ 1, atomic_load_explicit(&b, memory_order_relaxed));
}

/*
 * Bringing it together.
 *
 * A lock-free stack, built entirely from compare-and-swap. Push and pop both
 * read the head, prepare a new head, and swap it in only if nothing changed
 * underneath them — retrying if it did.
 *
 * Note what this version does NOT solve: the ABA problem. If a node is popped
 * and pushed back between one thread's read and its swap, the CAS succeeds on
 * a stale pointer. Real implementations add a version counter or hazard
 * pointers. The koan below therefore only pushes, then only pops, which is
 * exactly the case this structure handles correctly.
 */
typedef struct Node {
    int          value;
    struct Node *next;
} Node;

typedef struct {
    _Atomic(Node *) head;
} Stack;

static void stack_push(Stack *s, Node *node)
{
    Node *old = atomic_load(&s->head);
    do {
        node->next = old;
    } while (!atomic_compare_exchange_weak(&s->head, &old, node));
}

static Node *stack_pop(Stack *s)
{
    Node *old = atomic_load(&s->head);
    while (old && !atomic_compare_exchange_weak(&s->head, &old, old->next)) { }
    return old;
}

typedef struct { Stack *stack; Node *nodes; int count; } Pusher;

static void *push_many(void *arg)
{
    Pusher *p = arg;
    for (int i = 0; i < p->count; i++) stack_push(p->stack, &p->nodes[i]);
    return nullptr;
}

KOAN(assembling_a_lock_free_stack)
{
    constexpr int PER_THREAD = 250;
    constexpr int THREADS    = 4;

    static Node nodes[THREADS][PER_THREAD];
    Stack       s;
    atomic_init(&s.head, nullptr);

    /* An empty stack pops nothing rather than crashing. */
    KOAN_TRUE(stack_pop(&s) == nullptr);

    Pusher    pushers[THREADS];
    pthread_t threads[THREADS];

    for (int t = 0; t < THREADS; t++) {
        for (int i = 0; i < PER_THREAD; i++) nodes[t][i].value = 1;
        pushers[t] = (Pusher){ .stack = &s, .nodes = nodes[t], .count = PER_THREAD };
        pthread_create(&threads[t], nullptr, push_many, &pushers[t]);
    }
    for (int t = 0; t < THREADS; t++) pthread_join(threads[t], nullptr);

    /* Every node arrived exactly once: none lost to a race, none duplicated. */
    int popped = 0, total = 0;
    for (Node *n = stack_pop(&s); n; n = stack_pop(&s)) { popped++; total += n->value; }

    KOAN_EQ_INT(/*__*/ 1000, popped);
    KOAN_EQ_INT(/*__*/ 1000, total);

    /* And the stack is empty again. */
    KOAN_TRUE(atomic_load(&s.head) == nullptr);
    KOAN_TRUE(stack_pop(&s) == nullptr);
}

KOAN_LESSON(lesson_about_atomics, "About Atomics and the Memory Model",
    KOAN_CASE(atomic_load_and_store_are_indivisible),
    KOAN_CASE(fetch_operations_return_the_previous_value),
    KOAN_CASE(compare_exchange_reports_what_it_found),
    KOAN_CASE(weak_may_fail_spuriously_so_loop),
    KOAN_CASE(some_atomics_are_implemented_with_locks),
    KOAN_CASE(release_and_acquire_publish_ordinary_writes),
    KOAN_CASE(relaxed_is_atomic_but_unordered),
    KOAN_CASE(fences_order_without_an_object),
    KOAN_CASE(assembling_a_lock_free_stack)
);
