# Contributing

## The one rule

`solutions/` is the source of truth. `koans/` is generated from it.

Never edit `koans/` by hand — your change will be overwritten on the next
regeneration. Edit the file under `solutions/`, then regenerate.

This arrangement exists so that the file a learner edits and the answer that
validates it cannot drift apart.

## Adding a koan to an existing lesson

1. Open the lesson under `solutions/`.
2. Write the koan, marking every answer with its blank type:

   ```c
   KOAN(arrays_decay_when_passed)
   {
       int nums[4] = {};
       KOAN_EQ_SZ(/*__SZ*/ 16, sizeof nums);
       KOAN_EQ_STR(/*__STR*/ "hello", greet());
       KOAN_TRUE(/*__BOOL*/ true);
   }
   ```

3. Add it to the `KOAN_LESSON(...)` list at the bottom of the file.
4. Regenerate and verify:

   ```sh
   python3 tools/genkoans.py
   make check
   ```

The answer runs from the marker to the next comma or closing parenthesis at
parenthesis depth zero, so an answer may itself contain calls and commas
inside nested parentheses.

## Adding a whole lesson

1. Create `solutions/<tier>/about_<topic>.c`.
2. End it with `KOAN_LESSON(lesson_about_<topic>, "About <Topic>", ...)`.
3. Add `KOAN_ENTRY(about_<topic>)` to `koans/manifest.def`, in the position it
   belongs — that file *is* the curriculum, and order is meaningful.
4. Regenerate and verify.

Files that are not `.c` (assets for `#embed`, for instance) are mirrored into
`koans/` automatically.

## Larger exercises

When a koan should have the learner write a function rather than fill in a
value, wrap the reference implementation in a block marker:

```c
/*__BEGIN__*/
static int add(int a, int b) { return a + b; }
/*__END__ implement add() so the assertions below hold */
```

The generated koan replaces the body with the hint and a `KOAN_PENDING`
marker, while `solutions/` keeps the working implementation.

## What makes a good koan

- **The comment teaches; the assertion checks.** A learner who reads the
  comment carefully should be able to answer without guessing.
- **Assert what the standard guarantees**, not what your machine happens to do.
  `sizeof(long)` differs across platforms; `sizeof(int32_t)` does not.
- **Prefer relationships to absolutes** wherever the standard leaves a size or
  an order unspecified.
- **Later koans combine earlier ones.** The last koan in each lesson is
  conventionally named `assembling_*` and builds something genuinely useful.
- **No flaky koans.** Anything touching threads, time or the filesystem must be
  deterministic. Run it a hundred times before submitting.

## Deliberate warnings

Some koans provoke a compiler warning on purpose, because the warning is the
lesson. Wrap only those lines:

```c
KOAN_DELIBERATE_BEGIN
    KOAN_EQ_INT(/*__*/ 0, flags & MASK == MASK);
KOAN_DELIBERATE_END
```

Everywhere else the build must be warning-free. A learner needs to trust that
any warning they see is one they caused.

## Before opening a pull request

```sh
python3 tools/genkoans.py --check   # koans/ is in sync with solutions/
make check                          # every solution passes
make san                            # and passes under ASan + UBSan
make -C projects                    # the capstones still build
```

CI runs all of the above on Linux and macOS, with both GCC and Clang, plus a
check that the *unsolved* koans still fail — so a generation bug cannot
silently ship koans that are already answered.
