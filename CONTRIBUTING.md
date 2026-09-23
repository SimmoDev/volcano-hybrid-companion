# Contributing

This project is released and open to contributions, maintained by one
person in their own time — there's no fixed review turnaround, and a
larger change may sit for a while before getting a look.

## Bug reports and protocol findings

Open an [issue](https://github.com/SimmoDev/volcano-hybrid-companion/issues).
For anything about the Volcano Hybrid's own BLE behaviour rather than
this project's code, a look at
[`docs/protocol/`](docs/protocol/README.md) first is worth it — it
already records a lot of findings, each with a confidence level
([ADR-0005](docs/decisions/ADR-0005-volcano-ble-discovery-methodology.md)),
and what you're seeing may already be one of them, or close to one.

## Pull requests

**Small, self-contained fixes** — a documentation error, a clear bug
with an obvious fix, a new protocol finding written up in the existing
format — are welcome as a PR directly.

**Anything touching architecture** — the Volcano component's
interfaces, how control interfaces are meant to depend on it, a new
control interface, a new hardware target — open an issue first. This
project makes that kind of decision through an ADR
([`docs/decisions/`](docs/decisions/README.md)) before implementing it,
not after, and a PR that skips straight to code is likely to be asked
to back up and have that conversation first.

Before opening a PR:

- Read [`docs/CONVENTIONS.md`](docs/CONVENTIONS.md) — terminology,
  British spelling, commit message style, and the naming conventions
  the rest of the repository already follows.
- See [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) for how to build and
  validate the `volcano` component locally, without needing real
  hardware for most of it.
- CI runs `esphome config`/`compile` against both configurations, the
  component's host-side tests, a `clang-format` check, and a Markdown
  link check — matching what's described there.

## Everything else

Be respectful. This project is otherwise happy to talk through an idea
that doesn't fit neatly into either category above — open an issue and
say what you're thinking.
