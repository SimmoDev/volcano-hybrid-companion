#!/usr/bin/env python3
"""Pre-commit hook: advisory nudge, not a block. If a commit touches
components/volcano/, docs/decisions/, or an example config, this names each of
the state-describing docs (root README.md, docs/DEVELOPMENT.md,
examples/README.md) the commit does not also touch, so a claim in one of them
isn't left stale.

A change to component code, an ADR, or an example config can silently outdate
a claim in one of those docs -- implementation status, phase, capability, or
what an example's entities are and do. Touching a doc is only a precondition
the hook can check, not proof its relevant sentences were re-read, and not
every trigger change makes every doc stale -- so this only ever prints, never
fails the commit. It nudges about each untouched doc individually rather than
falling silent as soon as any one of the three is staged, since touching
examples/README.md for an example change does nothing for a claim that has
gone stale in DEVELOPMENT.md.
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
    if not any(f.startswith(TRIGGER_PREFIXES) for f in files):
        return 0
    untouched = [doc for doc in TARGET_DOCS if doc not in files]
    if untouched:
        # Built from TRIGGER_PREFIXES/TARGET_DOCS so the message can't state a
        # narrower trigger than the code actually uses.
        subject = "it" if len(untouched) == 1 else "each"
        print(
            "NOTE: this commit touches " + " or ".join(TRIGGER_PREFIXES) + " "
            "but not " + ", ".join(untouched) + ". "
            "If this changes what's true about the project's current state "
            "(implementation status, phase, capability, or what an example's "
            "entities are/do), check " + subject + " before committing.",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
