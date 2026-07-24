# Volcano Hybrid Companion

An open-source, standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, built on ESPHome and ESP32 hardware.

This project is **not affiliated with, endorsed by, or supported by Storz & Bickel**. Bluetooth Low Energy (BLE) communication is being independently documented and implemented through observation and testing. Protocol behaviour will only be considered supported once it has been verified against real hardware.

## Status

Early planning stage. No firmware, components, or BLE protocol implementation exist yet. This README is the first project document; everything below describes intent and direction, not delivered functionality.

Initial development is taking place on an ESP32-S3-WROOM-1-N16R8 development board. Once the BLE implementation is mature, development will move to the M5Stack Dial for the user interface.

## Goals

Build a controller for the Volcano Hybrid that:

- Works as a fully standalone device — no phone app or cloud service required.
- Can optionally integrate with Home Assistant, without ever depending on it.
- Keeps the core Volcano communication logic reusable across hardware targets and UIs.

ESPHome provides the firmware framework this project is built on, and also provides the optional Home Assistant integration. The Volcano control logic lives inside a reusable ESPHome external component, kept independent of any particular UI or hardware platform.

## Development Phases

**Phase 1 — BLE foundation (current)**
- Develop on an ESP32-S3-WROOM-1-N16R8 development board.
- Use ESPHome as the firmware framework.
- Document and validate the Volcano Hybrid BLE protocol through observation and testing.
- Implement a hardware-independent Volcano communication/abstraction layer.
- Expose that layer as an ESPHome external component.

**Phase 2 — Local standalone remote**
- Port the working firmware to the M5Stack Dial.
- Add rotary encoder input, touchscreen, and a local UI.
- The Dial should fully control the Volcano with no external dependencies.

**Phase 3 — Home Assistant integration**
- Add Home Assistant integration through the ESPHome API.
- Home Assistant is an additional, optional control surface — never a requirement.
- The device must keep controlling the Volcano if Home Assistant is unreachable.

### Control Paths

Three independent control paths, all built on the same underlying Volcano abstraction layer:

1. M5Stack Dial local UI
2. Home Assistant
3. Direct ESPHome API / automation control

All control paths interact with the same underlying Volcano abstraction layer, ensuring consistent behaviour regardless of whether commands originate from the local UI, Home Assistant, or ESPHome automations.

## Design Principles

These constraints apply for the life of the project, not just Phase 1:

1. Do not tightly couple the Volcano BLE implementation to the UI.
2. Do not make Home Assistant a dependency.
3. Keep the Volcano communication layer hardware-independent.
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

This structure will be created incrementally, as each part is actually needed — it does not all exist yet.

```
volcano-hybrid-companion/
├── components/
│   └── volcano/      # ESPHome external component (Volcano abstraction layer)
├── examples/         # Example ESPHome configurations
├── docs/             # Design decisions, protocol notes, architecture
├── firmware/         # Device-specific firmware configs
└── README.md
```

## Contributing

Not yet open for contributions — the project is still defining its foundations. This will be updated once there is working code to contribute to.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.
