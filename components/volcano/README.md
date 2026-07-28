# Volcano component

This is the `volcano` ESPHome external component. It is a `ble_client` node ([ADR-0007](../../docs/decisions/ADR-0007-ble-connection-lifecycle.md)) that currently implements:

- **Read-only state**: on each connection it resolves the status/flags register (`CHAR-008`), the auto-shutoff countdown (`CHAR-016`), and current/target temperature (`CHAR-013`/`CHAR-014`) by UUID, subscribes, reads their initial values, decodes them, and logs them. Current temperature's sub-40 °C "no reading" gate (STATE-012) is decoded explicitly rather than logged as a false `0.0` reading.
- **Writes**: `set_auto_shutoff_duration_seconds()` writes the auto-shutoff duration (`CHAR-017`), refusing anything below 60 seconds — the floor CMD-003 confirms the device accepts, loads at the next arming, and honours through to an actual expiry. Values below that are unverified and are not written. `turn_heater_on()`/`turn_heater_off()`/`turn_pump_on()`/`turn_pump_off()` write the four one-byte trigger characteristics (`CHAR-018`–`CHAR-021`), each with the single value CMD-006 through CMD-009 confirm those characteristics accept. `set_target_temperature_decidegrees()` writes the target temperature (`CHAR-014`), refusing anything outside the 40.0–230.0 °C range CMD-001 confirms the official app's UI accepts.

- **Optional entities**: every decoded value above can be published to a standard ESPHome sensor/binary_sensor, configured directly under the `volcano:` block — see "Configuration" below. This is a stopgap for observing state (e.g. on an ESPHome `web_server` page or in Home Assistant), not the hardware-independent Volcano domain interface control interfaces will eventually depend on.

See `volcano.h` for the `TODO` markers showing where the remaining protocol coverage and the Volcano abstraction layer ([ADR-0002](../../docs/decisions/ADR-0002-volcano-component-architecture.md)) belong.

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

Each decoded value has a matching optional key, all omitted by default, each accepting the same options as the underlying [`sensor`](https://esphome.io/components/sensor/#config-sensor) or [`binary_sensor`](https://esphome.io/components/binary_sensor/#config-binary-sensor) platform (`name`, `id`, etc.):

```yaml
volcano:
  ble_client_id: volcano_ble_client
  current_temperature:
    name: "Current temperature"
  target_temperature:
    name: "Target temperature"
  auto_shutoff_countdown:
    name: "Auto-shutoff countdown"
  heater:
    name: "Heater"
  pump:
    name: "Pump"
```

## Building / validating

See [`docs/DEVELOPMENT.md`](../../docs/DEVELOPMENT.md#validating-the-component-locally) for how to validate this component against the example configuration.
