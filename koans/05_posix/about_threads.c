/*
 * About Threads and Synchronisation
 *
 * Threads share everything: one address space, one heap, one set of globals.
 * That is what makes them cheap, and it is what makes them dangerous. A data
 * race — two threads touching the same object, at least one of them writing,
 * with no synchronisation between them — is undefined behaviour, not merely
 * a wrong answer.
 *
 * These koans use pthreads, which is what actually ships everywhere. C11's
 * <threads.h> is an *optional* part of the standard, and several major
 * platforms (Apple's, for one) do not provide it; include/compat.h supplies a
 * faithful shim over pthreads so the last koan can still be about the
 * standard API. Read that shim — it shows exactly how the two correspond.
 *
 * Run this lesson under `make san`. ThreadSanitizer is not wired in, but
 * ASan will still catch the memory errors that races tend to produce.
 */
#include "koan.h"
#include "compat.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * A thread runs a function that takes and returns void*. Creating one returns
 * 0 on success — note that it does NOT use errno, but returns the error code
 * directly, unlike most of POSIX.
 */
static void *return_a_number(void *arg)
{
    int n = *(const int *)arg;
    return (void *)(intptr_t)(n * 2);
}

KOAN(a_thread_runs_a_function_and_returns_a_pointer)
{
    pthread_t t;
    int       input = 21;

    KOAN_EQ_INT(__, pthread_create(&t, nullptr, return_a_number, &input));

    void *result = nullptr;
    KOAN_EQ_INT(__, pthread_join(t, &result));
    KOAN_EQ_INT(__, (int)(intptr_t)result);
}

/*
 * Joining is what makes a thread's work visible to you. Until you join (or
 * synchronise some other way), you have no guarantee the thread has finished
 * or that its writes are observable.
 */
static void *set_a_flag(void *arg)
{
    int *flag = arg;
    *flag = 1;
    return nullptr;
}

KOAN(joining_establishes_a_happens_before_edge)
{
    int       flag = 0;
    pthread_t t;

    pthread_create(&t, nullptr, set_a_flag, &flag);
    pthread_join(t, nullptr);

    /* Safe to read only because the join ordered the two threads. */
    KOAN_EQ_INT(__, flag);
}

/*
 * Threads share the heap and globals. This is the whole point, and the whole
 * problem: `counter++` is a read, an add and a write, and another thread may
 * land between any two of them.
 *
 * A mutex makes the sequence indivisible.
 */
typedef struct {
    long            counter;
    pthread_mutex_t lock;
    int             iterations;
} Shared;

static void *increment_guarded(void *arg)
{
    Shared *s = arg;
    for (int i = 0; i < s->iterations; i++) {
        pthread_mutex_lock(&s->lock);
        s->counter++;
        pthread_mutex_unlock(&s->lock);
    }
    return nullptr;
}

KOAN(a_mutex_makes_read_modify_write_indivisible)
{
    Shared s = { .counter = 0, .iterations = 10000 };
    pthread_mutex_init(&s.lock, nullptr);

    pthread_t threads[4];
    for (int i = 0; i < 4; i++)
        pthread_create(&threads[i], nullptr, increment_guarded, &s);
    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], nullptr);

    pthread_mutex_destroy(&s.lock);

    /* Four threads, ten thousand increments each, and not one lost. */
    KOAN_EQ_INT(__, (int)s.counter);
}

/*
 * An atomic is the lock-free alternative for a single value. It is not a
 * mutex substitute for compound invariants — it makes one operation
 * indivisible, nothing more — but for a counter it is exactly right, and
 * considerably faster.
 */
typedef struct {
    atomic_long counter;
    int         iterations;
} AtomicShared;

static void *increment_atomic(void *arg)
{
    AtomicShared *s = arg;
    for (int i = 0; i < s->iterations; i++)
        atomic_fetch_add(&s->counter, 1);
    return nullptr;
}

