# Examples

An example ESPHome configuration that exercises the `volcano` component. The device firmware the project will ship lives under [`firmware/`](../firmware/README.md); what is here is a scaffold — a compile check and a BLE-only test surface for the component on the Phase 1 development board. It is still a working config you can flash to real hardware — see [`docs/DEVELOPMENT.md`](../docs/DEVELOPMENT.md#validating-the-component-locally) for validating that it loads and compiles without a physical device. This document covers the next step: getting it running on real hardware and watching what it does.

## `esp32-s3-devkit.yaml`

Targets the Phase 1 development board ([ADR-0004](../docs/decisions/ADR-0004-development-hardware-strategy.md)) and exercises the `volcano` component's current BLE implementation — see [`components/volcano/README.md`](../components/volcano/README.md) for what it does. Its entities are the command controls and the state sensors described below, reachable both from the local `web_server` page and — per [ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md) — from Home Assistant through the ESPHome `api` component.

It needs a real Volcano's BLE MAC address to connect to anything. Copy [`secrets.yaml.example`](secrets.yaml.example) to `secrets.yaml` alongside it (not committed — see the repository's `.gitignore`) and set `volcano_mac_address` to your device's actual address. A placeholder value is fine for `esphome config`/`esphome compile`, but flashing it to hardware needs the real one to see anything happen.

It also joins your WiFi network — set `wifi_ssid`/`wifi_password` in the same `secrets.yaml` — so a browser on that network can reach the command controls described in "Sending commands" below and so Home Assistant can connect. Home Assistant also needs `api_encryption_key` set: generate one with `openssl rand -base64 32`. The committed placeholder is an obvious dummy — 32 zero bytes — so it is plainly not a real key, but it still has to be replaced before flashing: a device left with it has an API anyone with this repository can reach. The `volcano` component itself is BLE-only and has no WiFi dependency; WiFi exists in this scaffold only to serve those two interfaces.

The M5Stack Dial firmware exercises the same `volcano` component from a full local UI — see [`firmware/README.md`](../firmware/README.md).

## Home Assistant

This config declares an ESPHome `api` block ([ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md)), so Home Assistant's ESPHome integration discovers the device over WiFi and exposes the same entities the `web_server` page carries. Add the device in Home Assistant with the `api_encryption_key` from your `secrets.yaml`. It declares the identical `api` block the Dial firmware ships, and its `volcano` entity set is a strict subset of the Dial's, so the dev board is a way to exercise the Home Assistant entity surface without Dial hardware in hand. This was checked against a real Home Assistant instance: entities discovered and controllable, no fault observed.

The connection is not load-bearing. The config sets `reboot_timeout: 0s` on `api` *and* on `wifi`, so the device never reboots for want of an API client or a WiFi association — the same standalone decision the Dial firmware makes ([ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md)), applied here for consistency even though this scaffold has only network control surfaces to keep alive. The trade-off is that neither component will auto-reboot to recover from a wedged network stack.

The temperature entities are always Celsius, and Home Assistant converts them per its own unit settings (system-wide, or a per-entity override) because they carry a temperature device class — see [ADR-0008](../docs/decisions/ADR-0008-temperature-units-handling.md). The Volcano's "Display in Fahrenheit" switch is the *device's own screen* setting; it does not change what these entities report.

## Flashing and watching logs

