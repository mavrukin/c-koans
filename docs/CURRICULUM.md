# Curriculum

The ordered list lives in [`koans/manifest.def`](../koans/manifest.def), which
is the single source the runner reads. This document explains the shape of the
path and records what is written and what is not.

Status as of the current commit: **15 lessons, 151 koans**, plus one capstone.

---

## Design principles

**Order is meaning.** Each lesson assumes every lesson above it and nothing
below. A koan may only use features already taught.

**The last koan in a lesson combines the rest.** By convention it is named
`assembling_*` and builds something genuinely useful — a run-length encoder, a
bounded string builder, a growable array, a vtable, a wire format, a blocking
queue. These are the koans that prove understanding rather than recall.

**Cross-feature pressure increases with depth.** Tier 1 koans test one idea.
By Tier 5, `assembling_a_command_runner` requires `fork`, `exec`, `pipe`,
`dup2`, `waitpid`, file-descriptor hygiene, buffer bounds and error paths at
once — and a mistake in any one of them hangs or crashes.

**Assert what the standard guarantees.** Where C leaves a size or an order
unspecified, the koan asks about the guaranteed *relationship* instead. This
is why `sizeof(long)` never appears as an answer but `sizeof(int32_t)` does.

---

## Written

### Tier 1 · Foundations — 9 lessons, 99 koans

| Lesson | Covers |
|---|---|
| `about_asserts` | how the koans work; typed blanks |
| `about_types` | integer sizes and ranges, `CHAR_BIT`, exact widths, unsigned wrap, signed-overflow prevention, integer promotion, usual arithmetic conversions, truncation, the `size_t` trap |
| `about_operators` | precedence vs. evaluation order, bitwise ops, binary literals, digit separators, shifts and their UB, short-circuit, ternary, comma, increment, compound assignment, `sizeof` not evaluating |
| `about_control_flow` | scalars as conditions, dangling else, loops, `break`/`continue`, `switch` fallthrough, `[[fallthrough]]`, grouped labels, `goto` for unwinding, C23 labels at block end |
| `about_functions` | pass-by-value, out-parameters, `static` at both scopes, shadowing, recursion, `const` parameters, never returning pointers to locals |
| `about_arrays` | sizeof and length, short and designated initialisers, indexing as pointer arithmetic, **decay**, one-past-the-end, bounds discipline |
| `about_pointers` | `&`/`*`, `nullptr`, declarator binding, `const` placement, pointer arithmetic scaling, `void *`, pointer-to-pointer, dangling, `uintptr_t`, linked list via `Node **` |
| `about_strings` | NUL termination, length vs. capacity, read-only literals, `strcmp`, `snprintf` truncation detection, searching, `mem*` vs `str*`, `memmove` overlap |
| `about_structs` | value semantics, designated initialisers, padding and order, `offsetof`, unions, tagged unions, enums, C23 fixed underlying type, bit-fields, explicit wire format |

### Tier 2 · Memory and Composition — 3 lessons, 24 koans

| Lesson | Covers |
|---|---|
| `about_dynamic_memory` | `malloc`/`calloc`/`realloc`/`free`, uninitialised bytes, sizing from the pointer, multiplication-overflow guards, `free(nullptr)`, double free, realloc-may-move, ownership conventions, `goto` cleanup, a growable array |
| `about_function_pointers` | declarator syntax, typedefs, dispatch tables, callbacks, `qsort`/`bsearch`, comparator pitfalls, `void *` user data instead of closures, a vtable |
| `about_preprocessor` | substitution vs. evaluation, double evaluation, `#`/`##`, `do{}while(0)`, `__VA_OPT__`, `#elifdef`, `__has_include`, predefined macros, **X-macros** |

### Tier 3 · Modern C23 — 1 lesson, 11 koans

| Lesson | Covers |
|---|---|
| `about_c23_features` | `bool` as a keyword, `constexpr`, `static_assert` without a message, `auto`, `typeof`/`typeof_unqual`, `[[nodiscard]]`/`[[deprecated]]`/`[[maybe_unused]]`/`[[noreturn]]`, `_BitInt(N)`, `<stdckdint.h>`, `#embed`/`__has_embed`, and a fixed-point type combining all of them |

### Tier 5 · POSIX — 2 lessons, 17 koans

| Lesson | Covers |
|---|---|
| `about_processes` | pids, `fork` returning twice, exit status decoding, signalled children, copy-not-share, pipes, `exec` + `dup2` redirection, `sigaction`, `volatile sig_atomic_t`, blocking and pending signals, a full command runner |
| `about_threads` | `pthread_create`/`join`, happens-before, mutexes, `<stdatomic.h>` and compare-exchange, condition variables and the three rules, `thread_local`, C11 `<threads.h>` via the shim, a bounded blocking queue |

### Tier 6 · Capstone

`projects/webserver` — a working HTTP/1.1 server. Thread pool over a bounded
queue, static file serving with content types, CGI via fork/exec/dup2/pipe,
percent-decoding, `realpath`-based traversal defence, `SIGPIPE` handling and
graceful shutdown on `SIGINT`/`SIGTERM`.

---

## Planned

These are described in the README's roadmap and are not yet written. Each is a
self-contained contribution; see [CONTRIBUTING.md](CONTRIBUTING.md).

**Tier 2** — multidimensional arrays, VLAs and flexible array members ·
translation units, headers and linkage · file I/O (`fopen`, `fread`, `fseek`,
text vs. binary) · `errno`, `perror`, `assert`, `setjmp`/`longjmp`

**Tier 3** — `_Generic` and type-generic macros · `<stdbit.h>` (a shim already
exists in `include/compat.h`) · `alignas`/`alignof`, `restrict`, `volatile` ·
floating point, IEEE 754, `<float.h>`, NaN and epsilon · compound literals

**Tier 4** — `<stdlib.h>` algorithms and conversions · the `str`/`mem` families
in depth, `strtok` · streams and buffering, `scanf` · `<time.h>` ·
`<stdarg.h>` · wide characters, `<uchar.h>` and UTF-8 · atomics and memory
ordering in depth

**Tier 5** — low-level `open`/`read`/`write`/`lseek` and `stat` · directories ·
TCP sockets and `getaddrinfo` · `poll`, non-blocking I/O and event loops

**Tier 6** — arena allocator · generic containers with `_Generic` · JSON parser
