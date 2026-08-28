/*
 * json.c — a recursive-descent JSON parser.
 *
 * Recursive descent means one function per grammar rule, each consuming its
 * own shape and calling the others. It is the parsing technique that reads
 * most like the specification, and JSON's grammar is small enough to fit on
 * a page:
 *
 *   value   = null | true | false | number | string | array | object
 *   array   = '[' (value (',' value)*)? ']'
 *   object  = '{' (string ':' value (',' string ':' value)*)? '}'
 *
 * The parser carries a cursor and an error. Every function returns null on
 * failure, and the first failure wins — no error is ever overwritten by a
 * later one.
 */
#include "json.h"

#include <math.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define X(name, text) text,
static const char *const TYPE_NAMES[] = { JSON_TYPES };
#undef X

static_assert(sizeof TYPE_NAMES / sizeof *TYPE_NAMES == JSON_TYPE_COUNT,
              "every JsonType needs a name");

const char *json_type_name(JsonType t)
{
    return (t >= 0 && t < JSON_TYPE_COUNT) ? TYPE_NAMES[t] : "?";
}

/* ------------------------------------------------------------------ */
/* The parser state                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *text;
    size_t      pos;
    size_t      len;
    Arena      *arena;
    JsonError  *err;
    int         depth;
} Parser;

constexpr int MAX_DEPTH = 200;   /* refuse to blow the stack on nested input */

static void fail(Parser *p, const char *message)
{
    if (p->err->message) return;      /* keep the first failure */

    p->err->message = message;
    p->err->offset  = p->pos;

    /* Recompute line and column from the start; parsing has stopped anyway. */
    int line = 1, column = 1;
    for (size_t i = 0; i < p->pos && i < p->len; i++) {
        if (p->text[i] == '\n') { line++; column = 1; }
        else                      column++;
    }
    p->err->line   = line;
    p->err->column = column;
}

static bool at_end(const Parser *p) { return p->pos >= p->len; }
static char peek(const Parser *p)   { return at_end(p) ? '\0' : p->text[p->pos]; }

