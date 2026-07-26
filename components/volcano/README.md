# Volcano component (scaffold)

This is the `volcano` ESPHome external component. It is currently a scaffold: it registers a valid, empty `VolcanoComponent` with no BLE communication, no Volcano protocol handling, and no domain logic yet. See `volcano.h` for the `TODO` markers showing where that work belongs, and [ADR-0002](../../docs/decisions/ADR-0002-volcano-component-architecture.md) for the architectural boundary it must respect.

Protocol behaviour to implement here is recorded in [`docs/protocol/`](../../docs/protocol/README.md). Per [ADR-0005](../../docs/decisions/ADR-0005-volcano-ble-discovery-methodology.md), only findings classified **Confirmed** back default production behaviour; Probable findings belong behind clearly marked experimental code, and Unknown behaviour must not be encoded as an assumption.

## Building / validating

See [`docs/DEVELOPMENT.md`](../../docs/DEVELOPMENT.md#validating-the-current-scaffold) for how to validate this component against the example configuration.
