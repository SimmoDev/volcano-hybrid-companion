# Examples

Example ESPHome configurations that exercise the components in this repository. These are working configs you can flash to real hardware, not just compile checks — see [`docs/DEVELOPMENT.md`](../docs/DEVELOPMENT.md#validating-the-component-locally) for validating that a config loads and compiles without a physical device. This document covers the next step: getting one running on real hardware and watching what it does.

## `esp32-s3-devkit-minimal.yaml`

Targets the Phase 1 development board ([ADR-0004](../docs/decisions/ADR-0004-development-hardware-strategy.md)) and exercises the `volcano` component's current BLE implementation — see [`components/volcano/README.md`](../components/volcano/README.md) for what it does. Its entities are the command controls and the state sensors described below, and it has no Home Assistant integration.

It needs a real Volcano's BLE MAC address to connect to anything. Copy [`secrets.yaml.example`](secrets.yaml.example) to `secrets.yaml` alongside it (not committed — see the repository's `.gitignore`) and set `volcano_mac_address` to your device's actual address. A placeholder value is fine for `esphome config`/`esphome compile`, but flashing it to hardware needs the real one to see anything happen.

It also joins your WiFi network — set `wifi_ssid`/`wifi_password` in the same `secrets.yaml` — so a browser on that network can reach the command controls described in "Sending commands" below. The `volcano` component itself is BLE-only and has no WiFi dependency; WiFi exists in this example purely to serve that page.

## Flashing and watching logs

Requires the [ESPHome CLI](https://esphome.io/) and an ESP32-S3 board connected over USB. From the repository root:

```sh
esphome run examples/esp32-s3-devkit-minimal.yaml
```

This compiles, flashes over USB, and opens the log monitor in one step — it'll prompt you to pick a serial port on first run. To flash and watch logs as separate steps instead:

```sh
esphome upload examples/esp32-s3-devkit-minimal.yaml
esphome logs examples/esp32-s3-devkit-minimal.yaml
```

`esphome logs` also re-attaches to an already-running device without reflashing it, which is the faster way back in after the first flash.

Watch for the `[volcano]` log tag: it logs heater/pump state, the auto-shutoff countdown, and current/target temperature on connect and whenever they change, including changes made at the device's own panel.

## Sending commands and watching state

Once connected to WiFi, the device serves a local page at `http://<device-ip-or-hostname>/` (`volcano-dev-scaffold.local` by default, or check `esphome logs` for the IP it picked up on connect). The page has no authentication and no TLS; fine on a trusted home network for this development example, not something to expose beyond it.

It exposes one control per value: "Heat" and "Air" switches for the heater and pump; number controls for the target temperature (°C) and the auto-shutoff duration (minutes); an LED brightness control (0–100); and read-only sensors for the current temperature and the auto-shutoff countdown. Below those sit a diagnostic group: the heater-runtime meter (hours and minutes of operation) and five fixed device-information strings — firmware version, BLE firmware version, serial number, power supply and product line — read once when the connection is established. The two switches are named after the labels on the Volcano's own panel rather than this project's own terminology, so the two read the same side by side — see [`docs/CONVENTIONS.md`](../docs/CONVENTIONS.md#device-actuators-heater-and-pump-except-on-a-label).

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
