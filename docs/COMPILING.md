# Compiling C

A companion to the `about_compiling` koans. Run every command here yourself —
the output is the lesson.

Everything uses `cc`, which on your machine is Clang or GCC. Substitute
`clang` or `gcc-14` if you prefer to be explicit.

---

## The four stages

A `.c` file becomes an executable in four steps. Normally one command does all
four, but each can be stopped at.

```
   hello.c
      │  preprocess   (cc -E)     #include, #define, #if
   hello.i
      │  compile      (cc -S)     C  ->  assembly
   hello.s
      │  assemble     (cc -c)     assembly  ->  machine code
   hello.o
      │  link         (cc)        + other .o files, + libraries
   hello
```

Make a file to try it on:

```sh
cat > hello.c <<'EOF'
#include <stdio.h>
#define GREETING "hello"

int main(void) {
    printf("%s, world\n", GREETING);
    return 0;
}
EOF
```

### 1. Preprocess

```sh
cc -std=c23 -E hello.c -o hello.i
wc -l hello.i          # hundreds of lines: all of stdio.h was pasted in
grep -n 'hello, world' hello.i
```

`GREETING` is gone — replaced by its text. `#include <stdio.h>` was replaced by
the entire contents of that header. This is why headers must be cheap and why
include guards matter.

### 2. Compile

```sh
cc -std=c23 -S hello.c -o hello.s
head -20 hello.s       # assembly for your CPU
```

### 3. Assemble

```sh
cc -std=c23 -c hello.c -o hello.o
nm hello.o             # the symbols this object defines and needs
```

You will see something like:

```
0000000000000000 T _main
                 U _printf
```

`T` means *defined here*. `U` means *undefined — someone else must supply it*.
That single letter is the whole concept of linking.

### 4. Link

```sh
cc hello.o -o hello
./hello
```

The linker found `printf` in the C standard library, which is linked
automatically.

---

## Several files

Real programs are many `.c` files. Each is compiled independently; the linker
joins them.

```sh
cat > mathy.h <<'EOF'
#ifndef MATHY_H
#define MATHY_H
int square(int n);
#endif
EOF

cat > mathy.c <<'EOF'
#include "mathy.h"
int square(int n) { return n * n; }
EOF

cat > main.c <<'EOF'
#include <stdio.h>
#include "mathy.h"
int main(void) { printf("%d\n", square(7)); return 0; }
EOF
```

Compile each to an object file, then link:

```sh
cc -std=c23 -c mathy.c -o mathy.o
cc -std=c23 -c main.c  -o main.o
cc mathy.o main.o -o prog
./prog                 # 49
```

Or in one command, which does exactly the same thing:

```sh
cc -std=c23 mathy.c main.c -o prog
```

### What the header is for

`main.c` never sees the body of `square`. It sees only the declaration, which
is enough for the compiler to check the call and emit an unresolved reference.

Prove it — delete the include and watch it fail at a *different* stage:

```sh
cc -std=c23 -c main.c -o main.o    # error: use of undeclared identifier 'square'
```

(Older C accepted this with only a warning, guessing the function returned
`int`. C23 removed implicit declarations, so it is now a hard error — one of
the quiet ways the language got safer.)

Now keep the include but link only `main.o`:

```sh
cc main.o -o prog                  # error: undefined symbol: _square
```

Two different errors, two different fixes:

| Error | Stage | Fix |
|---|---|---|
| undeclared identifier / unknown type | compile | add the `#include` |
| undefined symbol / undefined reference | link | add the `.o` or the `-l` |
| duplicate symbol | link | a definition ended up in a header |

That last one is why headers hold *declarations*, not function bodies. If you
must put a body in a header, mark it `static` (a private copy per file) or
`inline` (see `about_translation_units`).

---

## The flags that matter

