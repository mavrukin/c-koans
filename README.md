# C Koans — Learn C Programming by Fixing Failing Tests

**A complete, hands-on C tutorial.** Learn C the way you actually learn a
language: by writing it. 338 exercises take you from `sizeof(char)` to a
working HTTP server you built yourself, covering the entire **ISO C23**
standard, the C standard library, and **POSIX systems programming**.

No videos. No slides. No "left as an exercise for the reader." Just a compiler
telling you what is true and what is not.

```sh
git clone https://github.com/mavrukin/c-koans.git
cd c-koans
make
```

That is the whole setup.

---

## Contents

- [What is a koan?](#what-is-a-koan)
- [What you will learn](#what-you-will-learn)
- [Getting started](#getting-started) — [install a compiler](#step-1-install-a-c-compiler) · [clone and run](#step-2-clone-and-run) · [solve your first koan](#step-3-solve-your-first-koan)
- [Editor setup](#editor-setup) — [VS Code](#visual-studio-code) · [Vim / Neovim](#vim--neovim) · [Emacs](#emacs) · [CLion](#clion-and-other-jetbrains-ides)
- [The learning path](#the-learning-path)
- [Commands](#commands)
- [Capstone projects](#capstone-projects)
- [FAQ](#frequently-asked-questions)
- [Contributing](#contributing)

---

## What is a koan?

Every koan is a small claim about C that is **currently false**. Your job is to
make it true.

```c
KOAN(filling_in_values)
{
    KOAN_EQ_INT(__, 3 * 7);     // replace __ with the answer
}
```

Run `make`, and the runner stops at the first koan that is not yet true:

```
  Tier 1 · Foundations and the Toolchain
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

  [###...........................]  2/338 koans
```

Open the file, fill in the blank, run `make` again. One problem at a time,
338 times, until you have written every important idea in C with your own hands.

This format was invented by [Ruby Koans](http://rubykoans.com/) and has been
carried into many languages. C never had a complete one — and the parts of C
that most need this treatment (lifetime, aliasing, undefined behaviour) are
exactly the parts a tutorial cannot teach you by telling.

---

## What you will learn

By the end you will understand these, because you will have written them:

**The C language, completely**
Pointers and pointer arithmetic · arrays and why they decay · `const`
correctness · structs, unions, enums and bit-fields · integer promotion and
the conversion rules that cause real bugs · undefined behaviour and how to
avoid it · the preprocessor and X-macros · translation units and linkage

**Modern C23** — the current ISO standard (ISO/IEC 9899:2024)
`constexpr` · `nullptr` · `auto` type inference · `typeof` · `[[nodiscard]]`
and friends · `_BitInt(N)` · `_Generic` · `#embed` · `<stdbit.h>` ·
`<stdckdint.h>` checked arithmetic · binary literals

**Memory, properly**
`malloc`/`free` and ownership discipline · what a leak actually is ·
use-after-free · alignment · arena allocation · running everything under
**AddressSanitizer**

**The standard library**
`printf` and the full conversion grammar · reading input safely · `<string.h>`
and its traps · streams and buffering · `qsort`/`bsearch` · `<time.h>` ·
variadic functions · UTF-8 and Unicode · atomics and the memory model

**POSIX systems programming**
File descriptors · `fork`/`exec`/`wait` · pipes · signals · `stat` and
directories · **pthreads**, mutexes and condition variables · **TCP sockets** ·
`poll` and event loops

**The toolchain**
What `gcc` and `clang` actually do · preprocess, compile, assemble, link · why
a missing semicolon is a *compile* error and a missing function body is a
*link* error · headers · `-I`, `-l`, `-Wall` · **how to write a Makefile**

---

## Getting started

### Step 1: Install a C compiler

You need **GCC 15+** or **Clang 19+**, plus `make`.

These koans target ISO C23 strictly. GCC 14 implements most of C23 but still
reports the pre-ratification version number, so the very first lesson rejects
it — GCC 15 is the first release that reports the ratified `202311L`.

<details open>
<summary><b>macOS</b></summary>

```sh
xcode-select --install
```

That gives you Apple Clang and `make`. For GCC instead:

```sh
brew install gcc
make CC=gcc-15
```
</details>

<details>
<summary><b>Ubuntu / Debian</b></summary>

```sh
sudo apt update
sudo apt install build-essential gcc-15
make CC=gcc-15
```
</details>

<details>
<summary><b>Fedora / RHEL</b></summary>

```sh
sudo dnf install gcc make
```
</details>

<details>
<summary><b>Arch Linux</b></summary>

```sh
sudo pacman -S base-devel gcc
```
</details>

<details>
<summary><b>Windows (WSL)</b></summary>

Tier 5 needs POSIX, so use WSL2:

```powershell
wsl --install -d Ubuntu
```

Then follow the Ubuntu instructions inside WSL.
</details>

Check that it worked:

```sh
cc --version
cc -std=c23 -E -x c /dev/null >/dev/null && echo "C23 ready"
```

If that fails, see [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

### Step 2: Clone and run

```sh
git clone https://github.com/mavrukin/c-koans.git
cd c-koans
make
```

`make` compiles the koans and walks the path until one stops you. There is
**nothing to install** — no package manager, no dependencies, no test
framework. Everything the koans need ships in this repository.

### Step 3: Solve your first koan

The runner just told you which file and line to open:

```
  file    koans/01_foundations/about_asserts.c:21
```

Open it with whatever you like:

```sh
nano koans/01_foundations/about_asserts.c     # simplest
vim  koans/01_foundations/about_asserts.c     # vim
code koans/01_foundations/about_asserts.c     # VS Code
```

Go to line 21:

```c
KOAN(asserting_truth)
{
    KOAN_TRUE(__BOOL);
}
```

Replace `__BOOL` with `true`, save, and run `make` again. That is the whole
loop — repeat it 338 times and you will know C.

**Blanks are typed**, because C is typed:

| Blank | Type | Example |
|---|---|---|
| `__` | `int` | `KOAN_EQ_INT(__, 1 + 1)` |
| `__SZ` | `size_t` | `KOAN_EQ_SZ(__SZ, sizeof(int))` |
| `__DBL` | `double` | `KOAN_EQ_DBL(__DBL, 0.5 + 0.25, 1e-9)` |
| `__CHR` | `char` | `KOAN_EQ_CHR(__CHR, "abc"[0])` |
| `__STR` | string | `KOAN_EQ_STR(__STR, greet())` |
| `__BOOL` | `bool` | `KOAN_TRUE(__BOOL)` |
| `__PTR` | pointer | `KOAN_EQ_PTR(__PTR, nullptr)` |

---

## Editor setup

Any text editor works — these are plain `.c` files and the build is a plain
Makefile. But autocomplete and inline errors make the experience much better.

Generate a compilation database once, and every modern C tool will understand
the project:

```sh
make compiledb        # writes compile_commands.json
```

### Visual Studio Code

```sh
code .
```

Install either the **C/C++ extension** (`ms-vscode.cpptools`) or, better,
**clangd** (`llvm-vs-code-extensions.vscode-clangd`). Run `make compiledb`
once and you get jump-to-definition, inline errors, and hover documentation
for every standard library function.

Press <kbd>Cmd/Ctrl</kbd>+<kbd>Shift</kbd>+<kbd>B</kbd> to run the koans
without leaving the editor.

### Vim / Neovim

```sh
vim koans/01_foundations/about_asserts.c
```

Useful immediately, with no plugins:

- `:make` runs the koans
- `:copen` opens the error list, `:cn` / `:cp` jump between failures

For autocomplete, point [clangd](https://clangd.llvm.org/) at the project after
`make compiledb`. With Neovim's built-in LSP:

```lua
require('lspconfig').clangd.setup{}
```

### Emacs

```sh
emacs koans/01_foundations/about_asserts.c
```

`M-x compile` with `make`, then `M-x next-error` to walk the failures.
`eglot` (built in since Emacs 29) picks up `compile_commands.json`
automatically:

```
M-x eglot
```

### CLion and other JetBrains IDEs

Open the folder directly — CLion reads Makefile projects natively. Set your
C23-capable compiler under **Settings → Build, Execution, Deployment →
Toolchains**.

---

## The learning path

Each tier assumes everything above it. The deeper you go, the more a single
exercise expects you to combine — by Tier 5 one koan may need pointers,
structs, error handling and file descriptors at once.

**38 lessons · 338 koans · 4 capstone projects.**
Run `make list` for the ordered index, or read
[docs/CURRICULUM.md](docs/CURRICULUM.md) for what every lesson covers.

### Tier 1 · Foundations and the Toolchain — 12 lessons, 125 koans

How a `.c` file becomes a binary · headers and multi-file builds · types and
integer promotion · operators and precedence · control flow · functions and
linkage · **`printf` and formatted output** · arrays and decay · pointers ·
strings · **reading input safely** · structs, unions, enums and bit-fields

> I/O and the toolchain come *early* on purpose. You should be able to build
> and run a real program that talks to a user before you reach Tier 2.

### Tier 2 · Memory and Composition — 7 lessons, 55 koans

Dynamic allocation and ownership · multidimensional arrays, VLAs and flexible
array members · function pointers and vtables · the preprocessor and X-macros ·
translation units · file I/O · `errno` and `setjmp`

### Tier 3 · Modern C23 — 6 lessons, 49 koans

`constexpr`, `auto`, `typeof`, attributes, `_BitInt`, `#embed` · `_Generic` ·
`<stdbit.h>` · alignment, `restrict` and `volatile` · IEEE 754 floating point ·
compound literals and object lifetime

### Tier 4 · The Standard Library — 7 lessons, 62 koans

`<stdlib.h>` · the `str`/`mem` families in depth · streams and buffering ·
time · variadic functions · UTF-8 and Unicode · atomics and memory ordering

### Tier 5 · POSIX — 6 lessons, 47 koans

File descriptors and `stat` · processes, `fork`/`exec`, pipes and signals ·
pthreads, mutexes and condition variables · directories · TCP sockets ·
`poll` event loops

### Tier 6 · Capstones — 4 complete programs

Working code, not exercises with the middle removed. See below.

---

## Commands

```sh
make                        # walk the path until a koan stops you
make list                   # every lesson, in order
make KOAN=about_pointers    # work on a single lesson
make FROM=about_structs     # start partway along
make all-koans              # don't stop at the first failure
make san                    # rebuild with AddressSanitizer + UBSan
make solutions              # run the reference answers
make compiledb              # generate compile_commands.json for your editor
make clean
```

**`make san` is worth knowing early.** Once you reach dynamic memory, running
your work under the sanitizers turns a silent corruption into a precise report
naming the exact line and object.

---

## Capstone projects

Four complete, working programs in [`projects/`](projects). Every technique in
them came from a koan you solved, and each file's header comment says which.

| Project | What it is |
|---|---|
| [**arena**](projects/arena) | A region allocator. Free-everything-at-once removes per-object ownership entirely |
| [**containers**](projects/containers) | `Vec` and a hash `Map`, showing both ways to write a generic container in C |
| [**json**](projects/json) | A full recursive-descent JSON parser: surrogate pairs, precise error positions, round-tripping serialiser |
| [**webserver**](projects/webserver) | An **HTTP/1.1 server**: thread pool, static files, CGI, path-traversal defence, graceful shutdown |

```sh
make -C projects check    # 701 self-checks across the three libraries
make -C projects run      # start the web server
curl http://127.0.0.1:8080/
```

---

## Frequently asked questions

**Do I need to know C already?**
No. Tier 1 assumes you can program in *something* and starts from
`sizeof(char)`. If you have never programmed at all, learn basic control flow
somewhere friendlier first, then come back.

**Is this C or C++?**
C, and only C. Specifically **C23**, the current ISO standard. Nothing here
compiles as C++, and that is deliberate.

**How long does it take?**
Most people take 20–40 hours. Tier 1 goes quickly; Tier 5 does not. A lesson a
day finishes it in about six weeks.

**Can I skip ahead?**
`make FROM=about_pointers` starts anywhere. But the ordering is real — later
koans use earlier ideas without re-explaining them.

**Is looking at the solutions cheating?**
No. `solutions/` holds a worked answer for every koan, and reading good C is
how you learn to write it. Try first, then look, then close it and do it again.

**Why does my build show a warning?**
It shouldn't. The build is warning-free on GCC and Clang, and the handful of
koans that provoke a warning deliberately are silenced explicitly. Any warning
you see is one you introduced — which is exactly the point.

**Does this work on Windows?**
Tiers 1–4 are pure ISO C and run anywhere. Tier 5 needs POSIX, so use WSL2.

**How is this different from a C book?**
A book tells you dangling pointers are bad. Here a koan hands you one, and the
sanitizer shows you the exact line where it bit. You cannot skim it, because
the compiler decides when you have understood.

---

## Contributing

Koans are **generated**. `solutions/` is the source of truth; `koans/` is
derived from it by `tools/genkoans.py`, which strips the answers. This
guarantees that the file you edit and the answer validating it can never drift
apart.

```c
KOAN_EQ_INT(/*__*/ 21, 3 * 7);      // becomes KOAN_EQ_INT(__, 3 * 7);
```

```sh
python3 tools/genkoans.py
make check-all              # everything CI runs
```

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for the full workflow.

## Documentation

- [**CURRICULUM.md**](docs/CURRICULUM.md) — every lesson and what it covers
- [**COMPILING.md**](docs/COMPILING.md) — `cc`, linking, headers and Makefiles from scratch
- [**TROUBLESHOOTING.md**](docs/TROUBLESHOOTING.md) — when the build fights you
- [**CONTRIBUTING.md**](docs/CONTRIBUTING.md) — adding koans

## A note on portability

C23 is fully implemented at the *language* level by current compilers, but the
*library* lags unevenly per platform — the Apple SDK ships neither
`<stdbit.h>` nor `<threads.h>`. Rather than skip those features,
[`include/compat.h`](include/compat.h) detects what is missing and supplies a
conforming implementation. Every symbol it defines is a real C23 feature, and
the shims are written to be read.

## License

MIT — see [LICENSE](LICENSE). Use it, fork it, teach with it.

---

<sub><b>Topics:</b> learn C programming · C tutorial · C23 tutorial · C koans ·
C exercises · C practice problems · learn C by doing · C pointers tutorial ·
C memory management · POSIX systems programming · pthreads tutorial ·
C sockets tutorial · modern C · ISO C23 · C standard library ·
makefile tutorial · gcc tutorial · clang · systems programming</sub>
