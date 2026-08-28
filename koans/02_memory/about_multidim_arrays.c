/*
 * About Multidimensional Arrays
 *
 * `int a[3][4]` is not an array of pointers. It is 12 contiguous ints, and
 * `a[i]` is an *array of 4*, not a pointer stored somewhere. Confusing the two
 * is the single most common cause of segfaults in C matrix code, because both
 * are written `a[i][j]`.
 */
#include "koan.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Row-major: the last index varies fastest. The whole thing is one block.
 */
KOAN(rows_are_contiguous)
{
    int grid[3][4] = {
        {  0,  1,  2,  3 },
        { 10, 11, 12, 13 },
        { 20, 21, 22, 23 },
    };

    KOAN_EQ_SZ(__SZ, sizeof grid);
    KOAN_EQ_SZ(__SZ, sizeof grid[0]);      /* one row */
    KOAN_EQ_SZ(__SZ, sizeof grid[0][0]);

    /* Because it is one block, it can be read as a flat array. */
    const int *flat = &grid[0][0];
    KOAN_EQ_INT(__, flat[5]);              /* row 1, column 1 */
    KOAN_EQ_INT(__, flat[2 * 4 + 2]);

    /* The index arithmetic the compiler is doing for you. */
    KOAN_EQ_INT(__, flat[1 * 4 + 3]);
}

/*
 * A 2D array decays to a *pointer to a row*, not to `int **`. The row width
 * is part of the type, which is why a function taking `int (*)[4]` can only
 * ever be given arrays 4 wide.
 */
static int sum_fixed(int (*rows)[4], size_t nrows)
{
    int total = 0;
    for (size_t r = 0; r < nrows; r++)
        for (size_t c = 0; c < 4; c++) total += rows[r][c];
    return total;
}

KOAN(a_2d_array_decays_to_a_pointer_to_a_row)
{
    int grid[2][4] = { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } };

    KOAN_EQ_INT(__, sum_fixed(grid, 2));

    /* Stepping a pointer-to-row advances by a whole row. */
    int (*p)[4] = grid;
    KOAN_EQ_SZ(__SZ, (size_t)((char *)(p + 1) - (char *)p));
    KOAN_EQ_INT(__, p[1][0]);
}

/*
 * Baking the width into the type is inflexible. The portable alternative is a
 * flat buffer plus explicit index arithmetic — which is what the compiler was
 * doing anyway, now under your control.
 */
static int at(const int *flat, size_t width, size_t r, size_t c)
{
    return flat[r * width + c];
}

KOAN(flat_buffers_take_any_shape)
{
    int data[6] = { 1, 2, 3, 4, 5, 6 };

    /* The same six ints, read as 2x3 and then as 3x2. */
    KOAN_EQ_INT(__, at(data, 3, 1, 0));
    KOAN_EQ_INT(__, at(data, 3, 1, 2));
    KOAN_EQ_INT(__, at(data, 2, 2, 0));
    KOAN_EQ_INT(__, at(data, 2, 1, 0));
}

/*
 * `int **` is a genuinely different structure: an array of pointers, each to a
 * separately allocated row. Rows need not be adjacent or even the same length,
 * and it costs an extra indirection per access.
 */
KOAN(pointer_to_pointer_is_a_different_shape)
{
    int  *rows[3];
    for (size_t r = 0; r < 3; r++) {
        rows[r] = malloc(3 * sizeof **rows);
        for (size_t c = 0; c < 3; c++) rows[r][c] = (int)(r * 3 + c);
    }

    int **jagged = rows;
    KOAN_EQ_INT(__, jagged[1][2]);

    /* The rows are separate allocations, so this is NOT one flat block —
     * reading it as one would be undefined. */
    KOAN_EQ_SZ(__SZ, sizeof rows);

    for (size_t r = 0; r < 3; r++) free(rows[r]);
}

/*
 * A variable-length array is sized at runtime, on the stack. It is real C99+
 * (optional since C11, but supported everywhere you will meet). Keep them
 * small: there is no way to detect stack exhaustion.
 */
KOAN_DELIBERATE_BEGIN
static int sum_vla(size_t n)
{
    int scratch[n];                       /* size known only now */
    for (size_t i = 0; i < n; i++) scratch[i] = (int)i;

    int total = 0;
    for (size_t i = 0; i < n; i++) total += scratch[i];
    return total;
}
KOAN_DELIBERATE_END

KOAN(vlas_are_sized_at_runtime)
{
    KOAN_EQ_INT(__, sum_vla(5));
    KOAN_EQ_INT(__, sum_vla(10));

    /* sizeof on a VLA is evaluated at runtime, unlike every other sizeof. */
KOAN_DELIBERATE_BEGIN
    size_t n = 7;
    int    v[n];
    KOAN_EQ_SZ(__SZ, sizeof v);
    (void)v;
KOAN_DELIBERATE_END
}

/*
 * A variably-modified parameter lets a function take a genuinely 2D array of
 * any shape. The dimensions must be declared before the array that uses them.
 */
KOAN_DELIBERATE_BEGIN
static int trace(size_t n, int m[n][n])
{
    int total = 0;
    for (size_t i = 0; i < n; i++) total += m[i][i];
    return total;
}
KOAN_DELIBERATE_END

