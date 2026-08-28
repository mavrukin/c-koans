# Curriculum

The ordered list lives in [`koans/manifest.def`](../koans/manifest.def), which
is the single source the runner reads. This document explains the shape of the
path and what each lesson covers.

**38 lessons, 338 koans, 4 capstone projects.**

---

## Design principles

**Order is meaning.** Each lesson assumes every lesson above it and nothing
below. A koan may only use features already taught.

**The last koan in a lesson combines the rest.** By convention it is named
`assembling_*` and builds something genuinely useful — a run-length encoder, a
bounded string builder, a growable array, a vtable, a wire format, a matrix, a
bitset, a Kahan summation, a miniature printf, a UTF-8-safe truncation, a
lock-free stack, a command runner, a blocking queue, a file copier, a
recursive directory walk, a request/response server, an event loop. These
prove understanding rather than recall.

**Cross-feature pressure increases with depth.** Tier 1 koans test one idea.
By Tier 5, `assembling_a_command_runner` needs `fork`, `exec`, `pipe`, `dup2`,
`waitpid`, file-descriptor hygiene, buffer bounds and error paths at once —
and a mistake in any one hangs or crashes.

**Assert what the standard guarantees.** Where C leaves a size or an order
unspecified, the koan asks about the guaranteed *relationship* instead. This
is why `sizeof(long)` never appears as an answer but `sizeof(int32_t)` does,
and why the endianness koans assert only what is portable.

**Prose only where it earns its place.** A koan whose answer is obvious gets
no comment. A koan about integer promotion, `restrict`, or why a failed
`scanf` consumes nothing gets exactly as much as the idea needs.

---

## Tier 1 · Foundations and the Toolchain — 12 lessons, 125 koans

| Lesson | Covers |
|---|---|
| `about_asserts` | how the koans work; typed blanks |
| `about_compiling` | the four stages; why a missing semicolon is a compile error and a missing body a link error; headers as textual inclusion; include guards; conditional compilation; `-std`. Calls across a real translation-unit boundary |
| `about_types` | integer sizes and ranges, `CHAR_BIT`, exact widths, unsigned wrap, signed-overflow prevention, integer promotion, usual arithmetic conversions, truncation, the `size_t` trap |
| `about_operators` | precedence vs. evaluation order, bitwise ops, binary literals, digit separators, shift UB, short-circuit, ternary, comma, increment, compound assignment, `sizeof` not evaluating |
| `about_control_flow` | scalars as conditions, dangling else, loops, `break`/`continue`, `switch` fallthrough, `[[fallthrough]]`, grouped labels, `goto` for unwinding, C23 labels at block end |
| `about_functions` | pass-by-value, out-parameters, `static` at both scopes, shadowing, recursion, `const` parameters, never returning pointers to locals |
| `about_printf` | the conversion grammar; flags, width, precision, length modifiers; `%zu` and `PRId64`; `snprintf`'s return as the truncation test; cursor-and-remainder appending; stdout vs stderr buffering |
| `about_arrays` | sizeof and length, short and designated initialisers, indexing as pointer arithmetic, **decay**, one-past-the-end, bounds discipline |
| `about_pointers` | `&`/`*`, `nullptr`, declarator binding, `const` placement, arithmetic scaling, `void *`, pointer-to-pointer, dangling, `uintptr_t`, a linked list via `Node **` |
| `about_strings` | NUL termination, length vs. capacity, read-only literals, `strcmp`, `snprintf` truncation, searching, `mem*` vs `str*`, `memmove` overlap |
| `about_reading_input` | `sscanf` assignment counts, `%s` needing a width, non-uniform whitespace, scansets, `fgets` keeping the newline, long lines in pieces, why a failed `scanf` consumes nothing, `fgets`-then-`strtol` |
| `about_structs` | value semantics, designated initialisers, padding and order, `offsetof`, unions, tagged unions, enums, C23 fixed underlying type, bit-fields, explicit wire format |

Formatted I/O and the toolchain sit in Tier 1 deliberately: you should be able
to build and run a program that talks to a user before reaching Tier 2. See
[COMPILING.md](COMPILING.md) for the hands-on `cc` walkthrough.

## Tier 2 · Memory and Composition — 7 lessons, 55 koans

| Lesson | Covers |
|---|---|
| `about_dynamic_memory` | `malloc`/`calloc`/`realloc`/`free`, uninitialised bytes, sizing from the pointer, multiplication-overflow guards, `free(nullptr)`, double free, realloc-may-move, ownership conventions, `goto` cleanup |
| `about_multidim_arrays` | row-major layout, decay to pointer-to-row vs. the different shape of `int **`, flat buffers, VLAs, variably-modified parameters, flexible array members |
| `about_function_pointers` | declarator syntax, typedefs, dispatch tables, callbacks, `qsort`/`bsearch`, comparator pitfalls, `void *` user data instead of closures, a vtable |
| `about_preprocessor` | substitution vs. evaluation, double evaluation, `#`/`##`, `do{}while(0)`, `__VA_OPT__`, `#elifdef`, `__has_include`, predefined macros, **X-macros** |
| `about_translation_units` | external vs. internal linkage, declarations, the three meanings of `static`, `inline`'s one-definition rule, include guards |
| `about_file_io` | modes, `fseek` origins, `fread` element counts, why `feof` is only true after a failed read, why `fgetc` returns `int`, buffering, `sscanf` |
| `about_error_handling` | `errno`'s contract, `strerror`, the three distinct `strtol` failures, error enums, `assert` vs. input validation, `setjmp`/`longjmp` and `volatile` |

