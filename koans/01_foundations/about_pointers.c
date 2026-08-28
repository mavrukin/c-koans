/*
 * About Pointers
 *
 * A pointer is a value that names a location. That is all it is. The
 * difficulty is never the concept; it is the notation — where the `*` binds,
 * what `const` is protecting, and which of the several things called "null"
 * is meant.
 *
 * Read declarations from the identifier outwards. `const char *p` reads:
 * p is a pointer, to char, which is const.
 */
#include "koan.h"

#include <stdint.h>
#include <string.h>

/*
 * & takes an address, * follows one. They undo each other.
 */
KOAN(address_of_and_dereference_are_inverses)
{
    int n = 42;
    int *p = &n;

    KOAN_EQ_INT(__, *p);
    KOAN_TRUE(p == &n);
    KOAN_EQ_INT(__, *&n);

    /* Writing through the pointer writes the original object. */
    *p = 7;
    KOAN_EQ_INT(__, n);
}

/*
 * C23 introduced `nullptr`, a keyword with its own type. Prefer it to NULL:
 * NULL may be defined as a bare 0, which makes it an integer in disguise and
 * causes real problems in variadic calls.
 */
KOAN(nullptr_is_the_modern_null)
{
    int *p = nullptr;

    KOAN_TRUE(p == nullptr);
    KOAN_FALSE(p != nullptr);

    /* A null pointer is false in a condition; any valid pointer is true. */
    int n = 0;
    int *q = &n;
    KOAN_EQ_INT(__, p ? 1 : 0);
    KOAN_EQ_INT(__, q ? 1 : 0);
}

/*
 * The `*` binds to the declarator, not the type. This declares one pointer
 * and one plain int, which is why the style `int *p` is preferred to
 * `int* p` — it puts the star where it actually belongs.
 */
KOAN(the_star_binds_to_the_name)
{
    int a = 1, b = 2;
    int *p = &a, q = b;      /* p is int*, q is int */

    KOAN_EQ_INT(__, *p);
    KOAN_EQ_INT(__, q);
    KOAN_EQ_SZ(__SZ, sizeof q);
}

/*
 * Where `const` sits changes what is protected. There are two independent
 * things to protect: the pointee and the pointer.
 *
 *   const char *p     pointer to const char   — cannot write *p, can move p
 *   char *const p     const pointer to char   — can write *p, cannot move p
 *   const char *const p                       — neither
 */
KOAN(const_position_decides_what_is_protected)
{
    char buf[] = "abc";

    const char *to_const = buf;     /* may not write through */
    char *const fixed    = buf;     /* may not be repointed  */

    KOAN_EQ_CHR(__CHR, to_const[0]);

    to_const = "xyz";               /* legal: the pointer may move */
    KOAN_EQ_CHR(__CHR, to_const[0]);

    fixed[0] = 'z';                 /* legal: the pointee may change */
    KOAN_EQ_STR(__STR, buf);
}

/*
 * A pointer's type decides how far "one step" is. Arithmetic is in units of
 * the pointee, not bytes.
 */
KOAN(pointer_arithmetic_steps_by_pointee_size)
{
    int      nums[4] = { 10, 20, 30, 40 };
    int     *ip = nums;
    char    *cp = (char *)nums;

    KOAN_EQ_INT(__, *(ip + 1));

    /* Advancing an int* by 1 advances the address by sizeof(int). */
    KOAN_EQ_SZ(__SZ, (size_t)((char *)(ip + 1) - (char *)ip));
    KOAN_EQ_SZ(__SZ, (size_t)((cp + 1) - cp));

    /* Subtracting two pointers into the same array yields an element count. */
    KOAN_EQ_SZ(__SZ, (size_t)(&nums[3] - &nums[0]));
}

/*
 * void* is the generic pointer: any object pointer converts to it and back
 * without a cast, and without loss. It cannot be dereferenced, because the
 * compiler has no idea how many bytes to read.
 */
KOAN(void_pointers_are_generic)
{
    int   n = 1234;
    void *v = &n;                 /* no cast needed on the way in */
    int  *back = v;               /* nor on the way out, in C     */

    KOAN_EQ_INT(__, *back);
    KOAN_TRUE(v == &n);
}

/*
 * A pointer to a pointer is used whenever a function must change the
 * caller's *pointer*, not merely what it points at. This is the shape of
 * every allocator and every "give me a new node" API.
 */
static void point_it_at(int **slot, int *target)
{
    *slot = target;
}

KOAN(pointers_to_pointers_rebind_the_caller)
{
    int a = 1, b = 2;
    int *p = &a;

    point_it_at(&p, &b);
    KOAN_EQ_INT(__, *p);
    KOAN_TRUE(p == &b);
}

/*
 * Pointers to different objects compare unequal; pointers to the same object
 * compare equal regardless of how they were obtained.
 */
