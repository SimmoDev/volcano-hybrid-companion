# ADR-0005: Volcano BLE Discovery and Validation Methodology

## Status

Accepted

## Context

The project depends on communicating with the Volcano Hybrid over Bluetooth Low Energy, but the protocol is not currently documented by this project. [ADR-0001](ADR-0001-project-vision.md) already establishes that protocol behaviour must be discovered through observation and testing rather than assumed, and [ADR-0002](ADR-0002-volcano-component-architecture.md) establishes that all BLE details belong inside the Volcano component and must never leak into control interfaces. Neither ADR defines *how* that discovery actually happens — what gets recorded, how confidence in a finding is tracked, or what is and isn't allowed to become implementation before it's verified. [ADR-0004](ADR-0004-development-hardware-strategy.md) gives this discovery work a home (the ESP32-S3 dev board, before any UI work begins), but not a process.

BLE protocol discovery requires a defined process because without one, discovery quietly turns into guesswork: a value observed once gets treated as "how the protocol works," a plausible-looking byte layout gets hardcoded because it seemed to work in one test, and there is no record afterward of whether a given piece of implementation is something the project actually verified or something someone assumed under time pressure. Assumptions are risky here specifically because the Volcano Hybrid is a real consumer device with real heating and airflow behaviour — an incorrect assumption about a command's meaning doesn't just produce a wrong log line, it can produce wrong physical behaviour on the device, and a wrong assumption baked into the Volcano component (per ADR-0002) becomes wrong behaviour for every control surface at once.

Evidence-driven development matters because it keeps the codebase honest about what it actually knows. A behaviour with supporting evidence can be trusted, re-verified, and debugged by returning to that evidence; a behaviour based on an unrecorded guess can only be re-examined by guessing again. Protocol understanding has to come before optimisation — there is nothing to optimise until the underlying behaviour is actually understood, and optimising an assumption just produces a more efficient wrong implementation.

## Decision

### Discovery process

- Observe existing Volcano Hybrid behaviour directly: interact with the device (official app, physical controls, or direct BLE tooling) while watching what it does.
- Capture BLE traffic and device behaviour for each observation session — e.g. BLE sniffer/logging output, GATT service and characteristic dumps, and notes on what physical action on the device produced what traffic.
- Record services, characteristics, notifications, commands, and state changes as they are identified, rather than only the final conclusion drawn from them.
- Maintain a protocol documentation record (a living document under `docs/protocol/`, separate from these ADRs) that accumulates these findings over time as the single reference for what is known about the protocol so far.

### Evidence classification

Every protocol finding has a current confidence classification from the following categories:

- **Confirmed** — directly observed and repeatable on real hardware. The same action reliably produces the same BLE traffic/behaviour across multiple attempts, ideally across multiple sessions.
- **Probable** — strongly indicated by multiple observations but not fully confirmed. For example, a pattern that has appeared consistently but hasn't yet been isolated from other variables, or has only been seen once but is consistent with confirmed behaviour elsewhere in the protocol.
- **Unknown** — insufficient evidence exists. Includes anything not yet investigated, and anything observed only ambiguously (e.g. a value changed, but it isn't clear why).

### Implementation rules

- Only **confirmed** behaviour becomes production implementation by default. This is what the Volcano component's mainline code is built against.
- **Probable** behaviour may exist behind clearly marked experimental code (e.g. an isolated code path, feature flag, or explicit comment/naming indicating its status), so it can be exercised and further tested without being presented as reliable, production-ready behaviour.
- **Unknown** behaviour must not be encoded as assumptions anywhere in the codebase. If a code path requires a decision about something unknown, that gap is left explicit (e.g. unimplemented, documented as a known gap) rather than filled with a guess.

### Documentation requirements

Each protocol finding recorded in the protocol documentation record must include:

- **Observation conditions** — what hardware, firmware/app version, and sequence of actions produced this observation.
- **Evidence collected** — the actual captured data (traffic logs, characteristic values, screenshots of official app state, etc.), or a reference to where it's stored.
- **Interpretation** — what the evidence is understood to mean in Volcano domain terms (e.g. "this characteristic write raises target temperature").
- **Confidence level** — one of Confirmed, Probable, or Unknown, per the classification above.

## Consequences

**Benefits**

- Reduces accidental protocol assumptions, since every piece of implemented behaviour has to trace back to a classified finding rather than "seemed to work."
- Creates traceability between observations and code: a given piece of Volcano component logic can be checked against the evidence that justified it.
- Makes future debugging easier, since a misbehaving feature can be traced back to whether its underlying protocol understanding was ever actually confirmed, or was probable/unknown all along.

**Trade-offs**

- Slower initial development, since every new piece of protocol behaviour requires observation and recording before it can be implemented, rather than being written directly from a guess.
- Requires maintaining documentation as an ongoing discipline alongside code, which is additional overhead compared to writing code alone.
- Some implementation decisions may be delayed until enough evidence accumulates to move a finding from Unknown/Probable to Confirmed, even when a plausible guess could have unblocked work sooner.

## Alternatives considered

**1. Implement while discovering**

Write BLE implementation code directly as protocol behaviour is being explored, without a separate recording/classification step. Rejected because assumptions become embedded directly in code with no record of how confident they were when written, and it becomes difficult later to distinguish verified behaviour from guesses that happened to work during testing.

**2. Use only existing third-party documentation**

Rely on any publicly available third-party notes or reverse-engineering write-ups about the Volcano Hybrid's BLE protocol instead of independently observing the device. Rejected because such documentation may be incomplete, outdated, or incorrect for this specific device/firmware, and any behaviour taken from it still requires validation against the actual Volcano Hybrid hardware before it can be trusted — at which point it is really just an unconfirmed starting hypothesis, not a substitute for this methodology.

**3. Reverse engineer without formal documentation**

Investigate the protocol informally — through exploration, scratch scripts, and code comments — without a maintained, structured record of findings and confidence levels. Rejected because knowledge then becomes dependent on individual memory and scattered code comments, which is difficult to maintain, hard for new contributors to pick up, and easy to lose track of as the protocol understanding grows.

## Notes

- Reference [ADR-0001](ADR-0001-project-vision.md) for the requirement that protocol behaviour be validated through observation and testing rather than assumed.
- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for the BLE ownership boundary this methodology's findings ultimately get implemented behind.
- Reference [ADR-0004](ADR-0004-development-hardware-strategy.md) for the development hardware (the ESP32-S3 dev board) this discovery work is expected to happen on.
- This ADR does not create the protocol documentation record itself, nor does it specify the tooling used to capture BLE traffic — those are implementation details to be established when discovery work actually begins.
