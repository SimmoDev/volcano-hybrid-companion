# Volcano Hybrid Companion

An open-source, standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, built on ESPHome and ESP32 hardware.

This project is **not affiliated with, endorsed by, or supported by Storz & Bickel**. Bluetooth Low Energy (BLE) communication is being independently documented and implemented through observation and testing. Protocol behaviour will only be considered supported once it has been verified against real hardware.

## Status

Early development. The `volcano` ESPHome component's BLE communication layer connects to the device and reads status, the auto-shutoff countdown, current temperature, the heater-runtime meter and the device-information strings, plus writes for the auto-shutoff duration, heater/pump on/off, target temperature, display brightness, and the vibration and display-on-cooling settings — all verified against real hardware. The hardware-independent Volcano abstraction layer that control interfaces will depend on doesn't exist yet. BLE protocol discovery is ongoing — see [`docs/protocol/`](docs/protocol/README.md) for recorded findings.

Initial development is taking place on an ESP32-S3-WROOM-1-N16R8 development board. Once the BLE implementation is mature, development will move to the M5Stack Dial for the user interface.

## Documentation

- [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) — how the repository is laid out and how to validate the component locally.
- [`docs/protocol/`](docs/protocol/README.md) — what is known about the Volcano Hybrid's BLE protocol, and what is still open.
- [`docs/decisions/`](docs/decisions/) — the ADR series recording each architectural decision and its reasoning.
- [`docs/CONVENTIONS.md`](docs/CONVENTIONS.md) — terminology, spelling, Markdown and naming conventions, and commit message style.

## Goals

Build a controller for the Volcano Hybrid that:

- Works as a fully standalone device — no phone app or cloud service required.
- Can optionally integrate with Home Assistant, without ever depending on it.
- Keeps the Volcano component's core logic reusable across hardware targets and UIs.

ESPHome provides the firmware framework this project is built on, and also provides the optional Home Assistant integration. The Volcano control logic lives inside a reusable ESPHome external component, kept independent of any particular UI or hardware platform.

## Development Phases

**Phase 1 — BLE foundation (current)**
- Develop on an ESP32-S3-WROOM-1-N16R8 development board.
- Use ESPHome as the firmware framework.
- Document and validate the Volcano Hybrid BLE protocol through observation and testing.
- Implement the hardware-independent Volcano component.
- Expose it as an ESPHome external component.

**Phase 2 — Local standalone remote**
- Port the working firmware to the M5Stack Dial.
- Add rotary encoder input, touchscreen, and a local UI.
- The Dial should fully control the Volcano with no external dependencies.

**Phase 3 — Home Assistant integration**
- Add Home Assistant integration through the ESPHome API.
- Home Assistant is an additional, optional control interface — never a requirement.
- The device must keep controlling the Volcano if Home Assistant is unreachable.

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

**Primary (Phase 1)**
- ESPHome
- ESP32-S3
- C++
- YAML configuration
- Bluetooth LE

**Future (Phase 2+)**
- M5Stack Dial hardware
- Display UI
- Touch interface
- Rotary encoder

## Planned Repository Structure

The remainder of this structure will be created incrementally, as each part is actually needed; `firmware/` does not exist yet.

```
volcano-hybrid-companion/
├── components/
│   └── volcano/      # Volcano component, as an ESPHome external component
├── examples/         # Example ESPHome configurations
├── docs/             # Design decisions, protocol notes, architecture
├── firmware/         # Device-specific firmware configs
└── README.md
```

## Contributing

Not yet open for contributions — the project is still early: only a partial BLE implementation exists, and there's no hardware-independent interface yet for a contributor to build against. This will be revisited as the foundations mature.

## Licence

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
