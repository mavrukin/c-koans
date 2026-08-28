/*
 * About Structs, Unions and Enums
 *
 * These are C's three ways of building a compound type, and they mean three
 * different things:
 *
 *   struct — all of these, side by side
 *   union  — exactly one of these, sharing storage
 *   enum   — a name for an integer constant
 *
 * The interesting parts are padding (which decides how big a struct really
 * is), and the fact that a union will not tell you which member is live.
 */
#include "koan.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    int  x;
    int  y;
} Point;

/*
 * Struct members are copied by value on assignment, unlike arrays. A struct
 * is a first-class value: you may assign it, pass it, and return it.
 */
KOAN(structs_are_values)
{
    Point a = { .x = 1, .y = 2 };
    Point b = a;                     /* a genuine copy */

    b.x = 99;
    KOAN_EQ_INT(/*__*/ 1, a.x);
    KOAN_EQ_INT(/*__*/ 99, b.x);
}

/*
 * Designated initialisers name the members, so order does not matter and
 * anything omitted is zeroed. Prefer them: they survive a member being added.
 */
KOAN(designated_initialisers_name_members)
{
    Point p = { .y = 5 };

    KOAN_EQ_INT(/*__*/ 0, p.x);
    KOAN_EQ_INT(/*__*/ 5, p.y);

    /* C23's empty initialiser zeroes the whole struct. */
    Point zero = {};
    KOAN_EQ_INT(/*__*/ 0, zero.x + zero.y);
}

/*
 * Access is `.` through a value and `->` through a pointer. The second is
 * shorthand: p->x means (*p).x.
 */
KOAN(arrow_dereferences_then_selects)
{
    Point  p = { .x = 3, .y = 4 };
    Point *q = &p;

    KOAN_EQ_INT(/*__*/ 3, q->x);
    KOAN_EQ_INT(/*__*/ 3, (*q).x);

    q->y = 40;
    KOAN_EQ_INT(/*__*/ 40, p.y);
}

/*
 * A compiler inserts padding so each member lands on a suitable alignment.
 * The size of a struct is therefore not the sum of its members, and it
 * depends on the *order* you declare them in.
 */
typedef struct { char a; int b; char c; } Loose;   /* padded twice */
typedef struct { int b; char a; char c; } Tight;   /* padded once  */

KOAN(padding_makes_order_matter)
{
    KOAN_TRUE(sizeof(Loose) > sizeof(Tight));

    /* On a typical machine where int is 4 and requires 4-byte alignment: */
    KOAN_EQ_SZ(/*__SZ*/ 12, sizeof(Loose));
    KOAN_EQ_SZ(/*__SZ*/ 8, sizeof(Tight));

    /* The members themselves account for only six bytes. */
    KOAN_EQ_SZ(/*__SZ*/ 6, sizeof(char) + sizeof(int) + sizeof(char));
}

/*
 * offsetof reports where a member actually sits, padding included. Never
 * guess an offset; ask.
 */
KOAN(offsetof_reports_real_layout)
{
    KOAN_EQ_SZ(/*__SZ*/ 0, offsetof(Loose, a));
    KOAN_EQ_SZ(/*__SZ*/ 4, offsetof(Loose, b));
    KOAN_EQ_SZ(/*__SZ*/ 8, offsetof(Loose, c));

    /* Because of padding, comparing structs with memcmp is unreliable: the
     * padding bytes hold whatever was there before. Compare member by
     * member, or zero the struct first. */
    Loose one = { .a = 1, .b = 2, .c = 3 };
    Loose two;
    memset(&two, 0, sizeof two);
    two.a = 1; two.b = 2; two.c = 3;

    KOAN_TRUE(one.a == two.a && one.b == two.b && one.c == two.c);
}

/*
 * A union overlays its members in one block of storage, sized for the
 * largest. Writing one member and reading another is "type punning", and in
 * C — unlike C++ — it is permitted, with the caveat that you get whatever
 * the representation happens to be.
 */
typedef union {
    uint32_t whole;
    uint8_t  bytes[4];
} Word;

KOAN(unions_share_one_block_of_storage)
{
    KOAN_EQ_SZ(/*__SZ*/ 4, sizeof(Word));

    Word w = { .whole = 0 };
    w.bytes[0] = 0xFF;

    /* Which byte moved depends on endianness, so assert what is portable:
     * exactly one byte is set, and the value is no longer zero. */
    KOAN_TRUE(w.whole != 0);

    int set_bytes = 0;
    for (int i = 0; i < 4; i++) if (w.bytes[i] != 0) set_bytes++;
    KOAN_EQ_INT(/*__*/ 1, set_bytes);
}

/*
 * A union does not record which member is live. The standard idiom is a
 * "tagged union": a struct pairing an enum tag with the union, where the tag
 * is the only thing that tells you which member may legally be read.
 */
typedef enum { VAL_INT, VAL_DOUBLE, VAL_TEXT } ValueKind;

typedef struct {
    ValueKind kind;
    union {
        int         as_int;
        double      as_double;
        const char *as_text;
    };                      /* anonymous: members are reached directly */
} Value;

static const char *describe(Value v, char *out, size_t cap)
{
    switch (v.kind) {
        case VAL_INT:    snprintf(out, cap, "int:%d", v.as_int);      break;
        case VAL_DOUBLE: snprintf(out, cap, "double:%.1f", v.as_double); break;
        case VAL_TEXT:   snprintf(out, cap, "text:%s", v.as_text);    break;
    }
    return out;
}

