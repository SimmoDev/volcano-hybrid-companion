# Examples

Example ESPHome configurations that exercise the components in this repository. These are working configs you can flash to real hardware, not just compile checks — see [`docs/DEVELOPMENT.md`](../docs/DEVELOPMENT.md#validating-the-component-locally) for validating that a config loads and compiles without a physical device. This document covers the next step: getting one running on real hardware and watching what it does.

## `esp32-s3-devkit-minimal.yaml`

Targets the Phase 1 development board ([ADR-0004](../docs/decisions/ADR-0004-development-hardware-strategy.md)) and exercises the `volcano` component's current BLE implementation — see [`components/volcano/README.md`](../components/volcano/README.md) for what it does. It exposes no sensors/entities and has no Home Assistant or UI logic.

It needs a real Volcano's BLE MAC address to connect to anything. Copy [`secrets.yaml.example`](secrets.yaml.example) to `secrets.yaml` alongside it (not committed — see the repository's `.gitignore`) and set `volcano_mac_address` to your device's actual address. A placeholder value is fine for `esphome config`/`esphome compile`, but flashing it to hardware needs the real one to see anything happen.

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

### Troubleshooting

**No serial port found.** Hold the board's `BOOT` button while plugging in or resetting it to force download mode.

**Logs stay silent after the ROM boot banner** (`ESP-ROM:esp32s3-...`), with nothing from the application itself. Two distinct causes, both specific to this board's native USB:

- `esptool`'s post-flash reset ("Hard resetting via RTS pin") relies on DTR/RTS toggling `EN`/`GPIO0`, which works through an external USB-UART bridge chip but does nothing over this board's native USB peripheral. Press the physical reset button once flashing finishes.
- Some ESP32-S3 devkits, including this one, expose **two** USB-C ports: one wired to a UART-bridge chip, one wired straight to the chip's native USB peripheral. This config's logger needs the native port — usually labelled `USB`, not `COM`/`UART`. The ROM bootloader's own banner goes out over the physical UART either way, which is why the wrong port still looks like it's working right up until the application takes over logging.

**Nothing from the Volcano — no connection, no decoded state.** The device accepts only one connection at a time and stops advertising while connected ([CONN-003](../docs/protocol/gatt-services.md#conn-003--single-connection-at-a-time)); make sure it isn't already connected to the official app, and that `volcano_mac_address` in `secrets.yaml` is correct.
