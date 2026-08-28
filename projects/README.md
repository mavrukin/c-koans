# Capstone projects

Four complete, working programs. Not exercises with the middle removed — these
are the finished article, and reading them is the point.

Every technique in them came from a koan you solved. The header comment of
each file names which.

```sh
make            # build everything
make check      # 701 self-checks across the three libraries
make san        # the same checks under AddressSanitizer + UBSan
make run        # start the web server on http://127.0.0.1:8080/
```

---

## `arena/` — a region allocator

Hands out memory from large blocks and frees everything at once. That single
decision removes the hardest problem in C: there is no per-object free, so
there is nothing to double-free, nothing to leak, and no ownership question to
get wrong.

```c
Arena a;
arena_init(&a, 0);

Config *c = ARENA_NEW(&a, Config);
c->name   = arena_strdup(&a, name);
c->label  = arena_printf(&a, "%s:%d", host, port);

arena_free(&a);        /* all of it, in one call */
```

Use it whenever allocations share a lifetime — one request, one frame, one
parse. The trade is that you cannot free a single object; if you need that,
you need `malloc`.

**Read it for:** alignment arithmetic, flexible array members, and why
`arena_reset` keeps exactly one block.

## `containers/` — Vec and Map

C has no templates, so a generic container is written one of two ways. This
library uses both, side by side, so you can compare them:

```c
Vec v;                          /* void* + element size: one implementation */
vec_init(&v, sizeof(int));
vec_push(&v, &value);
int x = *(int *)vec_at(&v, 0);

VEC_DECLARE(IntVec, int_vec, int);   /* macros: a struct per type, no casts */
VEC_DEFINE(IntVec, int_vec, int)
int_vec_push(&t, 42);
int y = int_vec_at(&t, 0);
```

`Map` is a string-keyed hash table using open addressing with linear probing.
The interesting part is deletion: an emptied slot would cut short a probe that
had walked past it, so removal leaves a **tombstone** instead.

**Read it for:** the two generic-container idioms, and a hash table whose
deletion is actually correct.

## `json/` — a recursive-descent parser

One function per grammar rule, each consuming its own shape and calling the
others — the parsing technique that reads most like the specification.

```c
Arena     a;  arena_init(&a, 0);
JsonError err;

JsonValue *doc = json_parse(&a, text, &err);
if (!doc) fprintf(stderr, "line %d col %d: %s\n",
                  err.line, err.column, err.message);

const char *name = json_string_or(json_member(doc, "name"), "(none)");
arena_free(&a);        /* every node, in one call */
```

Handles the whole format: strict number grammar (no leading zeros, no hex),
all escapes including surrogate pairs decoded to UTF-8, a depth limit so
nested input cannot overflow the stack, and a serialiser that round-trips.

**Read it for:** how a real parser reports *where* it failed, and why an arena
suits a recursive tree so well.

## `webserver/` — koanhttpd

A genuine HTTP/1.1 server. It listens on a TCP socket, parses requests, serves
static files, runs CGI programs, and shuts down cleanly on `SIGINT`.

```sh
make run
curl -v http://127.0.0.1:8080/
curl http://127.0.0.1:8080/cgi-bin/env.cgi?name=koan
```

Inside: a thread pool over a bounded blocking queue, percent-decoding,
`realpath`-based path-traversal defence, `SIGPIPE` handling, and CGI via
fork/exec/dup2/pipe.

**Read it for:** how the whole path assembles into one program — the X-macro
status table, the bounded string handling, the ownership discipline, the
fork/exec plumbing, and the thread pool, all doing real work at once.

> **Security note.** This is a teaching server. It binds to `127.0.0.1` only,
> refuses traversal, and bounds every buffer — but it has not been audited and
> should not face the public internet.
