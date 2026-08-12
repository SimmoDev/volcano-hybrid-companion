# Volcano component

This is the `volcano` ESPHome external component. Per [ADR-0009](../../docs/decisions/ADR-0009-volcano-abstraction-layer-interface.md), it is three types, each with its own file pair:

- **`VolcanoBleClient`** (`volcano_ble_client.h`/`.cpp`) — the BLE communication layer ([ADR-0002](../../docs/decisions/ADR-0002-volcano-component-architecture.md)). Owns the `ble_client` node lifecycle's GATT detail ([ADR-0007](../../docs/decisions/ADR-0007-ble-connection-lifecycle.md)): resolving every characteristic by UUID, subscribing, reading, writing, and every wire encoding/decoding. No BLE type or characteristic handle ever appears above this layer.
- **`VolcanoDevice`** (`volcano_device.h`/`.cpp`) — the Volcano abstraction layer. Owns `VolcanoState`, the single authoritative record of device state, connection state (`DISCONNECTED`/`CONNECTING`/`READY`), and the requested-versus-confirmed handling every write goes through. Every field is a `DeviceValue<T>`: `is_known()` before trusting `value()`, and `requested()` while a write is outstanding. Has no BLE dependency and is not an ESPHome `Component`, so it can be constructed and exercised directly — including by a future control interface — without a Volcano, an ESP32, or ESPHome's runtime present.
- **`VolcanoComponent`** (`volcano.h`/`.cpp`) — the ESPHome integration. Owns `Component`/`ble_client::BLEClientNode` registration and the optional entities below, and does no protocol work of its own: it is a consumer of `VolcanoDevice`'s interface, on equal footing with any other control interface built against it.

`VolcanoDevice`'s interface, all Celsius/seconds/percent in the device's own units (never the wire's decidegrees):

```cpp
void set_target_temperature(float celsius);
void set_heater(bool on);
void set_pump(bool on);
void set_auto_shutoff_duration(uint16_t seconds);
void set_led_brightness(uint8_t percent);
void set_vibration(bool enabled);
void set_display_on_cooling(bool enabled);
void set_display_units_fahrenheit(bool fahrenheit);
```

Every temperature is Celsius, per [ADR-0008](../../docs/decisions/ADR-0008-temperature-units-handling.md) — presenting Fahrenheit is a control interface's concern, not this component's. Range enforcement (CMD-001's 40.0–230.0 °C, CMD-002's 0–100%, CMD-003's 60–21600 s) lives in `VolcanoBleClient`, the last gate before the wire; a command outside a confirmed range is refused and logged rather than sent. Vibration and display-on-cooling hide their registers' inverted bit polarity — the method and the device's observable behaviour agree, even though the bit written is the opposite.

A command is a request, never an immediate state change (ADR-0002): `set_heater(true)` does not make `device.heater().value()` become `true`. Each field resolves through whichever confirmation source the protocol actually gives it — heater, pump, vibration, display-on-cooling and display-units resolve from the device's own notification; target temperature, LED brightness and auto-shutoff duration have no such notification (STATE-013), so each is confirmed by an explicit read-back issued right after the write. Either way, `requested()` clears once the device answers — matching what was asked, confirming it; answering with something else (STATE-013's silent-drop case) still clears it, with `value()` landing on whatever the device actually reported — or after a 5-second timeout with no answer at all, in which case `value()` is left untouched.

`set_auto_shutoff_duration()` and `set_led_brightness()` are additionally refused (logged, nothing written) for the brief window between a connection starting and `connection_state()` reaching `READY`: both characteristics are also read once per connection as part of the initial state sweep, and a write's own read-back reads the same characteristic a second time — a control interface should treat a connection as writable only once it is `READY`, which every other command already assumes implicitly since nothing has a value to change before then.

Protocol behaviour to implement here is recorded in [`docs/protocol/`](../../docs/protocol/README.md). Per [ADR-0005](../../docs/decisions/ADR-0005-volcano-ble-discovery-methodology.md), only findings classified **Confirmed** back default production behaviour; Probable findings belong behind clearly marked experimental code, and Unknown behaviour must not be encoded as an assumption.

## Optional entities

Every value `VolcanoDevice` tracks can be exposed as a standard ESPHome entity, configured directly under the `volcano:` block — see "Configuration" below. Read-only values get a sensor or text_sensor; each writable one gets a single two-way entity that both reports the device's current value and sets a new one — a number for the temperatures and the duration, a switch for the heater and pump. `VolcanoComponent` is what wires these: an entity's `control()`/`write_state()` forwards into the matching `VolcanoDevice::set_*()`, and `VolcanoDevice`'s state-change callback publishes to whichever entities are configured, whenever a field they carry actually changes.

Unknown state is handled per what each ESPHome entity type can express. `sensor` and `number` publish `NAN` (shown as `NA` on a `web_server` page); `text_sensor` clears its `has_state()`. A `switch` cannot express unknown at all — ESPHome's switch entity has no such state — so the heater, pump, vibration, display-on-cooling and display-units switches simply hold whatever they last published across a disconnect, rather than resetting or being forced to a value. The `connected` entity below is what tells a consumer whether that held value is still trustworthy.

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

Each value has a matching optional key, all omitted by default, each accepting the same options as the underlying [`sensor`](https://esphome.io/components/sensor/#config-sensor), [`text_sensor`](https://esphome.io/components/text_sensor/#config-text-sensor), [`number`](https://esphome.io/components/number/#config-number), [`switch`](https://esphome.io/components/switch/#config-switch) or [`binary_sensor`](https://esphome.io/components/binary_sensor/#config-binary-sensor) platform (`name`, `id`, `mode`, etc.):

```yaml
volcano:
  ble_client_id: volcano_ble_client
  # Read-only (binary_sensor)
  connected:
    name: "Connected"
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
  display_units_fahrenheit:
    name: "Display in Fahrenheit"
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
| *(none)* | `connected`, `heater`, `pump`, `current_temperature`, `target_temperature`, `auto_shutoff_countdown` — connection state, live state and primary control |
| `config` | `auto_shutoff_duration`, `led_brightness`, `vibration`, `display_on_cooling`, `display_units_fahrenheit` — settings that configure how the device behaves |
| `diagnostic` | `hours_of_operation`, `minutes_of_operation` and the five device-information strings — information about the device |

`auto_shutoff_countdown` sits with the controls rather than the diagnostics despite being read-only: it is live operational state that changes every second, and it is the backstop against the heater running unattended, so it belongs where the actuator state is read. `connected` sits there too, despite also being read-only: it is what every other entity's currently-displayed value depends on being trustworthy, not background information about the device. The lifetime meters alongside the diagnostics change too slowly to inform anything in the moment. Every category can be overridden per entity in YAML.

None of the five switches ever publishes optimistically — the state shown is the one the device reported, not the one that was requested — and their restore mode is fixed to `DISABLED`. Restoring a remembered state would actuate the heater or pump at boot, before the device has said what it is actually doing. None of them can represent "unknown" either (see "Optional entities" above): each holds whatever it last published across a disconnect, with `connected` as the signal that a held value may no longer reflect the device.

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

`VolcanoDevice` additionally has its own host-side tests, needing neither ESPHome nor hardware — see [`test/`](test/) and [`docs/DEVELOPMENT.md`](../../docs/DEVELOPMENT.md#testing-volcanodevice).
