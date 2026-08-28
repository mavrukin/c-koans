# C Koans

Learn C — the whole of it — by making failing assertions true.

The koans start with `sizeof(char)` and end with a working HTTP server you
wrote yourself. In between they cover the complete ISO C23 language, the
standard library, and the POSIX interfaces that real C programs are built on.

```
  Tier 1 · Foundations
  About Asserts
    + asserting_truth
    + asserting_a_claim
    - filling_in_values

  This koan has a blank in it.

  lesson  About Asserts
  koan    filling_in_values
  file    koans/01_foundations/about_asserts.c:47
  assert  __ == 3 * 7

  replace __ with the int you expect

  [###...........................]  2/9 koans
```

---

## Quick start

```sh
git clone https://github.com/<you>/c_koans.git
cd c_koans
make
```

That is the entire setup. There is nothing to install, no package manager, and
no dependencies beyond a C compiler and `make`.

`make` builds the koans and walks the path until it reaches one that is not yet
true. Open the file it names, fill in the blank, and run `make` again.

### What you need

| | |
|---|---|
| **Compiler** | GCC 14+ or Clang 18+ (C23 support) |
| **Build** | `make` |
| **Platform** | Linux or macOS. Tiers 1–4 are pure ISO C and run anywhere; Tier 5 needs POSIX |

Check your compiler:

```sh
cc -std=c23 -E -x c /dev/null >/dev/null && echo "ready"
```