| Flag | Meaning |
|---|---|
| `-std=c23` | which language version to accept |
| `-c` | compile only, do not link |
| `-o FILE` | where to write the output |
| `-I DIR` | also search `DIR` for `#include "..."` |
| `-L DIR` | also search `DIR` for libraries |
| `-l NAME` | link `libNAME` (`-lm` for maths, `-lpthread` for threads) |
| `-g` | keep debug information |
| `-O0` … `-O2` | optimisation level |
| `-Wall -Wextra` | turn on the warnings you actually want |
| `-fsanitize=address,undefined` | catch memory and UB errors at runtime |
| `-D NAME=VALUE` | define a macro from the command line |

### Include paths

This repository's headers live in `include/`, so every compile passes `-Iinclude`:

```sh
cc -std=c23 -Iinclude -c src/koan.c -o koan.o
```

Without it, `#include "koan.h"` fails, because `"..."` only searches next to
the including file.

### Libraries

`-lm` is the classic surprise: the maths functions are declared in `<math.h>`
but live in a separate library on most systems.

```sh
cc -std=c23 uses_sqrt.c -o prog        # may fail: undefined symbol _sqrt
cc -std=c23 uses_sqrt.c -o prog -lm    # works
```

**Order matters.** Put `-l` flags *after* the files that use them; the linker
resolves left to right.

---

## Warnings are the point

The compiler is the cheapest reviewer you will ever have. Treat its output as
part of the program.

```sh
cc -std=c23 -Wall -Wextra -Wpedantic hello.c -o hello
```

This repository goes further and asks for `-Wshadow` and `-Wvla` too. The
handful of koans that provoke a warning deliberately are wrapped in
`KOAN_DELIBERATE_BEGIN`, so that anything else you see is yours.

## Sanitizers

Far more useful than a debugger for the bugs C is famous for. They instrument
the program and abort with an exact diagnosis on the first mistake.

```sh
cc -std=c23 -g -fsanitize=address,undefined buggy.c -o buggy
./buggy
```

`make san` does this for the koans.

---

## What make is doing

Compiling by hand stops being fun at about three files. `make` reads a set of
rules — *this file depends on those files, and here is how to rebuild it* —
and runs only what is out of date.

A minimal Makefile for the example above:

```make
prog: main.o mathy.o
	cc main.o mathy.o -o prog

main.o: main.c mathy.h
	cc -std=c23 -c main.c -o main.o

mathy.o: mathy.c mathy.h
	cc -std=c23 -c mathy.c -o mathy.o

clean:
	rm -f prog *.o
```

The shape of a rule is:

```make
target: prerequisites
	recipe
```

**The recipe line must begin with a tab, not spaces.** This is make's most
notorious wart and its error message is unhelpful.

Run it:

```sh
make            # builds the first target
touch mathy.c
make            # rebuilds only mathy.o, then relinks
make            # "make: 'prog' is up to date."
```

That is the entire value proposition: it does the least work that is correct.

### Variables and patterns

Repetition is avoidable:

```make
CC      = cc
CFLAGS  = -std=c23 -Wall -Wextra -g
OBJS    = main.o mathy.o

prog: $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

`$@` is the target, `$<` is the first prerequisite, `$^` is all of them.
`%.o: %.c` is a pattern rule matching every object file.

### The dependency problem

The pattern rule above has a bug: it does not know that `main.o` depends on
`mathy.h`. Edit the header and make will not rebuild.

Compilers solve this. `-MMD -MP` makes the compiler emit a `.d` file listing
the headers it actually read, which you then include:

```make
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)
```

The leading `-` means "do not complain if these do not exist yet". This is
exactly what [the koans' Makefile](../Makefile) does — read it now; you have
seen every construct in it.

---

## Reading this repository's build

```sh
make          # build and run the koans
make -n       # print the commands without running them
```

`make -n` is the fastest way to understand any Makefile. Try it here and you
will see one `cc -c` per lesson, then one link, then the runner being invoked.

---

## Further

- `man cc`, `man ld`, `man make`
- `cc -###  hello.c` — every internal step, with full arguments
- `nm`, `objdump -d`, `otool -L` (macOS) / `ldd` (Linux) — inspect objects and
  binaries
