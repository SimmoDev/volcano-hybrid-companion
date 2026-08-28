#!/usr/bin/env python3
"""Pre-commit hook: advisory nudge, not a block. If a commit touches
components/volcano/, docs/decisions/, or an example config, without touching
any of the docs that describe current behaviour (root README.md,
docs/DEVELOPMENT.md, examples/README.md), print a reminder to check them for
staleness.

A change to component code, an ADR, or an example config can silently
outdate a claim in one of those docs -- implementation status, phase,
capability, or what an example's entities are and do. Touching a target doc
is only a precondition the hook can check, not proof its relevant sentences
were re-read, and not every trigger change makes a doc stale -- so this only
ever prints, never fails the commit.
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
        # Built from TRIGGER_PREFIXES/TARGET_DOCS so the message can't state a
        # narrower trigger than the code actually uses.
        print(
            "NOTE: this commit touches " + " or ".join(TRIGGER_PREFIXES) + " "
            "but none of " + ", ".join(TARGET_DOCS) + ". "
            "If this changes what's true about the project's current state "
            "(implementation status, phase, capability, or what an example's "
            "entities are/do), check those docs before committing.",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