## Tier 3 · Modern C23 — 6 lessons, 49 koans

| Lesson | Covers |
|---|---|
| `about_c23_features` | `bool` as a keyword, `constexpr`, `static_assert` without a message, `auto`, `typeof`/`typeof_unqual`, `[[nodiscard]]`/`[[deprecated]]`/`[[maybe_unused]]`/`[[noreturn]]`, `_BitInt(N)`, `<stdckdint.h>`, `#embed`/`__has_embed` |
| `about_generic` | type-based selection, why only the chosen branch is compiled, dispatch, qualifiers as distinct types, how `<tgmath.h>` is built |
| `about_bit_manipulation` | masks, set/clear/toggle/test, `<stdbit.h>`, powers of two, the rotate that is undefined at a zero shift |
| `about_alignment` | `alignof`, `alignas`, what `malloc` guarantees, `aligned_alloc`, mask-based rounding, `restrict` as a promise, `volatile` as distinct from atomic |
| `about_floating_point` | exact vs. inexact values, relative tolerance, `DBL_EPSILON`, the 2^53 integer limit, NaN and infinity rules, two zeros, four meanings of rounding, non-associativity |
| `about_initialisation` | storage durations, constant expressions, compound literals as objects with a lifetime, the block-scope dangling trap, designator ordering and nesting |

## Tier 4 · The Standard Library — 7 lessons, 62 koans

| Lesson | Covers |
|---|---|
| `about_stdlib` | `strto*` and base inference, comparator contracts, `qsort` instability and tiebreakers, `bsearch`, `rand` modulo bias, `div`, `getenv`, program termination (`exit`/`quick_exit`/`_Exit`/`abort`/`atexit`), `system`, the allocation family, `strtoul` accepting a minus sign |
| `about_string_library` | `strncpy` not terminating, `snprintf` as the portable bounded copy, `memchr` on non-strings, comparisons reporting order not distance, `strspn`/`strcspn`/`strpbrk`, `strtok`'s hidden state, `ctype` needing an `unsigned char` cast |
| `about_stdio` | buffering modes and when bytes leave, sticky EOF and error flags, a silently failed write, `ungetc`, `fgetpos`/`fsetpos`, `remove`/`rename` |
| `about_time` | `time_t` vs `struct tm` vs `clock_t`, irregular field bases, `strftime`, `mktime` normalisation as date arithmetic, the shared static buffer, `timespec_get` |
| `about_variadic` | count/sentinel/format conventions, default argument promotions, `va_copy`, v-prefixed forwarding, why a non-literal format is a security hole |
| `about_unicode` | byte vs code point vs grapheme, UTF-8's lead-byte structure, `u8`/`u`/`U` literals, a decoder rejecting over-long encodings, the encoder, `wchar_t`'s non-portability |
| `about_atomics` | atomicity vs ordering, fetch operations, compare-exchange strong and weak, lock-free guarantees, `atomic_flag`, release/acquire publishing ordinary writes, relaxed, fences |

## Tier 5 · POSIX — 6 lessons, 47 koans

| Lesson | Covers |
|---|---|
| `about_low_level_io` | descriptors as integers, open flags, `O_EXCL`, short reads, the `read_full`/`write_full` loops, `lseek`, `dup2`, `stat`, an unlinked file outliving its name |
| `about_processes` | pids, `fork` returning twice, exit status decoding, signalled children, copy-not-share, pipes, `exec` + `dup2`, `sigaction`, `volatile sig_atomic_t`, blocking and pending signals |
| `about_threads` | `pthread_create`/`join`, happens-before, mutexes, atomics, condition variables and the three rules, `thread_local`, C11 `<threads.h>` via the shim |
| `about_directories` | `mkdir` one level, `readdir`'s unspecified order and mandatory dots, an entry being a name not a path, `stat` vs `d_type`, `chdir` as process-wide state |
| `about_sockets` | socket/bind/listen/accept, network byte order, a stream preserving order but not boundaries, length-prefix framing, `socketpair`, `SIGPIPE` |
| `about_event_loops` | `EAGAIN` as the normal case, `poll`'s `revents` vs its return, timeouts, a closed peer showing readable, `POLLNVAL`, `POLLOUT` |

## Tier 6 · Capstones

Complete, working programs in [`projects/`](../projects), not exercises with
the middle removed. Read them after the koans; every technique came from one.

| Project | What it is |
|---|---|
| [`arena/`](../projects/arena) | A region allocator. Free-everything-at-once removes per-object ownership entirely |
| [`containers/`](../projects/containers) | `Vec` and `Map`, showing both ways to write a generic container in C — `void *` plus element size, and macro-generated typed wrappers. Open addressing with tombstones |
| [`json/`](../projects/json) | A complete recursive-descent parser: strict number grammar, surrogate pairs to UTF-8, errors with line and column, a depth limit, and a round-tripping serialiser. Arena-allocated throughout |
| [`webserver/`](../projects/webserver) | An HTTP/1.1 server: thread pool over a bounded queue, static files, CGI via fork/exec/dup2, percent-decoding, `realpath` traversal defence, graceful shutdown |

`make -C projects check` runs 701 self-checks across the three libraries,
including 14 malformed JSON documents that must be rejected and 50 arena reset
cycles asserting memory does not grow.
