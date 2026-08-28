/*
 * tests.c — self-checks for the capstone libraries.
 *
 * These are not koans; they are the tests the libraries ship with, and they
 * are what `make -C projects check` runs. Read them as worked examples of the
 * APIs, and as a demonstration that a C project can be tested without a
 * framework.
 */
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena/arena.h"
#include "containers/containers.h"
#include "json/json.h"

static int failures = 0;
static int checks   = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        checks++;                                                             \
        if (!(cond)) {                                                        \
            failures++;                                                       \
            fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                                     \
    } while (0)

#define CHECK_STR(a, b)                                                       \
    do {                                                                      \
        checks++;                                                             \
        const char *x_ = (a), *y_ = (b);                                      \
        if (!x_ || !y_ || strcmp(x_, y_) != 0) {                              \
            failures++;                                                       \
            fprintf(stderr, "  FAIL %s:%d\n    expected: %s\n    actual:   %s\n", \
                    __FILE__, __LINE__, y_ ? y_ : "(null)", x_ ? x_ : "(null)"); \
        }                                                                     \
    } while (0)

/* ------------------------------------------------------------------ */

static void test_arena(void)
{
    Arena a;
    arena_init(&a, 1024);

    /* Allocations are correctly aligned for their type. */
    int    *i = ARENA_NEW(&a, int);
    double *d = ARENA_NEW(&a, double);
    CHECK(i != nullptr && d != nullptr);
    CHECK((uintptr_t)d % alignof(double) == 0);

    *i = 42; *d = 1.5;
    CHECK(*i == 42 && *d == 1.5);

    /* Strings and formatted text. */
    char *s = arena_strdup(&a, "borrowed");
    CHECK_STR(s, "borrowed");
    CHECK_STR(arena_printf(&a, "%s=%d", "port", 8080), "port=8080");

    /* An allocation larger than the block size gets its own block. */
    size_t before = a.block_count;
    void  *big = arena_alloc(&a, 8192, 16);
    CHECK(big != nullptr);
    CHECK(a.block_count > before);

    /* calloc zeroes and refuses an overflowing product. */
    int *zeros = arena_calloc(&a, 16, sizeof(int));
    CHECK(zeros != nullptr && zeros[15] == 0);
    CHECK(arena_calloc(&a, SIZE_MAX / 2, SIZE_MAX / 2) == nullptr);

    /* A non-power-of-two alignment is refused rather than mis-aligning. */
    CHECK(arena_alloc(&a, 8, 3) == nullptr);

    /* Reset keeps one block and forgets the contents. */
    arena_reset(&a);
    CHECK(a.used_bytes == 0);
    CHECK(a.block_count == 1);

    /* Resetting in a loop must not grow without bound. */
    for (int round = 0; round < 50; round++) {
        for (int k = 0; k < 100; k++) (void)ARENA_NEW(&a, double);
        arena_reset(&a);
    }
    CHECK(a.block_count == 1);

    arena_free(&a);
    CHECK(a.head == nullptr && a.total_bytes == 0);

    /* An arena is reusable after being freed. */
    void *again = arena_alloc(&a, 16, 8);
    CHECK(again != nullptr);
    arena_free(&a);
}

/* ------------------------------------------------------------------ */

VEC_DECLARE(IntVec, int_vec, int);
VEC_DEFINE(IntVec, int_vec, int)

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static bool is_negative(const void *elem, void *user)
{
    (void)user;
    return *(const int *)elem < 0;
}

