/*
 * json.h — a complete JSON parser.
 *
 * Every value is allocated from an arena, so the whole document is freed in
 * one call and no node needs an owner. That is the arena's entire argument,
 * demonstrated: a recursive tree structure with zero per-node cleanup.
 *
 * Techniques from: about_structs (tagged unions), about_pointers (recursive
 * structures), about_reading_input (a parser that reports where it failed),
 * about_unicode (\u escapes to UTF-8), about_preprocessor (X-macro types),
 * and the arena capstone next door.
 */
#ifndef KOAN_JSON_H
#define KOAN_JSON_H

#include <stddef.h>

#include "../arena/arena.h"

/* One X-macro list generating the enum and its names (about_preprocessor). */
#define JSON_TYPES        \
    X(JSON_NULL,   "null")   \
    X(JSON_BOOL,   "bool")   \
    X(JSON_NUMBER, "number") \
    X(JSON_STRING, "string") \
    X(JSON_ARRAY,  "array")  \
    X(JSON_OBJECT, "object")

#define X(name, text) name,
typedef enum { JSON_TYPES JSON_TYPE_COUNT } JsonType;
#undef X

const char *json_type_name(JsonType t);

typedef struct JsonValue JsonValue;

/* A member of an object, and a link in its list. */
typedef struct JsonMember {
    const char        *key;
    JsonValue         *value;
    struct JsonMember *next;
} JsonMember;

/*
 * A tagged union (about_structs). Arrays and objects are singly linked lists
 * rather than arrays, because an arena cannot realloc — and a list needs no
 * resizing at all.
 */
struct JsonValue {
    JsonType kind;
    union {
        bool        boolean;
        double      number;
        const char *string;
        struct { JsonValue  *first; size_t count; } array;
        struct { JsonMember *first; size_t count; } object;
    };
    JsonValue *next;          /* the next element, when inside an array */
};

typedef struct {
    const char *message;      /* null when the parse succeeded */
    size_t      offset;       /* byte offset where it went wrong */
    int         line;
    int         column;
} JsonError;

/*
 * Parse `text` into the arena. Returns null on failure and fills in `err`.
 * The result lives exactly as long as the arena does.
 */
JsonValue *json_parse(Arena *a, const char *text, JsonError *err);

/* Lookups. Both return null rather than failing when the shape is wrong. */
JsonValue *json_member(const JsonValue *object, const char *key);
JsonValue *json_index(const JsonValue *array, size_t i);

/* Typed accessors with a fallback, so a caller need not check twice. */
double      json_number_or(const JsonValue *v, double fallback);
const char *json_string_or(const JsonValue *v, const char *fallback);
bool        json_bool_or(const JsonValue *v, bool fallback);

/* Serialise back to text, into the arena. `indent` of 0 means compact. */
char *json_write(Arena *a, const JsonValue *v, int indent);

#endif /* KOAN_JSON_H */
