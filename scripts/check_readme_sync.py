#!/usr/bin/env python3
"""Pre-commit hook: advisory nudge, not a block. If a commit touches
components/volcano/ or docs/decisions/ without also touching the root
README.md, print a reminder to check README.md's Status/Development
Phases/Contributing sections for staleness.

This is exactly the pattern that let README.md go stale for three commits
after the ADR-0009 abstraction-layer split landed: every other doc that
referenced the architecture was updated in that commit except README.md,
which was corrected only later by a milestone review. Not every such change
makes README.md stale, so this never fails the commit -- it only prints.
"""

import subprocess
import sys

TRIGGER_PREFIXES = ("components/volcano/", "docs/decisions/")


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
    touches_readme = "README.md" in files
    if touches_trigger and not touches_readme:
        print(
            "NOTE: this commit touches components/volcano/ or docs/decisions/ "
            "but not the root README.md. If this changes what's true about "
            "the project's current state (implementation status, phase, "
            "capability), check README.md's Status/Development Phases/"
            "Contributing sections before committing.",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
