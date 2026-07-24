# ADR-0006: Protocol Documentation Structure

## Status

Accepted

## Context

[ADR-0005](ADR-0005-volcano-ble-discovery-methodology.md) establishes that Volcano BLE protocol discovery must be evidence-driven, and that every finding must be recorded with observation conditions, evidence, interpretation, and a confidence classification (Confirmed, Probable, or Unknown). It does not define where that record lives or how it is structured. Before BLE investigation begins, the project needs a consistent format for recording discoveries. Without a defined structure, protocol knowledge will become fragmented across scratch notes, code comments, capture files, and memory — exactly the outcome ADR-0005 exists to prevent, and the fragmentation would make it impossible to tell later whether a given piece of understanding was ever actually verified.

Protocol documentation is deliberately separate from ADRs. ADRs record decisions the project has made and the reasoning behind them; once accepted, they evolve only through new architectural decisions — superseding or amending ADRs — not through ongoing edits to their content. Protocol documentation, by contrast, is a living record: it is expected to evolve continuously as understanding improves, with findings updated, strengthened, or corrected as new evidence comes in, without that ongoing change representing any kind of decision reversal. A finding moving from Probable to Confirmed, or an open question being resolved, is normal progress for a protocol document, but would be a strange thing to encode as a revision to an accepted architectural decision. Keeping the two separate lets ADRs stay a stable reference point while protocol knowledge is free to evolve.

Raw observations must be separated from interpretation because they carry different kinds of trust. A raw capture — the actual bytes seen on the wire, a GATT dump, a characteristic value snapshot — is simply what was observed and does not change once recorded. An interpretation of that capture (for example, "this write raises target temperature") is a claim about what it means, and that claim can turn out to be wrong or incomplete even when the underlying capture is accurate. If the two are combined, there is no way to later re-examine a conclusion against the original evidence, or to revise an interpretation without losing the data it was based on.

## Decision

**Protocol documentation lives under:**

```
docs/protocol/
```

**The intended structure is:**

- `docs/protocol/README.md`
- `docs/protocol/gatt-services.md`
- `docs/protocol/characteristics.md`
- `docs/protocol/commands.md`
- `docs/protocol/state-model.md`
- `docs/protocol/open-questions.md`
- `docs/protocol/captures/`

This structure is a planned layout only. Creating these files and directories is outside the scope of this ADR.

**Standard finding format**

Every protocol finding recorded under `docs/protocol/` uses the following format:

- **Finding ID** — a unique identifier for the finding, so it can be referenced from elsewhere (e.g. from code or other findings).
- **Date** — when the finding was recorded.
- **Hardware/software conditions** — the hardware, firmware, and/or official app version involved in the observation.
- **Observation** — what was actually observed happening.
- **Evidence** — a concise summary of what supports the conclusion (e.g. what was seen and why it's convincing).
- **Interpretation** — what the observation is understood to mean, in Volcano domain terms.
- **Confidence level** — Confirmed, Probable, or Unknown, per ADR-0005's classification.
- **Capture references** — the specific capture filename(s) or ID(s) under `docs/protocol/captures/` where the raw data supporting this finding is stored.
- **Implementation status** — whether and where this finding has been implemented in the Volcano component, if at all.

**Additional rules**

- Raw BLE captures are preserved separately from interpretation, stored under `docs/protocol/captures/` and never edited to fit a conclusion.
- Conclusions must always reference supporting observations; a finding without referenced evidence is not a valid finding.
- Unknown behaviour is documented explicitly, in `docs/protocol/open-questions.md`, rather than guessed at or silently left out.
- Open questions are tracked until sufficient evidence exists to resolve them, at which point they are converted into a proper finding in the relevant document and removed from `open-questions.md`.
- Findings may be revised as new evidence becomes available, but previous observations and capture data are never rewritten or discarded.

## Consequences

**Benefits**

- Protocol knowledge remains organised and maintainable as investigation grows, rather than fragmenting across ad hoc locations.
- Implementation can always be traced back to evidence, since every finding is required to reference the observations that support it.
- Easier debugging and future contributions, since anyone can trace a piece of Volcano component behaviour back to the finding and evidence that justified it.
- Supports the evidence-driven process established by ADR-0005 by giving it a concrete place and format to operate through.

**Trade-offs**

- Additional documentation overhead: every discovery needs to be written up in this format rather than left as a quick note.
- Requires discipline to keep documentation synchronised with discoveries, so findings and implementation status don't drift out of date as work continues.

## Alternatives considered

**1. Store protocol knowledge only in source code**

Rely on code comments and implementation to capture what's understood about the protocol, without a separate documentation record. Rejected because source code cannot capture all observations, failed experiments, or confidence levels — it can only represent what was ultimately implemented, losing everything about what was tried, ruled out, or still uncertain.

**2. Use one large protocol document**

Keep all findings, evidence, and open questions in a single Markdown file. Rejected because it becomes difficult to navigate and maintain as protocol knowledge grows, especially once services, characteristics, commands, and state findings all accumulate in the same place.

**3. Mix protocol discoveries with ADRs**

Record protocol findings as entries within the ADR series itself. Rejected because ADRs record project decisions, whereas protocol documentation records observed behaviour — the two have fundamentally different lifecycles and shouldn't be conflated.

## Notes

- Reference [ADR-0005](ADR-0005-volcano-ble-discovery-methodology.md) for the discovery methodology this documentation structure exists to support.
- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for where validated protocol knowledge is ultimately implemented.
