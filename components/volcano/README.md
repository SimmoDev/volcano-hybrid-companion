# Volcano component

This is the `volcano` ESPHome external component. It is a `ble_client` node ([ADR-0007](../../docs/decisions/ADR-0007-ble-connection-lifecycle.md)) that currently implements:

- **Read-only state**: on each connection it resolves the status/flags register (`CHAR-008`), the auto-shutoff countdown (`CHAR-016`), current/target temperature (`CHAR-013`/`CHAR-014`) and the heater-runtime meter (`CHAR-022`/`CHAR-023`) by UUID, subscribes, reads their initial values, decodes them, and logs them. Current temperature's sub-40 °C "no reading" gate (STATE-012) is decoded explicitly rather than logged as a false `0.0` reading.
- **Vibration**: `set_vibration()` writes the vibration setting (`CHAR-010`) — whether the device buzzes on reaching temperature — using the mask-and-action form CMD-004 confirms. The register bit is inverted (clear means enabled), which the component hides: the entity and method both read and write in the obvious sense. The register notifies, so it is subscribed rather than re-read after each write.
- **Display on cooling**: `set_display_on_cooling()` writes the setting (`CHAR-009`) governing whether the device shows current temperature on its own display while cooling, using the same mask-and-action form and naming only that bit — the register's units bit and its unidentified bits are left alone. Display-only: current temperature keeps notifying over BLE either way. Its register notifies on every 1 °C temperature change, so the component logs only actual changes to the setting.
- **LED brightness**: `set_led_brightness_percent()` writes the display brightness (`CHAR-015`) on the 0–100 scale CMD-002 records, read once per connection and re-read after each write. `0` switches the display off entirely rather than dimming it; any non-zero value restores it.
- **Device information**: firmware version (`CHAR-005`), firmware BLE version (`CHAR-006`), serial number (`CHAR-007`), power supply rating (`CHAR-024`) and product line name (`CHAR-025`) are read once per connection. None of them notifies, so each is read explicitly; they are read one at a time rather than all at once, since a GATT client has only a small number of outstanding reads available. Three of the five are writable on the device and none is ever written here — writing `CHAR-007` would replace a real unit's serial number.
- **Writes**: `set_auto_shutoff_duration_seconds()` writes the auto-shutoff duration (`CHAR-017`), refusing anything outside 60–21600 seconds — the range CMD-003 confirms accepted, from the floor this project verified through to an actual expiry up to the top of the official app's own range. Values outside it are unverified and are not written. `turn_heater_on()`/`turn_heater_off()`/`turn_pump_on()`/`turn_pump_off()` write the four one-byte trigger characteristics (`CHAR-018`–`CHAR-021`), each with the single value CMD-006 through CMD-009 confirm those characteristics accept. `set_target_temperature_decidegrees()` writes the target temperature (`CHAR-014`), refusing anything outside the 40.0–230.0 °C range CMD-001 confirms the official app's UI accepts.

- **Optional entities**: every value above can be exposed as a standard ESPHome entity, configured directly under the `volcano:` block — see "Configuration" below. Read-only values get a sensor or text_sensor; each writable one gets a single two-way entity that both reports the device's current value and sets a new one — a number for the temperatures and the duration, a switch for the heater and pump. This is a stopgap for observing and exercising state (e.g. on an ESPHome `web_server` page or in Home Assistant), not the hardware-independent Volcano domain interface control interfaces will eventually depend on.

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