KOAN(atomics_make_one_operation_indivisible)
{
    AtomicShared s = { .iterations = 10000 };
    atomic_init(&s.counter, 0);

    pthread_t threads[4];
    for (int i = 0; i < 4; i++)
        pthread_create(&threads[i], nullptr, increment_atomic, &s);
    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], nullptr);

    KOAN_EQ_INT(__, (int)atomic_load(&s.counter));

    /* Compare-and-swap is the primitive the rest are built from: it writes
     * only if the value is still what you last saw. */
    atomic_int value;
    atomic_init(&value, 10);

    int expected = 10;
    KOAN_EQ_INT(__, atomic_compare_exchange_strong(&value, &expected, 99));
    KOAN_EQ_INT(__, atomic_load(&value));

    /* The second attempt fails, and reports what it actually found. */
    expected = 10;
    KOAN_EQ_INT(__, atomic_compare_exchange_strong(&value, &expected, 5));
    KOAN_EQ_INT(__, expected);
}

/*
 * A condition variable lets a thread wait for a *predicate* rather than spin
 * on it. Three rules, and all three are mandatory:
 *
 *   1. the mutex must be held when you wait
 *   2. you must wait in a loop, because wakeups can be spurious
 *   3. the predicate must be changed with the mutex held
 *
 * Rule 2 is the one people omit, and it is the one that bites.
 */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  ready;
    bool            data_available;
    int             payload;
} Channel;

static void *producer(void *arg)
{
    Channel *ch = arg;

    pthread_mutex_lock(&ch->lock);
    ch->payload = 42;
    ch->data_available = true;
    pthread_cond_signal(&ch->ready);
    pthread_mutex_unlock(&ch->lock);
    return nullptr;
}

KOAN(condition_variables_wait_for_a_predicate)
{
    Channel ch = { .data_available = false, .payload = 0 };
    pthread_mutex_init(&ch.lock, nullptr);
    pthread_cond_init(&ch.ready, nullptr);

    pthread_t t;
    pthread_create(&t, nullptr, producer, &ch);

    pthread_mutex_lock(&ch.lock);
    while (!ch.data_available)                 /* the loop, not an if */
        pthread_cond_wait(&ch.ready, &ch.lock);
    int got = ch.payload;
    pthread_mutex_unlock(&ch.lock);

    pthread_join(t, nullptr);
    pthread_cond_destroy(&ch.ready);
    pthread_mutex_destroy(&ch.lock);

    KOAN_EQ_INT(__, got);
}

/*
 * Thread-local storage gives each thread its own copy of a variable. C23
 * spells it `thread_local`; the C11 spelling was `_Thread_local`. It is the
 * cleanest fix for a function that was made non-reentrant by a static.
 */
static thread_local int per_thread_value = 0;

static void *stamp_thread_local(void *arg)
{
    per_thread_value = *(const int *)arg;
    /* Each thread sees only its own copy. */
    return (void *)(intptr_t)per_thread_value;
}

KOAN(thread_local_gives_each_thread_its_own_copy)
{
    per_thread_value = 1;               /* the main thread's copy */

    int       a_input = 10, b_input = 20;
    pthread_t a, b;
    void     *a_out = nullptr, *b_out = nullptr;

    pthread_create(&a, nullptr, stamp_thread_local, &a_input);
    pthread_join(a, &a_out);
    pthread_create(&b, nullptr, stamp_thread_local, &b_input);
    pthread_join(b, &b_out);

    KOAN_EQ_INT(__, (int)(intptr_t)a_out);
    KOAN_EQ_INT(__, (int)(intptr_t)b_out);

    /* And the main thread's copy was never touched by either. */
    KOAN_EQ_INT(__, per_thread_value);
}

/*
 * The C11/C23 standard threads API. Where it exists it is a thin renaming of
 * pthreads; where it does not, compat.h provides it. Note the differences:
 * the start function returns int rather than void*, and the success code is
 * the named constant thrd_success rather than 0.
 */
static int standard_thread_body(void *arg)
{
    int *n = arg;
    return *n + 1;
}

KOAN(c11_threads_are_pthreads_renamed)
{
    thrd_t t;
    int    input = 41;

    KOAN_EQ_INT(__,
                thrd_create(&t, standard_thread_body, &input));

    int result = 0;
    KOAN_EQ_INT(__, thrd_join(t, &result));
    KOAN_EQ_INT(__, result);

    /* mtx_t and cnd_t mirror pthread_mutex_t and pthread_cond_t exactly. */
    mtx_t m;
    KOAN_EQ_INT(__, mtx_init(&m, mtx_plain));
    KOAN_EQ_INT(__, mtx_lock(&m));
    KOAN_EQ_INT(__, mtx_unlock(&m));
    mtx_destroy(&m);
}

