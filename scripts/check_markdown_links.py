#!/usr/bin/env python3
"""Pre-commit hook: every relative Markdown link in a tracked .md file must
resolve to a real file, per CONVENTIONS.md's "cross-reference other
documents with relative Markdown links" rule -- a broken one is an
unambiguous defect, so this blocks the commit rather than just warning.

Link-like text inside inline code spans or fenced code blocks is not
checked: CONVENTIONS.md itself uses `[ADR-0002](ADR-0002-....md)` as a
*syntax example* inside backticks, which is not a real cross-reference and
must not be flagged as a broken one.
"""

import os
import re
import sys

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
FENCED_CODE_RE = re.compile(r"```.*?```", re.DOTALL)
INLINE_CODE_RE = re.compile(r"`[^`]*`")


def strip_code(text: str) -> str:
    text = FENCED_CODE_RE.sub("", text)
    text = INLINE_CODE_RE.sub("", text)
    return text


def check_file(path: str) -> list[tuple[str, str, str]]:
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    problems = []
    for match in LINK_RE.finditer(strip_code(text)):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:")):
            continue
        path_part, _, _anchor = target.partition("#")
        if path_part == "":
            continue  # same-document anchor
        resolved = os.path.normpath(os.path.join(os.path.dirname(path), path_part))
        if not os.path.exists(resolved):
            problems.append((path, target, resolved))
    return problems


def main(argv: list[str]) -> int:
    problems = []
    for path in argv:
        if path.endswith(".md"):
            problems.extend(check_file(path))
    for path, target, resolved in problems:
        print(f"{path}: broken relative link [{target}] -> {resolved}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
