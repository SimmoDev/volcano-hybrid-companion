# Development guide

This document orients a new contributor: what this repository is, what stage it's at, how it's laid out, and how to validate the current scaffold locally. It doesn't repeat architectural reasoning that already lives in an ADR — it links to those instead.

## What this repository is

Volcano Hybrid Companion is a standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, built on ESPHome and ESP32 hardware. See the root [README.md](../README.md) for the full project vision, phased roadmap, and design principles.

## Current phase

The project is in **Phase 1**: establishing the BLE foundation and the Volcano component on an ESP32-S3-WROOM-1-N16R8 development board, per [ADR-0004](decisions/ADR-0004-development-hardware-strategy.md).

At this stage:

- **The Volcano BLE protocol has not yet been implemented.** The `volcano` component is currently a scaffold that compiles and registers as a valid ESPHome component — see `components/volcano/volcano.h` for the `TODO` markers showing where BLE communication, the device state model, and domain commands will eventually go.
- **Protocol discovery, once it starts, follows [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md)**: findings must be evidence-driven and classified Confirmed/Probable/Unknown before becoming default implementation.
- **The Volcano component's internal architecture follows [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md)**: BLE details stay inside the component, control interfaces depend on a hardware-independent interface, and the component owns the device state model.
- **ESPHome is the firmware framework**, per [ADR-0003](decisions/ADR-0003-esphome-as-firmware-framework.md). The `volcano` component is implemented as an ESPHome external component, and Home Assistant integration (when it exists, in Phase 3) will be an optional consumer of it, not a dependency.

## Repository structure

- **`components/volcano/`** — the Volcano ESPHome external component. Currently a scaffold `VolcanoComponent` with no BLE, protocol, or domain logic yet. See [`components/volcano/README.md`](../components/volcano/README.md) for component-specific build notes.
- **`examples/`** — example ESPHome YAML configurations that exercise the components in this repository. These prove that a configuration loads, registers the components correctly, and compiles; they are not production device firmware.
- **`docs/decisions/`** — the ADR series. Each ADR records one architectural decision and its reasoning; see [ADR-0001](decisions/ADR-0001-project-vision.md) onward for the full set.
- **`docs/protocol/`** — where Volcano BLE protocol findings will be recorded once discovery begins, per [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md). This directory has not yet been created in the repository.
- **`docs/CONVENTIONS.md`** — the living reference for terminology, spelling, Markdown/naming conventions, and commit message style used across this repository.

## Validating the current scaffold

The current example configuration is [`examples/esp32-s3-devkit-minimal.yaml`](../examples/esp32-s3-devkit-minimal.yaml). It loads the `volcano` component and targets the Phase 1 development board; it exposes no sensors or entities.

Requires the [ESPHome CLI](https://esphome.io/) installed locally. From the repository root:

```sh
# Validate the YAML and confirm the component registers correctly
esphome config examples/esp32-s3-devkit-minimal.yaml

# Compile the firmware (no physical device required for this step)
esphome compile examples/esp32-s3-devkit-minimal.yaml
```

Both commands should complete without errors. `esphome compile` is the closest available check that the component's C++ actually builds, since there is no physical ESP32-S3 hardware requirement to run it.
