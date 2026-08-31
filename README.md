# Volcano Hybrid Companion

[![CI](https://github.com/SimmoDev/volcano-hybrid-companion/actions/workflows/ci.yml/badge.svg)](https://github.com/SimmoDev/volcano-hybrid-companion/actions/workflows/ci.yml)

An open-source, standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, built on ESPHome and ESP32 hardware.

This project is **not affiliated with, endorsed by, or supported by Storz & Bickel**. Bluetooth Low Energy (BLE) communication is being independently documented and implemented through observation and testing. Protocol behaviour will only be considered supported once it has been verified against real hardware.

## Status

Phases 1–3 are complete and verified against real hardware: the BLE foundation, the M5Stack Dial's local UI, and the optional Home Assistant integration. Phase 4 — packaging the Dial firmware as a browser-flashable release a Volcano owner can install without a development environment — is in progress, per [ADR-0013](docs/decisions/ADR-0013-release-and-distribution.md). The project is not yet released and the repository carries no version tag.

The `volcano` ESPHome component connects to the device and reads the auto-shutoff countdown, current temperature, the heater-runtime meter and the device-information strings, and both reports and sets heater and pump state, the target temperature, the auto-shutoff duration, LED brightness, and the vibration, display-on-cooling and display-units settings — all verified against real hardware. Per [ADR-0009](docs/decisions/ADR-0009-volcano-abstraction-layer-interface.md), the component is split into a BLE communication layer (`VolcanoBleClient`), the hardware-independent Volcano abstraction layer control interfaces depend on (`VolcanoDevice`), and a thin ESPHome integration (`VolcanoComponent`) — also verified against real hardware. BLE protocol discovery continues as a living document — see [`docs/protocol/`](docs/protocol/README.md) for recorded findings.

The M5Stack Dial's local UI is complete: every page [ADR-0011](docs/decisions/ADR-0011-dial-ui-navigation-architecture.md) names — Home, Navigation Menu, Connections, Settings, LED Brightness, Auto-Shutoff Duration, Dial Brightness, Dial Sound, About and Diagnostics — is built and verified against real hardware, per [ADR-0010](docs/decisions/ADR-0010-dial-hardware-and-ui-framework.md)'s hardware/UI framework choices and ADR-0011's navigation model. Connections is what lets a user release the BLE connection back to the official app and toggle WiFi, both independently of the Volcano. The Dial controls the Volcano fully on its own — no phone, browser, or Home Assistant required — and also retains the `web_server` page Phase 1 established, unchanged. The Dial's configuration is the firmware the project will ship, kept under [`firmware/`](firmware/README.md); see that document for what each page does. The ESP32-S3-WROOM-1-N16R8 development board remains available for isolating BLE-only issues from Dial-specific hardware, per [ADR-0004](docs/decisions/ADR-0004-development-hardware-strategy.md), with its scaffold config under [`examples/`](examples/README.md).

Phase 3 — optional Home Assistant integration — is complete. Both ESPHome configurations enable an encrypted `api`, the Dial's Connections page shows Home Assistant's connection status, and the standalone guarantee is verified on real hardware: the Dial keeps full control of the Volcano with Home Assistant stopped, the API key wrong, or WiFi absent, and never reboots for want of either. See [ADR-0012](docs/decisions/ADR-0012-home-assistant-integration.md).

Phase 4 — release and distribution — is in progress. The Dial firmware is feature-complete and versioned `1.0.0` internally, but installing it still needs the ESPHome toolchain and a hand-edited `secrets.yaml`. Phase 4 replaces that with a prebuilt binary flashed from a web browser, WiFi and device details entered on the device rather than compiled in, and an over-the-air update path. The `v1.0.0` git tag and the first GitHub Release are cut when that work lands. See [ADR-0013](docs/decisions/ADR-0013-release-and-distribution.md).

## Documentation

- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — how the repository is laid out and how to validate the component locally.
- [`firmware/README.md`](firmware/README.md) — the shipped M5Stack Dial firmware: what each page does, its Home Assistant integration, and how to flash it.
- [`examples/README.md`](examples/README.md) — the dev-board scaffold config that exercises the `volcano` component in isolation, and how to flash it.
- [`docs/protocol/`](docs/protocol/README.md) — what is known about the Volcano Hybrid's BLE protocol, and what is still open.
- [`docs/decisions/`](docs/decisions/README.md) — the ADR series recording each architectural decision and its reasoning.
- [`docs/CONVENTIONS.md`](docs/CONVENTIONS.md) — terminology, spelling, Markdown and naming conventions, and commit message style.

## Goals

Build a controller for the Volcano Hybrid that:

- Works as a fully standalone device — no phone app or cloud service required.
- Can optionally integrate with Home Assistant, without ever depending on it.
- Keeps the Volcano component's core logic reusable across hardware targets and UIs.

ESPHome provides the firmware framework this project is built on, and also provides the optional Home Assistant integration. The Volcano control logic lives inside a reusable ESPHome external component, kept independent of any particular UI or hardware platform.

## Development Phases

**Phase 1 — BLE foundation (complete)**
- Develop on an ESP32-S3-WROOM-1-N16R8 development board.
- Use ESPHome as the firmware framework.
- Document and validate the Volcano Hybrid BLE protocol through observation and testing.
- Implement the hardware-independent Volcano component.
- Expose it as an ESPHome external component.

**Phase 2 — Local standalone remote (complete)**
- Port the working firmware to the M5Stack Dial.
- Add rotary encoder input, touchscreen, and a local UI.
- The Dial should fully control the Volcano with no external dependencies.

**Phase 3 — Home Assistant integration (complete)**
- Add Home Assistant integration through the ESPHome API.
- Home Assistant is an additional, optional control interface — never a requirement.
- The device must keep controlling the Volcano if Home Assistant is unreachable.

**Phase 4 — Release and distribution (in progress)**
- Package the Dial firmware as a prebuilt binary, flashable from a web browser with no ESPHome toolchain.
- Move WiFi credentials, the Volcano's BLE address, and the API encryption key from compile time to on-device setup.
- Add an over-the-air update path to the Dial firmware.
- Cut the first tagged release, `v1.0.0`, and publish it as a GitHub Release.

### Control Paths

Three independent control paths, all built on the same underlying Volcano abstraction layer, ensuring consistent behaviour regardless of which one issues a command:

1. M5Stack Dial local UI
2. Home Assistant
3. Direct ESPHome API / automation control

## Design Principles

These constraints apply for the life of the project, not just Phase 1:

1. Do not tightly couple the Volcano BLE implementation to the UI.
2. Do not make Home Assistant a dependency.
3. Keep the Volcano component hardware-independent.
4. Prefer clean abstractions over quick hacks.
5. Document decisions before implementing them.
6. Avoid assumptions about the Volcano BLE protocol until verified.
7. Clearly distinguish confirmed facts from hypotheses.
8. Build incrementally and verify each stage before moving forward.
9. Prefer local, offline operation wherever possible.

## Technology Stack

**Established (Phase 1, complete)**
- ESPHome
- ESP32-S3
- C++
- YAML configuration
- Bluetooth LE

**Phase 2 (complete)**
- M5Stack Dial hardware
- Display UI
- Touch interface
- Rotary encoder

**Phase 3 (complete)**
- ESPHome native API, encrypted
- Home Assistant integration

**Phase 4 (in progress)**
- Browser-based flashing (WebSerial / ESP Web Tools)
- GitHub Releases and a hosted install page
- On-device provisioning
- ESPHome OTA updates

## Repository Structure

```
volcano-hybrid-companion/
├── components/
│   └── volcano/      # Volcano component, as an ESPHome external component
├── firmware/         # Shipped device firmware (the M5Stack Dial config)
├── examples/         # Dev-board scaffold config for exercising the component
├── docs/             # Design decisions, protocol notes, architecture
└── README.md
```

`firmware/` holds the M5Stack Dial configuration the project will ship (`m5stack-dial.yaml`, split into `dial/*.yaml` packages). `examples/` holds a single dev-board configuration that exercises the `volcano` component in isolation — a compile check and BLE-only test surface, not device firmware — kept available for BLE work per [ADR-0004](docs/decisions/ADR-0004-development-hardware-strategy.md).

## Contributing

Not yet open for contributions. Phases 1–3 — the BLE foundation, the M5Stack Dial UI, and the optional Home Assistant integration — are complete, and Phase 4 (packaging the first release) is in progress. There is not yet a contribution process — issue triage, review expectations, and so on — defined.

## Licence

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
