# Volcano Hybrid protocol documentation

This directory records what is actually known about the Volcano Hybrid's Bluetooth Low Energy (BLE) protocol, per the structure defined in [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md).

**Status: no BLE discovery has started yet.** Every file below is a placeholder describing what it is for, not a record of any findings. Nothing here should be read as confirmed (or even hypothesised) Volcano protocol behaviour.

## What lives here

- [`gatt-services.md`](gatt-services.md) — findings about the GATT services the Volcano Hybrid exposes.
- [`characteristics.md`](characteristics.md) — findings about individual characteristics within those services.
- [`commands.md`](commands.md) — findings about writes/commands and their observed effects.
- [`state-model.md`](state-model.md) — findings about how device state (temperature, heater, valve, etc.) is observed and reported.
- [`open-questions.md`](open-questions.md) — unresolved questions, tracked until enough evidence exists to turn them into a finding.
- [`captures/`](captures/README.md) — raw, unmodified BLE captures that findings cite as evidence.

## How this documentation works

This is a summary only — see [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md) and [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md) for the full methodology and structure this directory follows.

- Discovery is evidence-driven: nothing is recorded as protocol behaviour until it has been observed.
- Every finding is classified **Confirmed**, **Probable**, or **Unknown**, and only Confirmed findings back default production behaviour in the Volcano component.
- Raw captures (`captures/`) are kept separate from interpretation (`gatt-services.md`, `characteristics.md`, `commands.md`, `state-model.md`): a capture is recorded once, unmodified, and findings reference it rather than embedding or restating it.
- See [`docs/CONVENTIONS.md`](../CONVENTIONS.md) for the finding ID format (`<area>-NNN`) and capture filename format (`YYYY-MM-DD-<short-description>.<ext>`) used across these files.
