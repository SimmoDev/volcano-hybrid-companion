# Development guide

This document orients a new contributor: what this repository is, what stage it's at, how it's laid out, and how to validate the current scaffold locally. It doesn't repeat architectural reasoning that already lives in an ADR — it links to those instead.

## What this repository is

Volcano Hybrid Companion is a standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, built on ESPHome and ESP32 hardware. See the root [README.md](../README.md) for the full project vision, phased roadmap, and design principles.

## Current phase

The project is in **Phase 1**: establishing the BLE foundation and the Volcano component on an ESP32-S3-WROOM-1-N16R8 development board, per [ADR-0004](decisions/ADR-0004-development-hardware-strategy.md).

At this stage:

- **The Volcano BLE protocol is being implemented incrementally, read-only first.** The `volcano` component currently connects, resolves the status/flags register by UUID, subscribes, reads its initial value, decodes heater and pump state, and logs them — see `components/volcano/volcano.h` for the `TODO` markers showing where the remaining characteristics, writes, the device state model, and domain commands will go.
- **Protocol discovery is underway and follows [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md)**: findings are evidence-driven and classified Confirmed/Probable/Unknown, and only Confirmed findings back default implementation. See [`docs/protocol/`](protocol/README.md) for recorded findings.
- **The Volcano component's internal architecture follows [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md)**: BLE details stay inside the component, control interfaces depend on a hardware-independent interface, and the component owns the device state model.
- **ESPHome is the firmware framework**, per [ADR-0003](decisions/ADR-0003-esphome-as-firmware-framework.md). The `volcano` component is implemented as an ESPHome external component, and Home Assistant integration (when it exists, in Phase 3) will be an optional consumer of it, not a dependency.

## Repository structure

- **`components/volcano/`** — the Volcano ESPHome external component. See [`components/volcano/README.md`](../components/volcano/README.md) for its current implementation status and component-specific build notes.
- **`examples/`** — example ESPHome YAML configurations that exercise the components in this repository. These prove that a configuration loads, registers the component correctly, and compiles; they are not production device firmware.
- **`docs/decisions/`** — the ADR series. Each ADR records one architectural decision and its reasoning; see [ADR-0001](decisions/ADR-0001-project-vision.md) onward for the full set.
- **`docs/protocol/`** — Volcano BLE protocol findings, recorded per [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md).
- **`docs/CONVENTIONS.md`** — the living reference for terminology, spelling, Markdown/naming conventions, and commit message style used across this repository.

## Validating the current scaffold

The current example configuration is [`examples/esp32-s3-devkit-minimal.yaml`](../examples/esp32-s3-devkit-minimal.yaml). It loads the `volcano` component and targets the Phase 1 development board; it exposes no sensors or entities. It reads a BLE MAC address from `examples/secrets.yaml` (not committed — copy it from [`examples/secrets.yaml.example`](../examples/secrets.yaml.example)), but `esphome config`/`esphome compile` below need only a placeholder value, not a real device.

Requires the [ESPHome CLI](https://esphome.io/) installed locally. From the repository root:

```sh
# Validate the YAML and confirm the component registers correctly
esphome config examples/esp32-s3-devkit-minimal.yaml

# Compile the firmware (no physical device required for this step)
esphome compile examples/esp32-s3-devkit-minimal.yaml
```

Both commands should complete without errors. `esphome compile` is the closest available check that the component's C++ actually builds; it requires no physical ESP32-S3 hardware.