KOAN(variably_modified_types_take_any_shape)
{
    int a[3][3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } };
    int b[2][2] = { { 10, 0 }, { 0, 20 } };

    KOAN_EQ_INT(__, trace(3, a));
    KOAN_EQ_INT(__, trace(2, b));
}

/*
 * A flexible array member is a trailing `[]` that occupies no space until you
 * allocate room for it. It puts the header and its payload in one allocation:
 * one malloc, one free, one cache line.
 */
typedef struct {
    size_t len;
    char   text[];        /* must be last; contributes nothing to sizeof */
} String;

static String *string_new(const char *from)
{
    size_t  n = strlen(from);
    String *s = malloc(sizeof *s + n + 1);      /* header + payload + NUL */
    if (!s) return nullptr;
    s->len = n;
    memcpy(s->text, from, n + 1);
    return s;
}

KOAN(flexible_array_members_pack_header_and_payload)
{
    /* The member is invisible to sizeof. */
    KOAN_EQ_SZ(__SZ, sizeof(String));

    String *s = string_new("koan");
    KOAN_EQ_SZ(__SZ, s->len);
    KOAN_EQ_STR(__STR, s->text);

    /* The payload sits immediately after the header, in the same block. */
    KOAN_EQ_SZ(__SZ,
               (size_t)((char *)s->text - (char *)s));

    free(s);                                    /* one free, not two */
}

/*
 * Bringing it together.
 *
 * A matrix type that owns one flat allocation, carries its own shape, and
 * multiplies. Getting this right needs index arithmetic, ownership, an
 * overflow guard, and a dimension check — the multiply is the easy part.
 *
 * Verify the result yourself before you fill in the blanks:
 *     [1 2]   [5 6]   [19 22]
 *     [3 4] x [7 8] = [43 50]
 */
typedef struct {
    size_t rows, cols;
    int   *cell;                 /* rows*cols ints, row-major */
} Matrix;

static bool mat_init(Matrix *m, size_t rows, size_t cols)
{
    if (rows != 0 && cols > SIZE_MAX / rows) return false;   /* overflow */
    size_t n = rows * cols;
    if (n != 0 && n > SIZE_MAX / sizeof *m->cell) return false;

    m->cell = calloc(n ? n : 1, sizeof *m->cell);
    if (!m->cell) return false;
    m->rows = rows;
    m->cols = cols;
    return true;
}

static void mat_free(Matrix *m) { free(m->cell); *m = (Matrix){ 0 }; }

static int  *mat_at(Matrix *m, size_t r, size_t c) { return &m->cell[r * m->cols + c]; }
static int   mat_get(const Matrix *m, size_t r, size_t c) { return m->cell[r * m->cols + c]; }

/* out = a x b, defined only when a's columns match b's rows. */
static bool mat_mul(Matrix *out, const Matrix *a, const Matrix *b)
{
    if (a->cols != b->rows) return false;
    if (!mat_init(out, a->rows, b->cols)) return false;

    for (size_t i = 0; i < a->rows; i++)
        for (size_t j = 0; j < b->cols; j++) {
            int acc = 0;
            for (size_t k = 0; k < a->cols; k++)
                acc += mat_get(a, i, k) * mat_get(b, k, j);
            *mat_at(out, i, j) = acc;
        }
    return true;
}

KOAN(assembling_a_matrix)
{
    Matrix a, b, product;

    KOAN_TRUE(mat_init(&a, 2, 2));
    KOAN_TRUE(mat_init(&b, 2, 2));

    int va[4] = { 1, 2, 3, 4 };
    int vb[4] = { 5, 6, 7, 8 };
    memcpy(a.cell, va, sizeof va);
    memcpy(b.cell, vb, sizeof vb);

    KOAN_TRUE(mat_mul(&product, &a, &b));
    KOAN_EQ_INT(__, mat_get(&product, 0, 0));
    KOAN_EQ_INT(__, mat_get(&product, 0, 1));
    KOAN_EQ_INT(__, mat_get(&product, 1, 0));
    KOAN_EQ_INT(__, mat_get(&product, 1, 1));

    mat_free(&product);
    mat_free(&b);

    /* Mismatched shapes are refused, not silently mangled. */
    Matrix wide;
    KOAN_TRUE(mat_init(&wide, 3, 5));
    KOAN_EQ_INT(__, mat_mul(&product, &a, &wide));

    mat_free(&wide);
    mat_free(&a);
}

KOAN_LESSON(lesson_about_multidim_arrays, "About Multidimensional Arrays",
    KOAN_CASE(rows_are_contiguous),
    KOAN_CASE(a_2d_array_decays_to_a_pointer_to_a_row),
    KOAN_CASE(flat_buffers_take_any_shape),
    KOAN_CASE(pointer_to_pointer_is_a_different_shape),
    KOAN_CASE(vlas_are_sized_at_runtime),
    KOAN_CASE(variably_modified_types_take_any_shape),
    KOAN_CASE(flexible_array_members_pack_header_and_payload),
    KOAN_CASE(assembling_a_matrix)
);
