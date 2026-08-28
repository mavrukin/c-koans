/*
 * About Arrays
 *
 * An array in C is a block of contiguous objects, and almost nothing else.
 * It carries no length, no bounds check, and no identity of its own once it
 * has been passed to a function. Understanding exactly when an array stops
 * behaving like an array — "decay" — is the single most useful thing in this
 * lesson.
 */
#include "koan.h"

#include <string.h>

/*
 * Elements are laid out contiguously and indexed from zero. sizeof on an
 * array in the scope where it was declared gives the size of the *whole*
 * array, which is how you recover its length.
 */
KOAN(arrays_know_their_own_size_where_declared)
{
    int nums[] = { 10, 20, 30, 40 };

    KOAN_EQ_SZ(__SZ, sizeof nums);
    KOAN_EQ_SZ(__SZ, sizeof nums / sizeof nums[0]);
    KOAN_EQ_INT(__, nums[2]);
}

/*
 * An initialiser may be shorter than the array. The remaining elements are
 * zero-initialised — all of them, not just the next one. This is the
 * idiomatic way to zero an array.
 */
KOAN(short_initialisers_zero_the_rest)
{
    int partial[5] = { 1, 2 };

    KOAN_EQ_INT(__, partial[1]);
    KOAN_EQ_INT(__, partial[2]);
    KOAN_EQ_INT(__, partial[4]);

    int all_zero[100] = { 0 };
    int sum = 0;
    for (size_t i = 0; i < 100; i++) sum += all_zero[i];
    KOAN_EQ_INT(__, sum);

    /* C23 permits an empty initialiser, which reads better than { 0 }. */
    int also_zero[8] = {};
    KOAN_EQ_INT(__, also_zero[7]);
}

/*
 * Designated initialisers set specific elements by index, in any order.
 * Everything not mentioned is still zeroed.
 */
KOAN(designated_initialisers_pick_elements)
{
    int sparse[6] = { [4] = 40, [1] = 10 };

    KOAN_EQ_INT(__, sparse[1]);
    KOAN_EQ_INT(__, sparse[4]);
    KOAN_EQ_INT(__, sparse[0]);
    KOAN_EQ_INT(__, sparse[5]);
}

/*
 * Indexing is defined as pointer arithmetic: a[i] means *(a + i). Because
 * addition commutes, i[a] is legal and means the same thing. This is not a
 * trick to use, but knowing it is proof you understand what indexing is.
 */
KOAN(indexing_is_pointer_arithmetic)
{
    int nums[] = { 5, 6, 7 };

    KOAN_EQ_INT(__, nums[1]);
    KOAN_EQ_INT(__, *(nums + 1));
    KOAN_EQ_INT(__, 1[nums]);
}

/*
 * Here is the decay. In almost every context an array name converts to a
 * pointer to its first element, losing the length. Inside a function, sizeof
 * measures the *pointer*, not the array.
 *
 * This is why every array-taking function in C also takes a length.
 */
KOAN_DELIBERATE_BEGIN
static size_t sizeof_inside(int arr[10])
{
    /* Despite the "[10]", arr is an int*. The 10 is documentation only,
     * and the compiler warns precisely because this is such a common bug. */
    return sizeof arr;
}
KOAN_DELIBERATE_END

KOAN(arrays_decay_to_pointers_when_passed)
{
    int nums[10] = {};

    KOAN_EQ_SZ(__SZ, sizeof nums);
    KOAN_EQ_SZ(__SZ, sizeof_inside(nums));

    /* Which is why the length must travel separately. */
    KOAN_TRUE(sizeof_inside(nums) != sizeof nums);
}

/*
 * The array name and the address of its first element are the same address,
 * though not the same type. &nums has type int(*)[10]; nums decays to int*.
 */
KOAN(the_array_and_its_first_element_share_an_address)
{
    int nums[4] = { 1, 2, 3, 4 };

    KOAN_TRUE((void *)nums == (void *)&nums[0]);
    KOAN_TRUE((void *)nums == (void *)&nums);

    /* But the types differ, so the arithmetic differs. Adding one to a
     * pointer-to-array steps over the entire array. */
    KOAN_EQ_SZ(__SZ, (size_t)((char *)(&nums + 1) - (char *)&nums));
    KOAN_EQ_SZ(__SZ, (size_t)((char *)(nums + 1) - (char *)nums));
}

