# Conventions

This document defines the conventions that all documentation, protocol notes, source code, and commits in this repository should follow. Unlike an ADR, it is not a record of a single decision — it is expected to grow and change as the project does. When a convention here needs to change, edit this document directly; there is no need to raise an ADR for that (see "ADRs vs. this document" below).

## Terminology

These terms are used consistently across the project. Definitions here are summaries for quick reference — see the referenced ADRs and design documents for the full reasoning and implementation details behind each concept.

- **Volcano component** — owns the device communication domain, comprising the Volcano abstraction layer and the BLE communication layer it contains, and exposes a hardware-independent interface to control interfaces. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **Volcano abstraction layer** — the part of the Volcano component that exposes Volcano domain concepts (temperature, heater, valve, state) to control interfaces, without exposing BLE details. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **BLE communication layer** — the part of the Volcano component that owns the actual BLE connection, services, characteristics, and wire protocol encoding/decoding. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **Control interface** — any consumer of the Volcano abstraction layer's interface: the M5Stack Dial local UI, Home Assistant, or direct ESPHome API/automation control. See [ADR-0001](decisions/ADR-0001-project-vision.md) and [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **State model** — the Volcano component's single authoritative record of device state (current/target temperature, heater state, valve state, connection state, etc.), which control interfaces read from rather than maintaining their own copies. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **Protocol finding** — a structured, recorded protocol entry under `docs/protocol/`, capturing an observation, its supporting evidence, an interpretation, and a confidence classification. See [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md) and [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md).

Use these terms as defined here rather than introducing synonyms for the architectural components themselves (e.g. don't call the Volcano abstraction layer a "driver" or the BLE communication layer a "transport" — pick the established term). "Control interface" is the canonical term for that architectural component. Descriptive terms such as "control path" or "control surface" are fine when describing a flow or user-facing route (e.g. "three control paths a command can arrive through"), but must not be used in place of "control interface" when naming the component itself.

## Spelling

British English spelling is used throughout the repository — in documentation, code comments, commit messages, and any user-facing text. For example: *behaviour*, *colour*, *organise*, *analyse*, *initialise*, *optimise*, *licence* (noun; *license* only as a verb, e.g. in the LICENSE file's boilerplate text, which follows the standard MIT wording as-is). When in doubt, prefer the spelling used elsewhere in `docs/decisions/`.

## Markdown conventions

- One `#` H1 title per document, matching the document's name/purpose.
- ADRs follow the section structure established by the existing ADRs: `## Status`, `## Context`, `## Decision`, `## Consequences`, `## Alternatives considered`, `## Notes`. Don't introduce new top-level sections into that structure without good reason.
- Use `**bold**` for short inline labels/subheadings within a section (as seen in the Decision/Consequences sections of existing ADRs), and `###` subheadings when a section needs multiple distinct, navigable parts.
- Cross-reference other documents with relative Markdown links (e.g. `[ADR-0002](ADR-0002-volcano-component-architecture.md)`), not bare filenames or wiki-style `[[links]]`.
- Wrap identifiers, filenames, and code fragments in backticks; use fenced code blocks (with a language tag where applicable) for anything longer than a single identifier.

## Normative language

- **Must / must not** — a mandatory requirement. Violating this means the implementation does not conform to the documented design.
- **Should / should not** — a strong recommendation. Exceptions may exist, but they should be explicitly justified.
- **May** — an optional capability or permitted behaviour.

Avoid vague terms such as "usually", "normally", "generally", or "probably" when describing requirements. If uncertainty exists, classify it explicitly as a fact, interpretation, hypothesis, or unknown as described later in this document.

## Code formatting expectations

Formatting expectations will be defined concretely once source code exists (this repository currently has none). At minimum, whatever formatter/style the Volcano component's C++ code adopts should be applied consistently via an automated tool (e.g. `clang-format`) rather than left to manual judgement, and that tooling choice should be documented here once made.

## Naming conventions — files and directories

- **ADRs**: `docs/decisions/ADR-NNNN-short-kebab-case-title.md`, numbered sequentially, never reused or renumbered once accepted.
- **Protocol documentation**: fixed filenames under `docs/protocol/` as defined in [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md) (`README.md`, `gatt-services.md`, `characteristics.md`, `commands.md`, `state-model.md`, `open-questions.md`), plus the `captures/` directory.
- **General directories and files**: use lower-kebab-case where practical (e.g. `components/volcano/`). Established conventional filenames such as `README.md`, `LICENSE`, and tool-required files are exceptions.
- **C++ source files**: to be defined when source code is introduced, following whatever convention ESPHome external components typically use, for consistency with the wider ESPHome ecosystem.

## Naming conventions — protocol findings and capture files

- **Finding IDs**: `<area>-NNN`, where `<area>` matches the document the finding lives in (e.g. `SVC-001` for a `gatt-services.md` finding, `CMD-014` for a `commands.md` finding, `STATE-003` for a `state-model.md` finding). IDs are never reused, even if a finding is later superseded.
- **Capture files**: `YYYY-MM-DD-<short-description>.<ext>` (e.g. `2026-08-03-set-target-temp-notification.pcap`), stored under `docs/protocol/captures/`. The date reflects when the capture was taken, and the description should be specific enough to distinguish it from other captures on the same day.
- Every finding's **Capture references** field (per [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md)) must use these exact capture filenames, so findings and evidence stay unambiguously linked.

## Commit message style

- Follow standard Git convention: a short (~50 character) imperative-mood summary line, blank line, then body paragraph(s) explaining *why* the change was made where that isn't obvious from the summary alone.
- Summary line does not end with a period, and avoids restating the diff (the diff already shows *what* changed).
- Reference an ADR or protocol finding ID in the body when a commit implements or depends on one (e.g. "Implements the temperature-set command per ADR-0002 and finding CMD-014.").
- British spelling applies to commit messages as it does everywhere else.

## ADRs vs. this document

Create a new ADR when a change represents an architectural decision — something that constrains future work, is expensive to reverse, or that later contributors need to understand the *reasoning* behind (see the existing ADRs for examples: framework choice, component boundaries, hardware strategy, discovery methodology). ADRs are static once accepted; changing course requires a new ADR that supersedes the old one, not an edit to it.

Update this document (or another living document, such as the protocol documentation under `docs/protocol/` or design documentation under `docs/design/`) when the change is a convention, a piece of accumulating knowledge, or anything else expected to be refined incrementally over time rather than decided once. If it's unclear which applies, ask: "does reversing this later require justifying a new decision, or just editing a reference document?" The former is an ADR; the latter is a living document.

## Facts, interpretations, hypotheses, and implementation

These four are kept distinct, particularly in protocol documentation, per [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md):

- **Facts / observations** — what was directly seen happening (raw captures, recorded exactly as observed, never edited to fit a conclusion).
- **Interpretation** — a claim about what an observation means in Volcano domain terms. Interpretations can be wrong even when the underlying observation is accurate.
- **Hypotheses** — interpretations not yet backed by enough evidence to be trusted; recorded with a Probable or Unknown confidence level rather than presented as settled.
- **Implementation** — code that acts on a finding. Per ADR-0005, only Confirmed findings back default production behaviour; Probable findings may exist behind clearly marked experimental code; Unknown behaviour is never silently encoded as an assumption.

Keep these labelled as what they are wherever they appear — in protocol documentation, code comments, and commit messages alike — rather than letting a hypothesis read as an established fact.
