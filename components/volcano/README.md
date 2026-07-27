# Volcano component

This is the `volcano` ESPHome external component. It is a `ble_client` node ([ADR-0007](../../docs/decisions/ADR-0007-ble-connection-lifecycle.md)) that currently implements a first, read-only increment: on each connection it resolves the status/flags register (`CHAR-008`) by UUID, subscribes, reads its initial value, decodes heater and pump state, and logs them. It issues no writes to any characteristic. See `volcano.h` for the `TODO` markers showing where the remaining protocol coverage and the Volcano abstraction layer ([ADR-0002](../../docs/decisions/ADR-0002-volcano-component-architecture.md)) belong.

Protocol behaviour to implement here is recorded in [`docs/protocol/`](../../docs/protocol/README.md). Per [ADR-0005](../../docs/decisions/ADR-0005-volcano-ble-discovery-methodology.md), only findings classified **Confirmed** back default production behaviour; Probable findings belong behind clearly marked experimental code, and Unknown behaviour must not be encoded as an assumption.

## Configuration

The component attaches to a `ble_client` you configure separately, identifying the device by its BLE MAC address:

```yaml
ble_client:
  - mac_address: !secret volcano_mac_address
    id: volcano_ble_client

volcano:
  ble_client_id: volcano_ble_client
```

The Volcano's BLE address is unique per unit and is deliberately never recorded in this repository (see [`docs/protocol/README.md`](../../docs/protocol/README.md)) — keep it in `secrets.yaml`, not in a committed config.

## Building / validating

See [`docs/DEVELOPMENT.md`](../../docs/DEVELOPMENT.md#validating-the-current-scaffold) for how to validate this component against the example configuration.