/*
 * Arrays cannot be assigned or compared as wholes. To copy one, copy the
 * bytes. To compare, compare the bytes.
 */
KOAN(arrays_are_copied_and_compared_by_bytes)
{
    int src[3] = { 1, 2, 3 };
    int dst[3] = {};

    memcpy(dst, src, sizeof src);
    KOAN_EQ_INT(__, dst[2]);
    KOAN_EQ_INT(__, memcmp(src, dst, sizeof src));

    dst[0] = 99;
    KOAN_TRUE(memcmp(src, dst, sizeof src) != 0);
}

/*
 * One past the end is a valid *address* to compute and compare against, but
 * dereferencing it is undefined. This is precisely what makes the standard
 * half-open loop idiom legal.
 */
KOAN(one_past_the_end_may_be_addressed_not_read)
{
    int nums[3] = { 1, 2, 3 };
    int *begin = nums;
    int *end   = nums + 3;        /* legal to form and compare */

    KOAN_EQ_SZ(__SZ, (size_t)(end - begin));

    int sum = 0;
    for (int *p = begin; p != end; p++) sum += *p;
    KOAN_EQ_INT(__, sum);
}

/*
 * Reading past the end is undefined behaviour, not "reading garbage". The
 * only defence is a bound you carry yourself. Write the guard, always.
 */
static int at_or_default(const int *arr, size_t len, size_t i, int fallback)
{
    return i < len ? arr[i] : fallback;
}

KOAN(bounds_are_your_responsibility)
{
    int nums[3] = { 7, 8, 9 };

    KOAN_EQ_INT(__, at_or_default(nums, 3, 1, -1));
    KOAN_EQ_INT(__, at_or_default(nums, 3, 3, -1));
    KOAN_EQ_INT(__, at_or_default(nums, 3, 99999, -1));
}

/*
 * Bringing it together.
 *
 * Reverse an array in place, then rotate it left by k. Both are classic
 * index-arithmetic exercises, and the rotation is most elegantly done as
 * three reversals — a trick worth carrying with you.
 *
 *   rotate_left(v, n, k) == reverse(0,k) then reverse(k,n) then reverse(0,n)
 */
static void reverse_range(int *a, size_t lo, size_t hi)
{
    while (lo < hi) {
        int tmp = a[lo];
        a[lo] = a[hi];
        a[hi] = tmp;
        lo++;
        hi--;
    }
}

static void rotate_left(int *a, size_t n, size_t k)
{
    if (n == 0) return;
    k %= n;
    if (k == 0) return;
    reverse_range(a, 0, k - 1);
    reverse_range(a, k, n - 1);
    reverse_range(a, 0, n - 1);
}

KOAN(assembling_an_array_rotation)
{
    int v[6] = { 1, 2, 3, 4, 5, 6 };

    reverse_range(v, 0, 5);
    KOAN_EQ_INT(__, v[0]);
    KOAN_EQ_INT(__, v[5]);

    int w[6] = { 1, 2, 3, 4, 5, 6 };
    rotate_left(w, 6, 2);
    KOAN_EQ_INT(__, w[0]);
    KOAN_EQ_INT(__, w[1]);
    KOAN_EQ_INT(__, w[5]);

    /* Rotating by the length is a no-op, and by more than the length wraps. */
    int x[3] = { 1, 2, 3 };
    rotate_left(x, 3, 3);
    KOAN_EQ_INT(__, x[0]);

    rotate_left(x, 3, 4);          /* same as rotating by 1 */
    KOAN_EQ_INT(__, x[0]);
}

KOAN_LESSON(lesson_about_arrays, "About Arrays",
    KOAN_CASE(arrays_know_their_own_size_where_declared),
    KOAN_CASE(short_initialisers_zero_the_rest),
    KOAN_CASE(designated_initialisers_pick_elements),
    KOAN_CASE(indexing_is_pointer_arithmetic),
    KOAN_CASE(arrays_decay_to_pointers_when_passed),
    KOAN_CASE(the_array_and_its_first_element_share_an_address),
    KOAN_CASE(arrays_are_copied_and_compared_by_bytes),
    KOAN_CASE(one_past_the_end_may_be_addressed_not_read),
    KOAN_CASE(bounds_are_your_responsibility),
    KOAN_CASE(assembling_an_array_rotation)
);