static void test_vec(void)
{
    Vec v;
    vec_init(&v, sizeof(int));

    CHECK(v.len == 0 && v.data == nullptr);
    CHECK(vec_at(&v, 0) == nullptr);

    for (int i = 0; i < 100; i++) CHECK(vec_push(&v, &i));
    CHECK(v.len == 100);
    CHECK(*(int *)vec_at(&v, 99) == 99);
    CHECK(vec_at(&v, 100) == nullptr);

    int popped = -1;
    CHECK(vec_pop(&v, &popped) && popped == 99);
    CHECK(v.len == 99);

    /* Sorting uses the same comparator contract as qsort. */
    int desc[] = { 5, 1, 4, 2, 3 };
    Vec s;
    vec_init(&s, sizeof(int));
    for (size_t i = 0; i < 5; i++) vec_push(&s, &desc[i]);
    vec_sort(&s, cmp_int);
    CHECK(*(int *)vec_at(&s, 0) == 1 && *(int *)vec_at(&s, 4) == 5);

    /* Searching with a predicate and user data. */
    CHECK(vec_find(&s, is_negative, nullptr) == SIZE_MAX);
    int negative = -7;
    vec_push(&s, &negative);
    CHECK(vec_find(&s, is_negative, nullptr) == 5);

    vec_free(&s);
    vec_free(&v);
    CHECK(v.len == 0 && v.data == nullptr);

    /* The typed wrapper: no casts anywhere. */
    IntVec t;
    int_vec_init(&t);
    int_vec_push(&t, 10);
    int_vec_push(&t, 20);
    CHECK(int_vec_len(&t) == 2);
    CHECK(int_vec_at(&t, 1) == 20);
    int_vec_free(&t);

    /* A vector of structs works identically — the point of elem_size. */
    typedef struct { int x, y; } Point;
    Vec pts;
    vec_init(&pts, sizeof(Point));
    Point p = { .x = 3, .y = 4 };
    vec_push(&pts, &p);
    CHECK(((Point *)vec_at(&pts, 0))->y == 4);
    vec_free(&pts);
}

/* ------------------------------------------------------------------ */

static bool count_entries(const char *key, void *value, void *user)
{
    (void)key; (void)value;
    (*(int *)user)++;
    return true;
}

static void test_map(void)
{
    Map m;
    map_init(&m);

    CHECK(map_get(&m, "absent") == nullptr);
    CHECK(!map_has(&m, "absent"));
    CHECK(!map_remove(&m, "absent"));

    static int one = 1, two = 2, three = 3;
    CHECK(map_put(&m, "one", &one));
    CHECK(map_put(&m, "two", &two));
    CHECK(map_put(&m, "three", &three));
    CHECK(m.len == 3);

    CHECK(map_get(&m, "two") == &two);
    CHECK(map_has(&m, "one"));
    CHECK(!map_has(&m, "four"));

    /* Putting an existing key replaces the value without growing. */
    CHECK(map_put(&m, "two", &three));
    CHECK(m.len == 3);
    CHECK(map_get(&m, "two") == &three);

    /* Removal leaves a tombstone, so later keys are still reachable. */
    CHECK(map_remove(&m, "one"));
    CHECK(m.len == 2);
    CHECK(!map_has(&m, "one"));
    CHECK(map_has(&m, "three"));       /* must survive the deletion */
    CHECK(map_get(&m, "two") == &three);

    /* Many keys, forcing several rehashes. */
    static int values[500];
    for (int i = 0; i < 500; i++) {
        char key[32];
        snprintf(key, sizeof key, "key-%d", i);
        values[i] = i;
        CHECK(map_put(&m, key, &values[i]));
    }
    CHECK(m.len == 502);

    /* Every one of them is still findable after the rehashing. */
    int found = 0;
    for (int i = 0; i < 500; i++) {
        char key[32];
        snprintf(key, sizeof key, "key-%d", i);
        int *got = map_get(&m, key);
        if (got && *got == i) found++;
    }
    CHECK(found == 500);

    int visited = 0;
    map_each(&m, count_entries, &visited);
    CHECK(visited == 502);

    map_free(&m);
    CHECK(m.len == 0 && m.slots == nullptr);
}

/* ------------------------------------------------------------------ */