static void skip_whitespace(Parser *p)
{
    while (!at_end(p)) {
        char c = p->text[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static bool consume(Parser *p, char c)
{
    if (peek(p) != c) return false;
    p->pos++;
    return true;
}

static JsonValue *new_value(Parser *p, JsonType kind)
{
    JsonValue *v = ARENA_NEW(p->arena, JsonValue);
    if (!v) { fail(p, "out of memory"); return nullptr; }
    *v = (JsonValue){ .kind = kind };
    return v;
}

static JsonValue *parse_value(Parser *p);

/* ------------------------------------------------------------------ */
/* Literals and numbers                                                */
/* ------------------------------------------------------------------ */

static JsonValue *parse_literal(Parser *p, const char *word, JsonType kind,
                                bool boolean)
{
    size_t n = strlen(word);
    if (p->len - p->pos < n || memcmp(p->text + p->pos, word, n) != 0) {
        fail(p, "unexpected token");
        return nullptr;
    }
    p->pos += n;

    JsonValue *v = new_value(p, kind);
    if (v && kind == JSON_BOOL) v->boolean = boolean;
    return v;
}

/*
 * JSON numbers are a strict subset of what strtod accepts: no hex, no inf,
 * no nan, no leading plus, no leading zeros. Validate the shape first, then
 * let strtod do the arithmetic.
 */
static JsonValue *parse_number(Parser *p)
{
    size_t start = p->pos;

    if (peek(p) == '-') p->pos++;

    if (!at_end(p) && peek(p) == '0') {
        p->pos++;
    } else if (!at_end(p) && peek(p) >= '1' && peek(p) <= '9') {
        while (!at_end(p) && peek(p) >= '0' && peek(p) <= '9') p->pos++;
    } else {
        fail(p, "expected a digit");
        return nullptr;
    }

    if (peek(p) == '.') {
        p->pos++;
        if (!(peek(p) >= '0' && peek(p) <= '9')) {
            fail(p, "expected a digit after the decimal point");
            return nullptr;
        }
        while (!at_end(p) && peek(p) >= '0' && peek(p) <= '9') p->pos++;
    }

    if (peek(p) == 'e' || peek(p) == 'E') {
        p->pos++;
        if (peek(p) == '+' || peek(p) == '-') p->pos++;
        if (!(peek(p) >= '0' && peek(p) <= '9')) {
            fail(p, "expected a digit in the exponent");
            return nullptr;
        }
        while (!at_end(p) && peek(p) >= '0' && peek(p) <= '9') p->pos++;
    }

    char   scratch[64];
    size_t n = p->pos - start;
    if (n >= sizeof scratch) { fail(p, "number too long"); return nullptr; }
    memcpy(scratch, p->text + start, n);
    scratch[n] = '\0';

    JsonValue *v = new_value(p, JSON_NUMBER);
    if (v) v->number = strtod(scratch, nullptr);
    return v;
}

/* ------------------------------------------------------------------ */
/* Strings                                                             */
/* ------------------------------------------------------------------ */

/* Encode a code point as UTF-8 (about_unicode). Returns bytes written. */
static size_t encode_utf8(uint32_t cp, char *out)
{
    if (cp <= 0x7F)   { out[0] = (char)cp; return 1; }
    if (cp <= 0x7FF)  {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static bool read_hex4(Parser *p, uint32_t *out)
{
    if (p->len - p->pos < 4) return false;

    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p->text[p->pos + i];
        int  d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        v = v * 16 + (uint32_t)d;
    }
    p->pos += 4;
    *out = v;
    return true;
}

/*
 * Strings are decoded into the arena. The decoded form is never longer than
 * the source, so one pass with a worst-case buffer is enough — no growth and
 * no second scan.
 */
static const char *parse_raw_string(Parser *p)
{
    if (!consume(p, '"')) { fail(p, "expected a string"); return nullptr; }

    size_t start = p->pos;
    size_t worst = p->len - start + 1;
    char  *out = arena_alloc(p->arena, worst, 1);
    if (!out) { fail(p, "out of memory"); return nullptr; }

    size_t w = 0;
    while (!at_end(p)) {
        char c = p->text[p->pos];

        if (c == '"') { p->pos++; out[w] = '\0'; return out; }

        if ((unsigned char)c < 0x20) {
            fail(p, "control character in string");
            return nullptr;
        }

        if (c != '\\') { out[w++] = c; p->pos++; continue; }

        p->pos++;                                   /* the backslash */
        if (at_end(p)) break;

        char esc = p->text[p->pos++];
        switch (esc) {
            case '"':  out[w++] = '"';  break;
            case '\\': out[w++] = '\\'; break;
            case '/':  out[w++] = '/';  break;
            case 'b':  out[w++] = '\b'; break;
            case 'f':  out[w++] = '\f'; break;
            case 'n':  out[w++] = '\n'; break;
            case 'r':  out[w++] = '\r'; break;
            case 't':  out[w++] = '\t'; break;
            case 'u': {
                uint32_t cp;
                if (!read_hex4(p, &cp)) {
                    fail(p, "malformed \\u escape");
                    return nullptr;
                }
                /* A surrogate pair encodes one code point above U+FFFF. */
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    uint32_t low;
                    if (p->len - p->pos >= 2 && p->text[p->pos] == '\\' &&
                        p->text[p->pos + 1] == 'u') {
                        p->pos += 2;
                        if (!read_hex4(p, &low)) {
                            fail(p, "malformed \\u escape");
                            return nullptr;
                        }
                        if (low < 0xDC00 || low > 0xDFFF) {
                            fail(p, "invalid low surrogate");
                            return nullptr;
                        }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else {
                        fail(p, "lone high surrogate");
                        return nullptr;
                    }
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail(p, "lone low surrogate");
                    return nullptr;
                }
                w += encode_utf8(cp, out + w);
                break;
            }
            default:
                fail(p, "unknown escape");
                return nullptr;
        }
    }

    fail(p, "unterminated string");
    return nullptr;
}

static JsonValue *parse_string(Parser *p)
{
    const char *text = parse_raw_string(p);
    if (!text) return nullptr;

    JsonValue *v = new_value(p, JSON_STRING);
    if (v) v->string = text;
    return v;
}

/* ------------------------------------------------------------------ */
/* Arrays and objects                                                  */
/* ------------------------------------------------------------------ */

static JsonValue *parse_array(Parser *p)
{
    if (!consume(p, '[')) { fail(p, "expected ["); return nullptr; }

    JsonValue *v = new_value(p, JSON_ARRAY);
    if (!v) return nullptr;

    skip_whitespace(p);
    if (consume(p, ']')) return v;                  /* the empty array */

    JsonValue *tail = nullptr;
    for (;;) {
        JsonValue *elem = parse_value(p);
        if (!elem) return nullptr;

        /* Append, keeping source order — a list, so no reallocation. */
        if (tail) tail->next = elem;
        else      v->array.first = elem;
        tail = elem;
        v->array.count++;

        skip_whitespace(p);
        if (consume(p, ',')) { skip_whitespace(p); continue; }
        if (consume(p, ']')) return v;

        fail(p, "expected , or ] in array");
        return nullptr;
    }
}

static JsonValue *parse_object(Parser *p)
{
    if (!consume(p, '{')) { fail(p, "expected {"); return nullptr; }

    JsonValue *v = new_value(p, JSON_OBJECT);
    if (!v) return nullptr;

    skip_whitespace(p);
    if (consume(p, '}')) return v;                  /* the empty object */

    JsonMember *tail = nullptr;
    for (;;) {
        skip_whitespace(p);

        const char *key = parse_raw_string(p);
        if (!key) return nullptr;

        skip_whitespace(p);
        if (!consume(p, ':')) { fail(p, "expected : after key"); return nullptr; }

        JsonValue *value = parse_value(p);
        if (!value) return nullptr;

        JsonMember *m = ARENA_NEW(p->arena, JsonMember);
        if (!m) { fail(p, "out of memory"); return nullptr; }
        *m = (JsonMember){ .key = key, .value = value };

        if (tail) tail->next = m;
        else      v->object.first = m;
        tail = m;
        v->object.count++;

        skip_whitespace(p);
        if (consume(p, ',')) continue;
        if (consume(p, '}')) return v;

        fail(p, "expected , or } in object");
        return nullptr;
    }
}

static JsonValue *parse_value(Parser *p)
{
    if (++p->depth > MAX_DEPTH) {
        fail(p, "nesting too deep");
        p->depth--;
        return nullptr;
    }

    skip_whitespace(p);

    JsonValue *v;
    switch (peek(p)) {
        case '{':  v = parse_object(p); break;
        case '[':  v = parse_array(p);  break;
        case '"':  v = parse_string(p); break;
        case 't':  v = parse_literal(p, "true",  JSON_BOOL, true);  break;
        case 'f':  v = parse_literal(p, "false", JSON_BOOL, false); break;
        case 'n':  v = parse_literal(p, "null",  JSON_NULL, false); break;
        case '\0': fail(p, "unexpected end of input"); v = nullptr; break;
        default:   v = parse_number(p); break;
    }

    p->depth--;
    return v;
}

JsonValue *json_parse(Arena *a, const char *text, JsonError *err)
{
    JsonError local = { 0 };
    if (!err) err = &local;
    *err = (JsonError){ 0 };

    Parser p = { .text = text, .len = strlen(text), .arena = a, .err = err };

    JsonValue *v = parse_value(&p);
    if (!v) return nullptr;

    /* Trailing content is an error: "1 2" is not one document. */
    skip_whitespace(&p);
    if (!at_end(&p)) {
        fail(&p, "trailing content after value");
        return nullptr;
    }
    return v;
}

/* ------------------------------------------------------------------ */
/* Access                                                              */
/* ------------------------------------------------------------------ */

JsonValue *json_member(const JsonValue *object, const char *key)
{
    if (!object || object->kind != JSON_OBJECT) return nullptr;

    for (JsonMember *m = object->object.first; m; m = m->next)
        if (strcmp(m->key, key) == 0) return m->value;
    return nullptr;
}

JsonValue *json_index(const JsonValue *array, size_t i)
{
    if (!array || array->kind != JSON_ARRAY) return nullptr;

    JsonValue *v = array->array.first;
    while (v && i--) v = v->next;
    return v;
}

double json_number_or(const JsonValue *v, double fallback)
{
    return (v && v->kind == JSON_NUMBER) ? v->number : fallback;
}

const char *json_string_or(const JsonValue *v, const char *fallback)
{
    return (v && v->kind == JSON_STRING) ? v->string : fallback;
}

bool json_bool_or(const JsonValue *v, bool fallback)
{
    return (v && v->kind == JSON_BOOL) ? v->boolean : fallback;
}

/* ------------------------------------------------------------------ */
/* Serialising                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
    bool   ok;
} Writer;

static void w_ensure(Writer *w, size_t extra)
{
    if (!w->ok) return;
    if (w->len + extra + 1 <= w->cap) return;

    size_t next = w->cap ? w->cap : 256;
    while (next < w->len + extra + 1) {
        if (next > SIZE_MAX / 2) { w->ok = false; return; }
        next *= 2;
    }
    char *grown = realloc(w->buf, next);
    if (!grown) { w->ok = false; return; }
    w->buf = grown;
    w->cap = next;
}

static void w_puts(Writer *w, const char *s)
{
    size_t n = strlen(s);
    w_ensure(w, n);
    if (!w->ok) return;
    memcpy(w->buf + w->len, s, n);
    w->len += n;
    w->buf[w->len] = '\0';
}

static void w_char(Writer *w, char c)
{
    w_ensure(w, 1);
    if (!w->ok) return;
    w->buf[w->len++] = c;
    w->buf[w->len] = '\0';
}

static void w_indent(Writer *w, int indent, int depth)
{
    if (indent <= 0) return;
    w_char(w, '\n');
    for (int i = 0; i < indent * depth; i++) w_char(w, ' ');
}

/* Escape exactly what JSON requires, and nothing else. */
static void w_string(Writer *w, const char *s)
{
    w_char(w, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
            case '"':  w_puts(w, "\\\""); break;
            case '\\': w_puts(w, "\\\\"); break;
            case '\b': w_puts(w, "\\b");  break;
            case '\f': w_puts(w, "\\f");  break;
            case '\n': w_puts(w, "\\n");  break;
            case '\r': w_puts(w, "\\r");  break;
            case '\t': w_puts(w, "\\t");  break;
            default:
                if (*p < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof esc, "\\u%04x", *p);
                    w_puts(w, esc);
                } else {
                    w_char(w, (char)*p);   /* UTF-8 passes through unchanged */
                }
        }
    }
    w_char(w, '"');
}

static void w_value(Writer *w, const JsonValue *v, int indent, int depth)
{
    if (!w->ok || !v) return;

    switch (v->kind) {
        case JSON_NULL:   w_puts(w, "null"); break;
        case JSON_BOOL:   w_puts(w, v->boolean ? "true" : "false"); break;
        case JSON_STRING: w_string(w, v->string); break;

        case JSON_NUMBER: {
            char num[40];
            /* Print integers without a decimal point, as JSON conventionally
             * does, and everything else with enough digits to round-trip. */
            if (v->number == (double)(long long)v->number &&
                v->number > -1e15 && v->number < 1e15)
                snprintf(num, sizeof num, "%lld", (long long)v->number);
            else
                snprintf(num, sizeof num, "%.17g", v->number);
            w_puts(w, num);
            break;
        }

        case JSON_ARRAY: {
            if (v->array.count == 0) { w_puts(w, "[]"); break; }
            w_char(w, '[');
            size_t i = 0;
            for (JsonValue *e = v->array.first; e; e = e->next, i++) {
                if (i) w_char(w, ',');
                w_indent(w, indent, depth + 1);
                w_value(w, e, indent, depth + 1);
            }
            w_indent(w, indent, depth);
            w_char(w, ']');
            break;
        }

        case JSON_OBJECT: {
            if (v->object.count == 0) { w_puts(w, "{}"); break; }
            w_char(w, '{');
            size_t i = 0;
            for (JsonMember *m = v->object.first; m; m = m->next, i++) {
                if (i) w_char(w, ',');
                w_indent(w, indent, depth + 1);
                w_string(w, m->key);
                w_char(w, ':');
                if (indent > 0) w_char(w, ' ');
                w_value(w, m->value, indent, depth + 1);
            }
            w_indent(w, indent, depth);
            w_char(w, '}');
            break;
        }

        default: w->ok = false; break;
    }
}

char *json_write(Arena *a, const JsonValue *v, int indent)
{
    Writer w = { .ok = true };
    w_value(&w, v, indent, 0);

    char *result = nullptr;
    if (w.ok && w.buf) {
        result = arena_alloc(a, w.len + 1, 1);
        if (result) memcpy(result, w.buf, w.len + 1);
    }
    free(w.buf);          /* the Writer grows with realloc; the arena cannot */
    return result;
}