/*
 * Bringing it together.
 *
 * A bounded blocking queue: the classic producer/consumer structure, and the
 * heart of the thread pool you will build in the capstone. It needs a mutex,
 * two condition variables (one for "not full", one for "not empty"), a ring
 * buffer, and a shutdown protocol that never leaves a thread waiting forever.
 *
 * The shutdown is the part people get wrong. Setting the flag is not enough;
 * you must broadcast, so that every waiter re-checks its predicate.
 */
#define QUEUE_CAP 4

typedef struct {
    int             slots[QUEUE_CAP];
    size_t          head, tail, count;
    bool            closed;
    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
} Queue;

static void queue_init(Queue *q)
{
    *q = (Queue){ 0 };
    pthread_mutex_init(&q->lock, nullptr);
    pthread_cond_init(&q->not_full, nullptr);
    pthread_cond_init(&q->not_empty, nullptr);
}

static void queue_destroy(Queue *q)
{
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}

static bool queue_push(Queue *q, int value)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == QUEUE_CAP && !q->closed)
        pthread_cond_wait(&q->not_full, &q->lock);

    if (q->closed) { pthread_mutex_unlock(&q->lock); return false; }

    q->slots[q->tail] = value;
    q->tail = (q->tail + 1) % QUEUE_CAP;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return true;
}

/* Returns false only when the queue is both empty and closed. */
static bool queue_pop(Queue *q, int *out)
{
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->closed)
        pthread_cond_wait(&q->not_empty, &q->lock);

    if (q->count == 0) { pthread_mutex_unlock(&q->lock); return false; }

    *out = q->slots[q->head];
    q->head = (q->head + 1) % QUEUE_CAP;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return true;
}

static void queue_close(Queue *q)
{
    pthread_mutex_lock(&q->lock);
    q->closed = true;
    pthread_cond_broadcast(&q->not_empty);   /* wake every waiter, not one */
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}

typedef struct { Queue *q; long sum; int taken; } Consumer;

static void *consume_all(void *arg)
{
    Consumer *c = arg;
    int       value;
    while (queue_pop(c->q, &value)) { c->sum += value; c->taken++; }
    return nullptr;
}

KOAN(assembling_a_blocking_queue)
{
    Queue q;
    queue_init(&q);

    Consumer consumers[3] = { { .q = &q }, { .q = &q }, { .q = &q } };
    pthread_t threads[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&threads[i], nullptr, consume_all, &consumers[i]);

    /* Push more items than the queue can hold, so the producer must block. */
    constexpr int ITEMS = 100;
    for (int i = 1; i <= ITEMS; i++) KOAN_TRUE(queue_push(&q, i));

    queue_close(&q);
    for (int i = 0; i < 3; i++) pthread_join(threads[i], nullptr);

    long total = 0;
    int  taken = 0;
    for (int i = 0; i < 3; i++) { total += consumers[i].sum; taken += consumers[i].taken; }

    /* Every item was delivered exactly once: 1 + 2 + ... + 100. */
    KOAN_EQ_INT(__, (int)total);
    KOAN_EQ_INT(__, taken);

    /* And pushing to a closed queue is refused rather than silently lost. */
    KOAN_EQ_INT(__, queue_push(&q, 1));

    queue_destroy(&q);
}

KOAN_LESSON(lesson_about_threads, "About Threads and Synchronisation",
    KOAN_CASE(a_thread_runs_a_function_and_returns_a_pointer),
    KOAN_CASE(joining_establishes_a_happens_before_edge),
    KOAN_CASE(a_mutex_makes_read_modify_write_indivisible),
    KOAN_CASE(atomics_make_one_operation_indivisible),
    KOAN_CASE(condition_variables_wait_for_a_predicate),
    KOAN_CASE(thread_local_gives_each_thread_its_own_copy),
    KOAN_CASE(c11_threads_are_pthreads_renamed),
    KOAN_CASE(assembling_a_blocking_queue)
);