Each value has a matching optional key, all omitted by default, each accepting the same options as the underlying [`sensor`](https://esphome.io/components/sensor/#config-sensor), [`text_sensor`](https://esphome.io/components/text_sensor/#config-text-sensor), [`number`](https://esphome.io/components/number/#config-number) or [`switch`](https://esphome.io/components/switch/#config-switch) platform (`name`, `id`, `mode`, etc.):

```yaml
volcano:
  ble_client_id: volcano_ble_client
  # Read-only (sensor)
  current_temperature:
    name: "Current temperature"
  auto_shutoff_countdown:
    name: "Auto-shutoff countdown"
  hours_of_operation:
    name: "Hours of operation"
  minutes_of_operation:
    name: "Minutes of operation"
  # Read-only (text_sensor)
  firmware_version:
    name: "Firmware version"
  ble_firmware_version:
    name: "BLE firmware version"
  serial_number:
    name: "Serial number"
  power_supply:
    name: "Power supply"
  product_line:
    name: "Product line"
  # Read/write (number)
  led_brightness:
    name: "LED brightness"
  target_temperature:
    name: "Target temperature"
  auto_shutoff_duration:
    name: "Auto-shutoff duration"
  # Read/write (switch)
  vibration:
    name: "Vibration"
  display_on_cooling:
    name: "Display on cooling"
  heater:
    name: "Heat"
  pump:
    name: "Air"
```

Every writable value is one entity, not a readout plus a separate setter: setting it writes to the device, and the component publishes back to it whenever the device reports a change — including changes made at the device's own control panel, and changes the device makes on its own, such as switching its actuators off at auto-shutoff expiry.

The heater and pump entities are named "Heat" and "Air" above to match the labels on the Volcano's own panel and in the official app; the keys stay `heater`/`pump`, which is this project's terminology and matches the device firmware's own identifiers. See [`docs/CONVENTIONS.md`](../../docs/CONVENTIONS.md#device-actuators-heater-and-pump-except-on-a-label).

`auto_shutoff_duration` is in **minutes**, the unit the official app presents it in, converted to the seconds `CHAR-017` encodes. It is distinct from `auto_shutoff_countdown`, which is the live time remaining and is read-only; the duration is what that countdown will load the next time it arms.

The two numbers accept `min_value`, `max_value` and `step`, defaulting to the ranges the protocol documentation records as confirmed accepted (40–230 °C and 1–360 minutes). Widening them does not widen what reaches the device: the component checks every write against the confirmed range itself and refuses anything outside it, whatever the entity advertises.

`hours_of_operation` and `minutes_of_operation` are the two halves of one meter: minutes counts 0–59 and carries into hours, and both advance only while the heater is on rather than tracking wall-clock time.

Entities default to an entity category reflecting what they are, so a UI can group them:

| Category | Entities |
|---|---|
| *(none)* | `heater`, `pump`, `current_temperature`, `target_temperature`, `auto_shutoff_countdown` — live state and primary control |
| `config` | `auto_shutoff_duration`, `led_brightness`, `vibration`, `display_on_cooling` — settings that configure how the device behaves |
| `diagnostic` | `hours_of_operation`, `minutes_of_operation` and the five device-information strings — information about the device |

`auto_shutoff_countdown` sits with the controls rather than the diagnostics despite being read-only: it is live operational state that changes every second, and it is the backstop against the heater running unattended, so it belongs where the actuator state is read. The lifetime meters alongside it in `diagnostic` change too slowly to inform anything in the moment. Every category can be overridden per entity in YAML.

The two switches never publish optimistically — the state shown is the one the device reported, not the one that was requested — and their restore mode is fixed to `DISABLED`. Restoring a remembered state would actuate the heater or pump at boot, before the device has said what it is actually doing.

## Notification limits

Every characteristic this component subscribes to consumes one of a fixed pool of GATT notification registrations, shared across everything on the same GATT client interface. The pool holds 12 by default; the component currently uses 8 of them, so a configuration that adds further notifying characteristics of its own can exhaust it. Registrations beyond the limit fail with `ESP_GATT_NO_RESOURCES` (status 128) — the failure is logged, but the characteristic simply never notifies afterwards.

Raise the pool on the `esp32_ble` component:

```yaml
esp32_ble:
  max_notifications: 16
```

Setting `CONFIG_BT_GATTC_NOTIF_REG_MAX` through `sdkconfig_options` instead does **not** work: `esp32_ble` writes that same option itself from `max_notifications`, after any raw override, so a value set that way is discarded with no warning. The device has 16 notify-capable characteristics in total, which is the practical ceiling worth requesting.

## Building / validating

See [`docs/DEVELOPMENT.md`](../../docs/DEVELOPMENT.md#validating-the-component-locally) for how to validate this component against the example configuration.
