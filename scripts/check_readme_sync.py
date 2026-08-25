#!/usr/bin/env python3
"""Pre-commit hook: advisory nudge, not a block. If a commit touches
components/volcano/, docs/decisions/, or an example config, without touching
any of the docs that describe current behaviour (root README.md,
docs/DEVELOPMENT.md, examples/README.md), print a reminder to check them for
staleness.

This is exactly the pattern that let README.md go stale for three commits
after the ADR-0009 abstraction-layer split landed: every other doc that
referenced the architecture was updated in that commit except README.md,
which was corrected only later by a milestone review. A later review then
found docs/DEVELOPMENT.md and the example config's own header comment still
describing pre-ADR-0009 trigger-style entities, in a commit that *did* touch
docs/DEVELOPMENT.md -- so touching a target file isn't proof its relevant
sentences were re-read, only a precondition this hook can actually check.
Not every trigger change makes one of these docs stale, so this never fails
the commit -- it only prints.

`examples/` was added to the trigger prefixes after a further review found
the entire M5Stack Dial UI build -- 18 commits, all touching only
examples/m5stack-dial-minimal.yaml -- landed without examples/README.md ever
gaining a section for it: none of those commits touched a prefix this hook
watched, so the nudge never once fired across the whole milestone.
"""

import subprocess
import sys

TRIGGER_PREFIXES = ("components/volcano/", "docs/decisions/", "examples/")
TARGET_DOCS = ("README.md", "docs/DEVELOPMENT.md", "examples/README.md")


def staged_files() -> list[str]:
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only"],
        capture_output=True,
        text=True,
        check=True,
    )
    return [line for line in result.stdout.splitlines() if line]


def main() -> int:
    files = staged_files()
    touches_trigger = any(f.startswith(TRIGGER_PREFIXES) for f in files)
    touches_target = any(doc in files for doc in TARGET_DOCS)
    if touches_trigger and not touches_target:
        print(
            "NOTE: this commit touches components/volcano/ or docs/decisions/ "
            "but none of README.md, docs/DEVELOPMENT.md or examples/README.md. "
            "If this changes what's true about the project's current state "
            "(implementation status, phase, capability, or what an example's "
            "entities are/do), check those docs before committing.",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
