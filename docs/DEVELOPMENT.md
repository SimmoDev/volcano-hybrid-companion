# Development guide

This document orients a new contributor: what this repository is, what stage it's at, how it's laid out, and how to validate the component locally. It doesn't repeat architectural reasoning that already lives in an ADR — it links to those instead.

## What this repository is

Volcano Hybrid Companion is a standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, built on ESPHome and ESP32 hardware. See the root [README.md](../README.md) for the full project vision, phased roadmap, and design principles.

## Current phase

The project is in **Phase 1**: establishing the BLE foundation and the Volcano component on an ESP32-S3-WROOM-1-N16R8 development board, per [ADR-0004](decisions/ADR-0004-development-hardware-strategy.md).

At this stage:

- **The Volcano BLE protocol is being implemented incrementally.** The `volcano` component currently connects, resolves nineteen characteristics by UUID — the status/flags register, the auto-shutoff countdown and duration, current/target temperature, the heater/pump on/off triggers, the heater-runtime meter, LED brightness, the vibration and display-on-cooling settings, and five device-information strings — subscribes to the notify-capable ones, reads the rest once per connection, and decodes and logs them all. It also writes the auto-shutoff duration, the target temperature and the LED brightness, refusing values outside the ranges confirmed accepted in each case, plus the heater/pump on/off triggers, the vibration setting, display on cooling and the display units. Each value can optionally be exposed as an ESPHome entity — a sensor for the read-only ones, and for each writable one a single two-way entity that both reports and sets it (a number for the temperature and duration, a switch for the heater and pump) — so state and commands are reachable from a `web_server` page or Home Assistant without a hardware-independent interface existing yet. See `components/volcano/volcano.h` for the `TODO` markers showing where the remaining characteristics, the device state model, and the hardware-independent interface for control interfaces will go.
- **Protocol discovery is underway and follows [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md)**: findings are evidence-driven and classified Confirmed/Probable/Unknown, and only Confirmed findings back default implementation. See [`docs/protocol/`](protocol/README.md) for recorded findings.
- **The Volcano component's internal architecture follows [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md)**: BLE details stay inside the component, control interfaces depend on a hardware-independent interface, and the component owns the device state model.
- **Temperature is Celsius throughout the component**, per [ADR-0008](decisions/ADR-0008-temperature-units-handling.md), matching the wire encoding. The device's own Celsius/Fahrenheit display setting is exposed as an ordinary device setting and changes nothing the component reports; converting for presentation belongs to a control interface.
- **ESPHome is the firmware framework**, per [ADR-0003](decisions/ADR-0003-esphome-as-firmware-framework.md). The `volcano` component is implemented as an ESPHome external component, and Home Assistant integration (when it exists, in Phase 3) will be an optional consumer of it, not a dependency.

## Repository structure

- **`components/volcano/`** — the Volcano ESPHome external component. See [`components/volcano/README.md`](../components/volcano/README.md) for its current implementation status and component-specific build notes.
- **`examples/`** — example ESPHome YAML configurations that exercise the components in this repository; they are not production device firmware. See [`examples/README.md`](../examples/README.md) for what each one does and how to flash it to real hardware.
- **`docs/decisions/`** — the ADR series. Each ADR records one architectural decision and its reasoning; see [ADR-0001](decisions/ADR-0001-project-vision.md) onward for the full set.
- **`docs/protocol/`** — Volcano BLE protocol findings, recorded per [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md).
- **`docs/CONVENTIONS.md`** — the living reference for terminology, spelling, Markdown/naming conventions, and commit message style used across this repository.

## Validating the component locally

The current example configuration is [`examples/esp32-s3-devkit-minimal.yaml`](../examples/esp32-s3-devkit-minimal.yaml). It loads the `volcano` component and targets the Phase 1 development board, with manual command-trigger controls and state sensors — see [`examples/README.md`](../examples/README.md) for what they do. It reads a BLE MAC address from `examples/secrets.yaml` (not committed — copy it from [`examples/secrets.yaml.example`](../examples/secrets.yaml.example)), but `esphome config`/`esphome compile` below need only a placeholder value, not a real device.

Requires the [ESPHome CLI](https://esphome.io/) installed locally. From the repository root:

```sh
# Validate the YAML and confirm the component registers correctly
esphome config examples/esp32-s3-devkit-minimal.yaml

# Compile the firmware (no physical device required for this step)
esphome compile examples/esp32-s3-devkit-minimal.yaml
```

Both commands should complete without errors. `esphome compile` is the closest available check that the component's C++ actually builds; it requires no physical ESP32-S3 hardware.

To flash this example to real hardware and watch its logs, see [`examples/README.md`](../examples/README.md).