KOAN(pointer_identity_is_address_identity)
{
    int nums[3] = { 1, 2, 3 };

    KOAN_TRUE(&nums[1] == nums + 1);
    KOAN_TRUE(&nums[0] != &nums[1]);

    /* Two string literals with the same contents may or may not share
     * storage — that is unspecified, so never compare literals with ==. */
    const char *s = "hello";
    KOAN_EQ_INT(__, strcmp(s, "hello"));
}

/*
 * A dangling pointer refers to storage whose lifetime has ended. The value is
 * not "wrong", it is unusable: even reading it is undefined. Set pointers to
 * nullptr when the thing they name goes away, so that mistakes fail loudly.
 */
KOAN(dangling_pointers_must_be_retired)
{
    int *p = nullptr;

    {
        int local = 5;
        p = &local;
        KOAN_EQ_INT(__, *p);
    }   /* local's lifetime ends here; p is now dangling */

    p = nullptr;                  /* the discipline that makes it safe */
    KOAN_TRUE(p == nullptr);
}

/*
 * Casting a pointer to an integer requires uintptr_t, the one integer type
 * guaranteed to hold a pointer without loss. Do not use int, and do not
 * assume a size.
 */
KOAN(uintptr_t_holds_a_pointer)
{
    int       n = 0;
    int      *p = &n;
    uintptr_t as_int = (uintptr_t)p;
    int      *back = (int *)as_int;

    KOAN_TRUE(back == p);
    KOAN_TRUE(sizeof(uintptr_t) >= sizeof(int *));
}

/*
 * Bringing it together.
 *
 * A singly linked list built with pointer-to-pointer traversal. The trick
 * below — walking with an `Node **` rather than a `Node *` — removes every
 * special case for "the head", which is why experienced C programmers reach
 * for it. There is no `if (node == head)` anywhere.
 */
typedef struct Node {
    int          value;
    struct Node *next;
} Node;

/* Insert keeping the list sorted ascending, with no head special-case. */
static void insert_sorted(Node **head, Node *fresh)
{
    Node **link = head;
    while (*link != nullptr && (*link)->value < fresh->value)
        link = &(*link)->next;
    fresh->next = *link;
    *link = fresh;
}

/* Remove the first node with the given value; returns whether it removed. */
static bool remove_value(Node **head, int value)
{
    for (Node **link = head; *link != nullptr; link = &(*link)->next) {
        if ((*link)->value == value) {
            *link = (*link)->next;
            return true;
        }
    }
    return false;
}

static size_t list_to_array(const Node *head, int *out, size_t cap)
{
    size_t n = 0;
    for (const Node *p = head; p && n < cap; p = p->next) out[n++] = p->value;
    return n;
}

KOAN(assembling_a_linked_list_with_pointer_to_pointer)
{
    Node  a = { .value = 3 }, b = { .value = 1 }, c = { .value = 2 };
    Node *head = nullptr;

    insert_sorted(&head, &a);
    insert_sorted(&head, &b);
    insert_sorted(&head, &c);

    int  got[4];
    size_t n = list_to_array(head, got, 4);

    KOAN_EQ_SZ(__SZ, n);
    KOAN_EQ_INT(__, got[0]);
    KOAN_EQ_INT(__, got[1]);
    KOAN_EQ_INT(__, got[2]);

    /* Removing the head needs no special handling at all. */
    KOAN_EQ_INT(__, remove_value(&head, 1));
    n = list_to_array(head, got, 4);
    KOAN_EQ_SZ(__SZ, n);
    KOAN_EQ_INT(__, got[0]);

    /* Nor does removing from the middle, or failing to find anything. */
    KOAN_EQ_INT(__, remove_value(&head, 99));
    KOAN_EQ_INT(__, remove_value(&head, 3));
    n = list_to_array(head, got, 4);
    KOAN_EQ_SZ(__SZ, n);
    KOAN_EQ_INT(__, got[0]);
}

KOAN_LESSON(lesson_about_pointers, "About Pointers",
    KOAN_CASE(address_of_and_dereference_are_inverses),
    KOAN_CASE(nullptr_is_the_modern_null),
    KOAN_CASE(the_star_binds_to_the_name),
    KOAN_CASE(const_position_decides_what_is_protected),
    KOAN_CASE(pointer_arithmetic_steps_by_pointee_size),
    KOAN_CASE(void_pointers_are_generic),
    KOAN_CASE(pointers_to_pointers_rebind_the_caller),
    KOAN_CASE(pointer_identity_is_address_identity),
    KOAN_CASE(dangling_pointers_must_be_retired),
    KOAN_CASE(uintptr_t_holds_a_pointer),
    KOAN_CASE(assembling_a_linked_list_with_pointer_to_pointer)
);