static void test_json(void)
{
    Arena     a;
    JsonError err;
    arena_init(&a, 0);

    /* Scalars. */
    CHECK(json_parse(&a, "null", &err)->kind == JSON_NULL);
    CHECK(json_bool_or(json_parse(&a, "true", &err), false) == true);
    CHECK(json_number_or(json_parse(&a, "-3.5e2", &err), 0) == -350.0);
    CHECK_STR(json_string_or(json_parse(&a, "\"hi\"", &err), ""), "hi");

    /* A document with every type in it. */
    const char *text =
        "{"
        "  \"name\": \"koan\","
        "  \"version\": 3,"
        "  \"ratio\": 0.5,"
        "  \"tags\": [\"c\", \"posix\", \"c23\"],"
        "  \"nested\": { \"deep\": { \"value\": 42 } },"
        "  \"enabled\": true,"
        "  \"missing\": null,"
        "  \"empty_list\": [],"
        "  \"empty_obj\": {}"
        "}";

    JsonValue *doc = json_parse(&a, text, &err);
    CHECK(doc != nullptr);
    CHECK(err.message == nullptr);
    CHECK(doc->kind == JSON_OBJECT);
    CHECK(doc->object.count == 9);

    CHECK_STR(json_string_or(json_member(doc, "name"), ""), "koan");
    CHECK(json_number_or(json_member(doc, "version"), 0) == 3.0);
    CHECK(json_number_or(json_member(doc, "ratio"), 0) == 0.5);
    CHECK(json_bool_or(json_member(doc, "enabled"), false) == true);
    CHECK(json_member(doc, "missing")->kind == JSON_NULL);

    JsonValue *tags = json_member(doc, "tags");
    CHECK(tags->array.count == 3);
    CHECK_STR(json_string_or(json_index(tags, 1), ""), "posix");
    CHECK(json_index(tags, 3) == nullptr);

    /* Nested lookup, and a missing key yielding null rather than crashing. */
    JsonValue *deep = json_member(json_member(doc, "nested"), "deep");
    CHECK(json_number_or(json_member(deep, "value"), 0) == 42.0);
    CHECK(json_member(doc, "nope") == nullptr);
    CHECK(json_number_or(json_member(doc, "nope"), -1) == -1.0);

    /* Asking an array for a member, or an object for an index, is safe. */
    CHECK(json_member(tags, "name") == nullptr);
    CHECK(json_index(doc, 0) == nullptr);

    CHECK(json_member(doc, "empty_list")->array.count == 0);
    CHECK(json_member(doc, "empty_obj")->object.count == 0);

    /* Escapes, including a surrogate pair becoming one UTF-8 character. */
    JsonValue *esc = json_parse(&a, "\"tab\\there\\n\\u00e9\\u0001F\"", &err);
    CHECK(esc != nullptr);
    CHECK(strstr(json_string_or(esc, ""), "\t") != nullptr);

    JsonValue *emoji = json_parse(&a, "\"\\ud83d\\ude00\"", &err);
    CHECK(emoji != nullptr);
    CHECK(strlen(json_string_or(emoji, "")) == 4);      /* one 4-byte character */

    /* Round trip: parse, write, parse again, same values. */
    char *compact = json_write(&a, doc, 0);
    CHECK(compact != nullptr);

    JsonValue *again = json_parse(&a, compact, &err);
    CHECK(again != nullptr);
    CHECK_STR(json_string_or(json_member(again, "name"), ""), "koan");
    CHECK(json_number_or(json_member(again, "version"), 0) == 3.0);

    /* An integral number is written without a decimal point. */
    CHECK(strstr(compact, "\"version\":3") != nullptr);

    char *pretty = json_write(&a, doc, 2);
    CHECK(pretty != nullptr && strchr(pretty, '\n') != nullptr);
    CHECK(json_parse(&a, pretty, &err) != nullptr);

    /* Errors are reported with a position, and the first failure wins. */
    static const struct { const char *input; const char *why; } BAD[] = {
        { "{",              "unterminated object"   },
        { "[1,]",           "trailing comma"        },
        { "{\"a\":}",       "missing value"         },
        { "{a:1}",          "unquoted key"          },
        { "01",             "leading zero"          },
        { "1.",             "no digit after point"  },
        { "1e",             "no exponent digits"    },
        { "\"unterminated", "unterminated string"   },
        { "\"bad\\x\"",     "unknown escape"        },
        { "\"\\ud800\"",    "lone surrogate"        },
        { "1 2",            "trailing content"      },
        { "tru",            "truncated literal"     },
        { "",               "empty input"           },
        { "+1",             "leading plus"          },
    };

    for (size_t i = 0; i < sizeof BAD / sizeof *BAD; i++) {
        JsonError e = { 0 };
        JsonValue *bad = json_parse(&a, BAD[i].input, &e);
        checks++;
        if (bad != nullptr || e.message == nullptr) {
            failures++;
            fprintf(stderr, "  FAIL %s should be rejected (%s)\n",
                    BAD[i].input, BAD[i].why);
        }
    }

    /* Deep nesting is refused rather than overflowing the stack. */
    char deep_text[3000];
    size_t n = 0;
    for (int i = 0; i < 1400; i++) deep_text[n++] = '[';
    deep_text[n] = '\0';
    JsonError deep_err = { 0 };
    CHECK(json_parse(&a, deep_text, &deep_err) == nullptr);
    CHECK_STR(deep_err.message, "nesting too deep");

    /* One free releases every node in every document parsed above. */
    arena_free(&a);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    printf("capstone self-checks\n");

    test_arena();
    printf("  arena       ok\n");
    test_vec();
    printf("  vec         ok\n");
    test_map();
    printf("  map         ok\n");
    test_json();
    printf("  json        ok\n");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