Requires the [ESPHome CLI](https://esphome.io/) and an ESP32-S3 board connected over USB. From the repository root:

```sh
esphome run examples/esp32-s3-devkit.yaml
```

This compiles, flashes over USB, and opens the log monitor in one step — it'll prompt you to pick a serial port on first run. To flash and watch logs as separate steps instead:

```sh
esphome upload examples/esp32-s3-devkit.yaml
esphome logs examples/esp32-s3-devkit.yaml
```

`esphome logs` also re-attaches to an already-running device without reflashing it, which is the faster way back in after the first flash.

This config declares no `ota:` block, so every reflash is over USB — there is no over-the-air update path. That is a deliberate omission to keep the scaffold lean.

Watch for the `[volcano]` log tag: it logs heater/pump state, the auto-shutoff countdown, and current/target temperature on connect and whenever they change, including changes made at the device's own panel.

## Sending commands and watching state (`web_server`)

This config serves a local `web_server` page; the Dial firmware's is identical in shape, with one extra entity ([`firmware/README.md`](../firmware/README.md)). Once connected to WiFi, the device serves a local page at `http://<device-ip-or-hostname>/` (`volcano-dev-scaffold.local` by default, or check `esphome logs` for the IP it picked up on connect). The page has no authentication and no TLS; fine on a trusted home network for this development scaffold, not something to expose beyond it.

It exposes one control per value: a "Connected" binary sensor; "Heat" and "Air" switches for the heater and pump; number controls for the target temperature (°C), the auto-shutoff duration (minutes) and LED brightness (0–100); vibration, display-on-cooling and Fahrenheit toggles; read-only sensors for the current temperature and the auto-shutoff countdown; and a diagnostic group — the heater-runtime meter (hours and minutes of operation) and five fixed device-information strings (serial number, power supply, product line, firmware version and BLE firmware version), the strings read once when the connection is established. That list is a functional inventory, not the page's layout order — see the note on `sorting_weight` below for how the entities are actually grouped on screen. The two switches are named after the labels on the Volcano's own panel rather than this project's own terminology, so the two read the same side by side — see [`docs/CONVENTIONS.md`](../docs/CONVENTIONS.md#device-actuators-heater-and-pump-except-on-a-label).

"Connected" reflects the abstraction layer's connection state ([ADR-0009](../docs/decisions/ADR-0009-volcano-abstraction-layer-interface.md)): on, once the link is up *and* every characteristic's initial read has completed, not merely once the link exists. It is the one control worth trusting over the others during a disconnect: none of the switches above can represent "unknown" — ESPHome's switch entities have no such state — so each one simply keeps showing whatever it last reported rather than resetting. If "Connected" is off, treat every other control's displayed state as stale until it comes back on, rather than as current.

The serial number identifies your specific unit, in the same way as its BLE address, so it is worth keeping out of screenshots and pasted logs. Remove the `serial_number:` key if you would rather it were never read at all.

Everything updates live from BLE notifications, including changes made at the device's own panel — so setting a target there moves the same control you would set it with, and switching the heater on there flips the same switch that turns it on, without touching the page. The switches also follow changes the device makes on its own, such as switching its actuators off at auto-shutoff expiry.

Per [STATE-005](../docs/protocol/state-model.md#state-005--auto-shutoff-countdown), the duration write only takes effect at the *next* arming, not on a countdown already running: to see a new duration take effect, set it while the device is fully idle (heater off, pump off, countdown at 0), not mid-countdown. Note that the duration control and the countdown sensor are different values — the duration is what the countdown will load next time it arms.

Both number controls are set to `mode: box`, so they take a typed value rather than rendering as the default slider. That also keeps their unit label on the same baseline as the sensor rows, which the slider layout does not. Dropping the `mode` line gives sliders back.

The controls appear grouped by what they do rather than alphabetically, via a `sorting_weight` on each. That option needs `web_server` version 3, which is why the config sets it explicitly rather than taking the default.

Setting the LED brightness to `0` switches the Volcano's display off entirely rather than dimming it — no temperature and no status icons — which is easy to mistake for the device having gone wrong. Any non-zero value brings it straight back.

The target-temperature control spans 30–240 °C here — deliberately wider than the 40.0–230.0 °C range [CMD-001](../docs/protocol/commands.md#cmd-001--set-target-temperature) confirms the device accepts — so it can also exercise the component's own refusal of an out-of-range value: setting one logs a warning and writes nothing, rather than sending an unverified value to the device. That widening is this example's choice; the component defaults both controls to the confirmed ranges and enforces them regardless of what the control advertises.

### Troubleshooting

**No serial port found.** Hold the board's `BOOT` button while plugging in or resetting it to force download mode.

**Logs stay silent after the ROM boot banner** (`ESP-ROM:esp32s3-...`), with nothing from the application itself. Two distinct causes, both specific to this board's native USB:

- `esptool`'s post-flash reset ("Hard resetting via RTS pin") relies on DTR/RTS toggling `EN`/`GPIO0`, which works through an external USB-UART bridge chip but does nothing over this board's native USB peripheral. Press the physical reset button once flashing finishes.
- Some ESP32-S3 devkits, including this one, expose **two** USB-C ports: one wired to a UART-bridge chip, one wired straight to the chip's native USB peripheral. This config's logger needs the native port — usually labelled `USB`, not `COM`/`UART`. The ROM bootloader's own banner goes out over the physical UART either way, which is why the wrong port still looks like it's working right up until the application takes over logging.

**Nothing from the Volcano — no connection, no decoded state.** The device accepts only one connection at a time and stops advertising while connected ([CONN-003](../docs/protocol/gatt-services.md#conn-003--single-connection-at-a-time)); make sure it isn't already connected to the official app, and that `volcano_mac_address` in `secrets.yaml` is correct.

**Can't reach the command page.** Check `wifi_ssid`/`wifi_password` in `secrets.yaml` are correct, and that the machine browsing is on the same network the device joined — `esphome logs` prints the IP address once WiFi connects. `<hostname>.local` resolution needs mDNS support, which not every network/browser combination has; the numeric IP from the logs always works as a fallback.
