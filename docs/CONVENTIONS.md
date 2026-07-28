# Conventions

This document defines the conventions that all documentation, protocol notes, source code, and commits in this repository should follow. Unlike an ADR, it is not a record of a single decision — it is expected to grow and change as the project does. When a convention here needs to change, edit this document directly; there is no need to raise an ADR for that (see "ADRs vs. this document" below).

## Terminology

These terms are used consistently across the project. Definitions here are summaries for quick reference — see the referenced ADRs and design documents for the full reasoning and implementation details behind each concept.

- **Volcano component** — owns the device communication domain, comprising the Volcano abstraction layer and the BLE communication layer it contains, and exposes a hardware-independent interface to control interfaces. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **Volcano abstraction layer** — the part of the Volcano component that exposes Volcano domain concepts (temperature, heater, pump, state) to control interfaces, without exposing BLE details. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **BLE communication layer** — the part of the Volcano component that owns the actual BLE connection, services, characteristics, and wire protocol encoding/decoding. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **Control interface** — any consumer of the Volcano abstraction layer's interface: the M5Stack Dial local UI, Home Assistant, or direct ESPHome API/automation control. See [ADR-0001](decisions/ADR-0001-project-vision.md) and [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **State model** — the Volcano component's single authoritative record of device state (current/target temperature, heater state, pump state, connection state, etc.), which control interfaces read from rather than maintaining their own copies. See [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md).
- **Protocol finding** — a structured, recorded protocol entry under `docs/protocol/`, carrying a finding ID, an observation (including supporting evidence where it isn't self-evident), an interpretation where the observation isn't self-explanatory, a confidence classification, and an implementation status. [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md) defines that format normatively; [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md) defines the confidence classification.

Use these terms as defined here rather than introducing synonyms for the architectural components themselves (e.g. don't call the Volcano abstraction layer a "driver" or the BLE communication layer a "transport" — pick the established term). "Control interface" is the canonical term for that architectural component. Descriptive terms such as "control path" or "control surface" are fine when describing a flow or user-facing route (e.g. "three control paths a command can arrive through"), but must not be used in place of "control interface" when naming the component itself.

## Spelling

British English spelling is used throughout the repository — in documentation, code comments, commit messages, and any user-facing text. For example: *behaviour*, *colour*, *organise*, *analyse*, *initialise*, *optimise*, *licence* (noun; *license* only as a verb). When in doubt, prefer the spelling used elsewhere in `docs/decisions/`.

Two deliberate exceptions, both cases where the American spelling is part of a proper name rather than ordinary prose:

- **"vaporizer"** — Storz & Bickel market the product under this spelling, so it is used when referring to the device as a product. Ordinary prose elsewhere still follows British spelling.
- **"License"** — retained when naming the MIT License or referring to the `LICENSE` file, both of which are fixed identifiers. A section heading describing licensing in general is still spelled *Licence*, which is why the root README's "Licence" section refers to the "MIT License" within it; that mismatch is intentional, not a typo.

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

Avoid vague terms such as "usually", "normally", "generally", or "probably" when describing requirements. If uncertainty exists, classify it explicitly as a fact, interpretation, or hypothesis as described later in this document, and — for protocol findings — give it a confidence level per [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md).

## Code formatting expectations

Formatting expectations have not yet been defined concretely; `components/volcano/` has no automated formatter configured. At minimum, whatever formatter/style the Volcano component's C++ code adopts should be applied consistently via an automated tool (e.g. `clang-format`) rather than left to manual judgement, and that tooling choice should be documented here once made.

## Naming conventions — files and directories

- **ADRs**: `docs/decisions/ADR-NNNN-short-kebab-case-title.md`, numbered sequentially, never reused or renumbered once accepted.
- **Protocol documentation**: fixed filenames under `docs/protocol/` as defined in [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md) (`README.md`, `gatt-services.md`, `characteristics.md`, `commands.md`, `state-model.md`, `open-questions.md`).
- **General directories and files**: use lower-kebab-case where practical (e.g. `components/volcano/`). Established conventional filenames such as `README.md`, `LICENSE`, and tool-required files are exceptions.
- **C++ source files**: `components/volcano/` currently uses lowercase filenames matching the component name (`volcano.h`, `volcano.cpp`), following typical ESPHome external component convention; no further formal naming convention has been decided beyond that.

## Naming conventions — protocol findings

- **Finding IDs**: `<area>-NNN`, where `<area>` matches the document the finding lives in (e.g. `SVC-001` for a `gatt-services.md` finding, `CHAR-001` for a `characteristics.md` finding, `CMD-014` for a `commands.md` finding, `STATE-003` for a `state-model.md` finding). IDs are never reused, even if a finding is later superseded. `gatt-services.md` additionally uses the `ADV-NNN` and `CONN-NNN` prefixes for device-level findings about advertising/discovery and about the connection procedure, which are not about any one service.

## Commit message style

- Follow standard Git convention: a short (~50 character) imperative-mood summary line, blank line, then body paragraph(s) explaining *why* the change was made where that isn't obvious from the summary alone.
- Summary line does not end with a period, and avoids restating the diff (the diff already shows *what* changed).
- Reference an ADR or protocol finding ID in the body when a commit implements or depends on one (e.g. "Implements the temperature-set command per ADR-0002 and finding CMD-014.").
- British spelling applies to commit messages as it does everywhere else.

## ADRs vs. this document

Create a new ADR when a change represents an architectural decision — something that constrains future work, is expensive to reverse, or that later contributors need to understand the *reasoning* behind (see the existing ADRs for examples: framework choice, component boundaries, hardware strategy, discovery methodology). ADRs are static once accepted; changing course requires a new ADR that supersedes the old one, not an edit to it.

Update this document (or another living document, such as the protocol documentation under `docs/protocol/`, or design documentation under a future `docs/design/`) when the change is a convention, a piece of accumulating knowledge, or anything else expected to be refined incrementally over time rather than decided once. If it's unclear which applies, ask: "does reversing this later require justifying a new decision, or just editing a reference document?" The former is an ADR; the latter is a living document.

## Facts, interpretations, hypotheses, and implementation

These four are kept distinct, particularly in protocol documentation, per [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md):

- **Facts / observations** — what was directly seen happening, recorded exactly as observed and never edited to fit a conclusion.
- **Interpretation** — a claim about what an observation means in Volcano domain terms. Interpretations can be wrong even when the underlying observation is accurate.
- **Hypotheses** — interpretations not yet backed by enough evidence to be trusted; recorded with a Probable or Unknown confidence level rather than presented as settled.
- **Implementation** — code that acts on a finding. Per ADR-0005, only Confirmed findings back default production behaviour; Probable findings may exist behind clearly marked experimental code; Unknown behaviour is never silently encoded as an assumption.

Keep these labelled as what they are wherever they appear — in protocol documentation, code comments, and commit messages alike — rather than letting a hypothesis read as an established fact.

A finding's different aspects don't always share one confidence level — for example, a characteristic's raw value can be Confirmed while what it means is still Unknown, or a register's write behaviour can be Confirmed while which specific bit governs a related, observed effect is only Probable. In that case, state each aspect with its own confidence rather than picking a single overall level: e.g. `Confirmed (value); Unknown (meaning)`.

## Findings, not testing narrative

Protocol documentation records what the device does and how far it can be trusted — not the story of how that came to be known. The distinction is not "no methodology": some evidence about *how strongly* something is established is exactly what justifies a confidence level, and belongs in the finding.

Record:

- The observation itself, and the value or behaviour seen.
- **Evidence strength**, stated as a property of the finding rather than as an activity — "reproduced across multiple sessions", "observed in both directions". ADR-0005 requires repeatability before anything is Confirmed, so a bare `Confirmed` with nothing supporting it is incomplete.
- **Control conditions**, where they are what separates correlation from causation — for example that a bit tracks the pump with heater state held constant, rather than merely appearing at the same time.
- For an open question, **what would resolve it**. This is prospective procedure and is required by [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md).

Leave out:

- The sequence of investigation — what was tried first, what failed, what was tried next, or which session something happened in.
- Which tool, app screen, or menu was used to look at something. Where checking somewhere yielded nothing, record the conclusion ("no app-displayed value corresponds to this characteristic"), not the act of checking.
- Attempt counts and timings that add nothing over "reproduced" — "4 of 4 matches", "within 3 seconds".
- The documentation's own history. If a recorded figure turns out to be wrong, correct it; don't narrate the correction.

A useful test: **would this sentence still be worth writing if the protocol had been documented correctly the first time, with no investigation behind it?** If not, it is testing narrative and belongs out. Note that the prospective "would be resolved by" in an open question passes this test — it describes work still to do, not work already done.
