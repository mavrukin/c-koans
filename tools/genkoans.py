#!/usr/bin/env python3
"""
genkoans.py — generate koans/ (blanked) from solutions/ (answered).

This is a maintainer tool. Learners never need to run it; koans/ is committed.

The single source of truth for a lesson is its file under solutions/, which is
ordinary, complete, compiling C. Each place the learner is meant to fill in is
marked with a blank comment immediately before the answer:

    KOAN_EQ_INT(/*__*/ 21, 3 * 7);
    KOAN_EQ_STR(/*__STR*/ "hello", greeting);
    KOAN_TRUE(/*__BOOL*/ true);

Running this tool rewrites each marked answer as its blank:

    KOAN_EQ_INT(__, 3 * 7);
    KOAN_EQ_STR(__STR, greeting);
    KOAN_TRUE(__BOOL);

The answer runs from the marker to the next comma or closing parenthesis at
parenthesis depth zero, so answers may themselves contain calls and commas
inside nested parentheses.

Larger fill-in-the-body exercises use a block form instead:

    /*__BEGIN__*/
    ... reference implementation ...
    /*__END__ hint text shown to the learner */

which is replaced by the hint and a KOAN_PENDING marker.

Usage:  python3 tools/genkoans.py [--check]
        --check verifies koans/ is up to date without writing (for CI).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "solutions"
DST = ROOT / "koans"

BLANKS = ["__SZ", "__CHR", "__DBL", "__STR", "__BOOL", "__PTR", "__"]
# Longest first so /*__SZ*/ is not matched as /*__*/ followed by "SZ".
MARKER = re.compile(r"/\*(" + "|".join(re.escape(b) for b in BLANKS) + r")\*/")

BLOCK = re.compile(
    r"[ \t]*/\*__BEGIN__\*/[ \t]*\n(.*?)^[ \t]*/\*__END__(.*?)\*/[ \t]*\n",
    re.DOTALL | re.MULTILINE,
)


def strip_answer(text: str, start: int) -> int:
    """Return the index just past the answer expression beginning at start.

    The answer ends at the first comma or closing bracket at bracket depth
    zero. Getting there requires stepping over the four things that can
    legitimately contain such a character without ending the expression:
    string literals, character constants, and both kinds of comment.

    The apostrophe is genuinely ambiguous in C23, because it is both the
    character-constant delimiter and the digit separator: 0b0000'1000 is one
    integer literal, not the start of a character constant. It is a separator
    exactly when the preceding character could end a numeric literal.
    """
    depth = 0
    i = start
    n = len(text)
    while i < n:
        c = text[i]
        two = text[i : i + 2]

        if two == "/*":
            end = text.find("*/", i + 2)
            i = n if end < 0 else end + 2
            continue
        if two == "//":
            end = text.find("\n", i + 2)
            i = n if end < 0 else end
            continue

        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                return i
            depth -= 1
        elif c == "," and depth == 0:
            return i
        elif c == '"':  # string literal, honouring escapes
            i += 1
            while i < n and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
        elif c == "'" and not (i > start and text[i - 1].isalnum()):
            # A real character constant, not a digit separator.
            i += 1
            while i < n and text[i] != "'":
                i += 2 if text[i] == "\\" else 1
        i += 1
    return i


def blank_out(text: str) -> str:
    # Block exercises first: replace a whole reference implementation with a
    # hint, so the learner writes the body themselves.
    def block_sub(m: re.Match[str]) -> str:
        body, hint = m.group(1), m.group(2).strip()
        indent = re.match(r"[ \t]*", body).group(0) if body else "    "
        lines = [f"{indent}/* Your work: {hint} */"] if hint else []
        lines.append(f'{indent}KOAN_PENDING("{hint or "implement this koan"}");')
        return "\n".join(lines) + "\n"

    text = BLOCK.sub(block_sub, text)

    out = []
    pos = 0
    for m in MARKER.finditer(text):
        blank = m.group(1)
        out.append(text[pos : m.start()])
        # Skip whitespace between the marker and the answer.
        j = m.end()
        while j < len(text) and text[j] in " \t":
            j += 1
        end = strip_answer(text, j)
        out.append(blank)
        pos = end
    out.append(text[pos:])
    return "".join(out)


def main() -> int:
    check = "--check" in sys.argv
    if not SRC.is_dir():
        print(f"no solutions/ directory at {SRC}", file=sys.stderr)
        return 2

    stale, written = [], 0

    # Mirror non-source assets (used by #embed) so koans/ is self-contained.
    for asset in sorted(SRC.rglob("*")):
        if asset.is_dir() or asset.suffix == ".c":
            continue
        target = DST / asset.relative_to(SRC)
        blob = asset.read_bytes()
        if target.exists() and target.read_bytes() == blob:
            continue
        if check:
            stale.append(str(asset.relative_to(SRC)))
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(blob)

    for src in sorted(SRC.rglob("*.c")):
        rel = src.relative_to(SRC)
        dst = DST / rel
        text = src.read_text(encoding="utf-8")
        if not MARKER.search(text) and not BLOCK.search(text):
            print(f"warning: {rel} has no blanks", file=sys.stderr)
        body = blank_out(text)

        current = dst.read_text(encoding="utf-8") if dst.exists() else None
        if current == body:
            continue
        if check:
            stale.append(str(rel))
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_text(body, encoding="utf-8")
            written += 1

    if check:
        if stale:
            print("koans/ is out of date for:", file=sys.stderr)
            for s in stale:
                print(f"  {s}", file=sys.stderr)
            print("run: python3 tools/genkoans.py", file=sys.stderr)
            return 1
        print("koans/ is up to date")
        return 0

    print(f"generated {written} koan file(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