If that fails, see [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

---

## How it works

Every koan is a claim about C that is currently false. You make it true by
replacing a blank with the value you believe is correct.

```c
KOAN(filling_in_values)
{
    KOAN_EQ_INT(__, 3 * 7);     /* replace __ with 21 */
}
```

Blanks are typed, because C is typed:

| Blank | Type | Example |
|---|---|---|
| `__` | `int` | `KOAN_EQ_INT(__, 1 + 1)` |
| `__SZ` | `size_t` | `KOAN_EQ_SZ(__SZ, sizeof(int))` |
| `__DBL` | `double` | `KOAN_EQ_DBL(__DBL, 0.5 + 0.25, 1e-9)` |
| `__CHR` | `char` | `KOAN_EQ_CHR(__CHR, "abc"[0])` |
| `__STR` | string | `KOAN_EQ_STR(__STR, greet())` |
| `__BOOL` | `bool` | `KOAN_TRUE(__BOOL)` |
| `__PTR` | pointer | `KOAN_EQ_PTR(__PTR, nullptr)` |

Later koans stop asking for values and start asking for code. Those are marked
`KOAN_PENDING` and tell you what to build; the assertions below them define
what "correct" means.

The runner shows you exactly one failure at a time, on purpose. Fix it, move on.

---

## Commands

```sh
make                  # walk the path until a koan stops you
make list             # every lesson, in order
make KOAN=about_pointers    # work on one lesson
make FROM=about_structs     # start partway along
make all-koans        # do not stop at the first failure
make san              # rebuild with AddressSanitizer + UBSan
make solutions        # run the reference answers
make clean
```

`make san` is worth knowing early. Once you reach dynamic memory, running your
work under the sanitizers turns a silent corruption into a precise report.

---

## The path

Each tier assumes everything above it. The later a koan sits, the more
features it expects you to combine — by Tier 5 a single exercise may need
pointers, structs, error handling, and file descriptors at once.

**151 koans across 15 lessons ship today.** Run `make list` for the ordered
index. The tiers marked *planned* are the roadmap, not the current contents;
see [docs/CURRICULUM.md](docs/CURRICULUM.md) for exactly what each covers and
[docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) if you would like to write one.

### Tier 1 · Foundations and the Toolchain — 12 lessons, 125 koans
How a `.c` file becomes a binary (preprocess, compile, assemble, link) ·
headers and multi-file builds · types, conversions and integer promotion ·
operators, precedence and evaluation order · control flow and `switch` ·
functions, scope and linkage · **printf and formatted output** · arrays and
decay · pointers, `const` and lifetime · strings · **reading input safely** ·
structs, unions, enums and bit-fields

Formatted I/O and the toolchain come early on purpose: you should be able to
write and build a real program that talks to a user before Tier 2.
See [docs/COMPILING.md](docs/COMPILING.md) for the hands-on `cc` walkthrough.

### Tier 2 · Memory and Composition — 3 of 7 lessons, 24 koans
Dynamic allocation and ownership · function pointers, `qsort` and vtables ·
the preprocessor and X-macros
*Planned:* multidimensional arrays, VLAs and flexible array members ·
translation units · file I/O · `errno` and `setjmp`

### Tier 3 · Modern C23 — 1 of 6 lessons, 11 koans
`bool`, `constexpr`, `static_assert`, `auto`, `typeof`/`typeof_unqual`,
attributes, `_BitInt`, `<stdckdint.h>` checked arithmetic, `#embed`
*Planned:* `_Generic` · `<stdbit.h>` · alignment, `restrict` and `volatile` ·
floating point and IEEE 754 · compound literals

### Tier 4 · The Standard Library — *planned*
`<stdlib.h>` algorithms · the `str`/`mem` families in depth · streams and
buffering · time · variadic functions · wide characters and UTF-8 · atomics

### Tier 5 · POSIX — 2 of 8 lessons, 17 koans
Processes, `fork`/`exec`/`wait`, pipes, `dup2`, signals and masks · pthreads,
mutexes, condition variables, atomics, `thread_local`, C11 `<threads.h>`
*Planned:* file descriptors and `stat` · directories · TCP sockets · event
loops with `poll`

### Tier 6 · Capstones — the web server ships
[`projects/webserver`](projects/webserver/server.c) is a complete, working
HTTP/1.1 server: a thread pool over a bounded queue, static files, CGI via
fork/exec/dup2, percent-decoding, `realpath`-based traversal defence, and
graceful shutdown. Every technique in it comes from a koan you solved.
*Planned:* arena allocator · generic containers with `_Generic` · JSON parser

---

## When you are stuck

0. **Check [docs/COMPILING.md](docs/COMPILING.md)** if the *build* is what is
   failing rather than a koan.
1. **Read the comment above the koan.** The answer is nearly always stated
   there, in prose.
2. **Ask the compiler.** `make san` catches memory errors; warnings are
   configured to be informative rather than noisy.
3. **Check the reference.** `solutions/` holds a worked answer for every koan.
   Using it is not cheating — reading good C is how you learn to write it. But
   try first.
4. **Look it up.** [cppreference's C section](https://en.cppreference.com/w/c)
   is excellent and tracks C23 closely.

---

## A note on portability

C23 is fully implemented at the *language* level by current compilers, but the
*library* lags, unevenly, per platform. The Apple SDK ships neither
`<stdbit.h>` nor `<threads.h>`, and its `printf` does not yet implement `%b`.

Rather than skip those features, `include/compat.h` detects what is missing and
supplies a conforming fallback. Every symbol it defines is a real C23 library
feature — nothing is invented — and the shims are written to be read. When your
platform catches up, the real header is used automatically.

---

## Contributing

Koans are generated. `solutions/` is the source of truth; `koans/` is derived
from it by `tools/genkoans.py`, which strips the answers. This guarantees that
the file you edit and the answer that validates it can never drift apart.

To add or change a koan, edit the file under `solutions/`, mark each answer
with its blank, and regenerate:

```c
KOAN_EQ_INT(/*__*/ 21, 3 * 7);      /* becomes KOAN_EQ_INT(__, 3 * 7); */
```

```sh
python3 tools/genkoans.py
make check                          # every solution must still pass
```

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for the full workflow.

---

## Prior art

The koan format was invented by [Ruby Koans](http://rubykoans.com/) and carried
into many languages since. This one exists because C had no equivalent, and
because the parts of C that most need this treatment — lifetime, aliasing,
undefined behaviour — are exactly the parts a tutorial cannot teach you by
telling.

## License

MIT. See [LICENSE](LICENSE).
