#!/usr/bin/env python3
"""Pre-commit hook: every relative Markdown link in a tracked .md file must
resolve to a real file, and -- where it carries a `#fragment` -- to a real
heading in that file, per CONVENTIONS.md's "cross-reference other documents
with relative Markdown links" rule. A broken path or a broken anchor is an
unambiguous defect, so this blocks the commit rather than just warning.

Link-like text inside inline code spans or fenced code blocks is not
checked: CONVENTIONS.md itself uses `[ADR-0002](ADR-0002-....md)` as a
*syntax example* inside backticks, which is not a real cross-reference and
must not be flagged as a broken one.

Anchors are resolved by regenerating the target file's heading slugs with
GitHub's algorithm (lower-case, strip a fixed set of punctuation, spaces to
hyphens, numeric suffix on repeats). A `#fragment` that names no such slug
is reported. Only `.md` targets are anchor-checked; a fragment on any other
target (an HTML page, say) is left alone.
"""

import os
import re
import sys

LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
FENCED_CODE_RE = re.compile(r"```.*?```", re.DOTALL)
INLINE_CODE_RE = re.compile(r"`[^`]*`")
INLINE_LINK_RE = re.compile(r"\[([^\]]*)\]\([^)]*\)")
ATX_HEADING_RE = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*$")

# GitHub's slugger keeps letters (any script), digits, hyphen and
# underscore; every other character is dropped, and spaces then become
# hyphens. `\w` under Python's default Unicode matching is that "letters,
# digits, underscore" set, so `[^\w\- ]` is what GitHub removes.
SLUG_DROP_RE = re.compile(r"[^\w\- ]")

_anchor_cache: dict[str, "set[str] | None"] = {}


def strip_code(text: str) -> str:
    text = FENCED_CODE_RE.sub("", text)
    text = INLINE_CODE_RE.sub("", text)
    return text


def github_slug(heading: str) -> str:
    # A heading may carry an inline link -- the slug comes from its visible
    # text, not the URL.
    text = INLINE_LINK_RE.sub(r"\1", heading).strip().lower()
    return SLUG_DROP_RE.sub("", text).replace(" ", "-")


def anchors_for(path: str) -> "set[str] | None":
    """The set of heading slugs a rendered Markdown file would expose, or
    None if the file can't be read."""
    if path not in _anchor_cache:
        try:
            with open(path, encoding="utf-8") as handle:
                text = handle.read()
        except OSError:
            _anchor_cache[path] = None
            return None
        slugs: set[str] = set()
        seen: dict[str, int] = {}
        for line in FENCED_CODE_RE.sub("", text).splitlines():
            match = ATX_HEADING_RE.match(line)
            if not match:
                continue
            base = github_slug(match.group(2).rstrip("#").rstrip())
            count = seen.get(base, 0)
            seen[base] = count + 1
            slugs.add(base if count == 0 else f"{base}-{count}")
        _anchor_cache[path] = slugs
    return _anchor_cache[path]


def check_file(path: str) -> list[tuple[str, str, str]]:
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    problems = []
    for match in LINK_RE.finditer(strip_code(text)):
        target = match.group(1)
        if target.startswith(("http://", "https://", "mailto:")):
            continue
        path_part, _, anchor = target.partition("#")
        if path_part == "":
            resolved = path  # same-document anchor
        else:
            resolved = os.path.normpath(os.path.join(os.path.dirname(path), path_part))
            if not os.path.exists(resolved):
                problems.append((path, target, f"no such file: {resolved}"))
                continue
        if anchor and resolved.endswith(".md"):
            anchors = anchors_for(resolved)
            if anchors is not None and anchor not in anchors:
                problems.append((path, target, f"no heading '#{anchor}' in {resolved}"))
    return problems


def main(argv: list[str]) -> int:
    problems = []
    for path in argv:
        if path.endswith(".md"):
            problems.extend(check_file(path))
    for path, target, detail in problems:
        print(f"{path}: broken relative link [{target}] -> {detail}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