KOAN(tagged_unions_remember_which_member_is_live)
{
    char buf[64];

    Value i = { .kind = VAL_INT,    .as_int = 42 };
    Value d = { .kind = VAL_DOUBLE, .as_double = 1.5 };
    Value t = { .kind = VAL_TEXT,   .as_text = "hi" };

    KOAN_EQ_STR(/*__STR*/ "int:42", describe(i, buf, sizeof buf));
    KOAN_EQ_STR(/*__STR*/ "double:1.5", describe(d, buf, sizeof buf));
    KOAN_EQ_STR(/*__STR*/ "text:hi", describe(t, buf, sizeof buf));

    /* An anonymous union's members are named as though they were the
     * struct's own, which is what makes this pattern readable. */
    KOAN_EQ_INT(/*__*/ 42, i.as_int);
}

/*
 * Enumerators are integer constants, numbered from zero and incrementing
 * unless you say otherwise. Assigning one explicitly moves the count.
 */
enum Status { STATUS_OK, STATUS_WARN, STATUS_ERROR = 10, STATUS_FATAL };

KOAN(enumerators_count_upward_unless_told_otherwise)
{
    KOAN_EQ_INT(/*__*/ 0, STATUS_OK);
    KOAN_EQ_INT(/*__*/ 1, STATUS_WARN);
    KOAN_EQ_INT(/*__*/ 10, STATUS_ERROR);
    KOAN_EQ_INT(/*__*/ 11, STATUS_FATAL);
}

/*
 * C23 lets you fix an enum's underlying type. Before this, the type was
 * implementation-defined, which made enums unusable in a struct whose layout
 * had to match a wire format. Now it is nailed down.
 */
enum Small : unsigned char { SMALL_A = 1, SMALL_B = 200 };

KOAN(c23_enums_can_fix_their_underlying_type)
{
    KOAN_EQ_SZ(/*__SZ*/ 1, sizeof(enum Small));
    KOAN_EQ_INT(/*__*/ 200, SMALL_B);

    /* Which makes this struct exactly two bytes, with no guesswork. */
    struct Packed { enum Small a; unsigned char b; };
    KOAN_EQ_SZ(/*__SZ*/ 2, sizeof(struct Packed));
}

/*
 * Bit-fields pack several small values into one storage unit. They are the
 * readable way to describe flags and hardware registers — but the standard
 * leaves the bit *order* implementation-defined, so never use them to parse
 * an external format. Shift and mask for that.
 */
typedef struct {
    unsigned version : 4;
    unsigned flags   : 4;
    unsigned length  : 8;
} Header;

KOAN(bitfields_pack_small_values)
{
    Header h = { .version = 3, .flags = 0b1010, .length = 255 };

    KOAN_EQ_INT(/*__*/ 3, h.version);
    KOAN_EQ_INT(/*__*/ 10, h.flags);
    KOAN_EQ_INT(/*__*/ 255, h.length);

    /* Sixteen bits of payload fit inside a single unsigned. */
    KOAN_EQ_SZ(/*__SZ*/ sizeof(unsigned), sizeof(Header));

    /* Assigning more than the field can hold silently truncates. */
KOAN_DELIBERATE_BEGIN
    h.version = 0b10011;                 /* five bits into a four-bit field */
KOAN_DELIBERATE_END
    KOAN_EQ_INT(/*__*/ 3, h.version);
}

/*
 * Bringing it together.
 *
 * Pack and unpack a header into a wire format explicitly, with shifts and
 * masks, rather than trusting bit-field layout. This is what real protocol
 * code does, and it is portable because it commits to an order.
 *
 * Layout, most significant bit first:
 *   [ version : 4 ][ flags : 4 ][ length : 8 ]
 */
static uint16_t pack_header(Header h)
{
    return (uint16_t)(((h.version & 0xF) << 12) |
                      ((h.flags   & 0xF) <<  8) |
                       (h.length  & 0xFF));
}

static Header unpack_header(uint16_t wire)
{
    return (Header){
        .version = (wire >> 12) & 0xF,
        .flags   = (wire >>  8) & 0xF,
        .length  =  wire        & 0xFF,
    };
}

KOAN(assembling_a_wire_format)
{
    Header h = { .version = 4, .flags = 0b0011, .length = 200 };

    uint16_t wire = pack_header(h);
    KOAN_EQ_INT(/*__*/ 0x43C8, wire);

    Header back = unpack_header(wire);
    KOAN_EQ_INT(/*__*/ 4, back.version);
    KOAN_EQ_INT(/*__*/ 3, back.flags);
    KOAN_EQ_INT(/*__*/ 200, back.length);

    /* A round trip must be lossless — the real test of a serialiser. */
    KOAN_EQ_INT(/*__*/ 1, pack_header(unpack_header(wire)) == wire);
}

KOAN_LESSON(lesson_about_structs, "About Structs, Unions and Enums",
    KOAN_CASE(structs_are_values),
    KOAN_CASE(designated_initialisers_name_members),
    KOAN_CASE(arrow_dereferences_then_selects),
    KOAN_CASE(padding_makes_order_matter),
    KOAN_CASE(offsetof_reports_real_layout),
    KOAN_CASE(unions_share_one_block_of_storage),
    KOAN_CASE(tagged_unions_remember_which_member_is_live),
    KOAN_CASE(enumerators_count_upward_unless_told_otherwise),
    KOAN_CASE(c23_enums_can_fix_their_underlying_type),
    KOAN_CASE(bitfields_pack_small_values),
    KOAN_CASE(assembling_a_wire_format)
);
