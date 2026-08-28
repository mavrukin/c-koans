# Troubleshooting

## "unknown argument: -std=c23" / "invalid value 'c23'"

Your compiler predates C23. You need **GCC 14+** or **Clang 18+**.

```sh
cc --version          # what you have
```

| Platform | Fix |
|---|---|
| macOS | `xcode-select --install`, or `brew install gcc` then `make CC=gcc-14` |
| Debian/Ubuntu | `sudo apt install gcc-14` then `make CC=gcc-14` |
| Fedora | `sudo dnf install gcc` |
| Arch | `sudo pacman -S gcc` |

Older compilers accept `-std=c2x` for a near-complete C23. If that is all you
have, most koans will still work:

```sh
make CFLAGS="-std=c2x -g -O0 -Iinclude -Ikoans"
```

but `constexpr`, `typeof_unqual` and `#embed` may not compile.

## A koan crashes instead of failing

The runner catches `SIGSEGV`, `SIGBUS`, `SIGFPE` and `SIGABRT` and reports
which koan died. That almost always means a null or dangling pointer, or an
index past the end of an array.

Rebuild with the sanitizers, which name the exact line and the exact object:

```sh
make san
```

## `make` says "No rule to make target"

You are probably not in the repository root. Every command assumes it.

## A koan passes that should not

Check that you edited the file under `koans/` and not the one under
`solutions/`. The runner builds `koans/`.

## `make check` fails after I edited a solution

`solutions/` is the source of truth and `koans/` is generated from it.
Regenerate after editing:

```sh
python3 tools/genkoans.py
make check
```

## The threads or processes koans hang

A hang in Tier 5 usually means a file descriptor was left open — a pipe reader
never sees end-of-file while any copy of the write end remains open — or that a
condition variable was signalled when it should have been broadcast.

Interrupt with `Ctrl-C` and re-read the comment above the koan; both cases are
called out explicitly.

## `<stdbit.h>` or `<threads.h>` not found

Expected on some platforms, and already handled: `include/compat.h` detects the
gap and supplies a conforming implementation. If you see this as a hard error
you are compiling something outside the koan build; add `-Iinclude`.

## Colours look wrong, or the output is full of escape codes

```sh
NO_COLOR=1 make
```

The runner also disables colour automatically when output is not a terminal.

## The web server will not start

`bind: Address already in use` means something already holds the port. Pick
another:

```sh
./projects/build/koanhttpd --port 8099 --root projects/webserver/www
```

The server binds to `127.0.0.1` only, by design. It is a teaching server and
should not face a network.
