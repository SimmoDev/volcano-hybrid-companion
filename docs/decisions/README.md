# Architecture Decision Records

This directory holds the project's ADR series. Each ADR records one architectural decision and the reasoning behind it, using the section structure the existing records share (`## Status`, `## Context`, `## Decision`, `## Consequences`, `## Alternatives considered`, `## Notes`).

ADRs are static once accepted. A change of direction is made by a new ADR that supersedes an older one, not by rewriting it. Where later work has resolved a question an ADR left open, or corrected a statement in it without reversing the decision, that is recorded in the ADR's own `## Notes` rather than in its Decision text — see the Notes in [ADR-0003](ADR-0003-esphome-as-firmware-framework.md), [ADR-0007](ADR-0007-ble-connection-lifecycle.md), [ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md) and [ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md) for examples.

See [`../CONVENTIONS.md`](../CONVENTIONS.md#adrs-vs-this-document) for when a change warrants an ADR versus an edit to a living document, and for the ADR file-naming rule.

## The records

All are **Accepted**.

- **[ADR-0001](ADR-0001-project-vision.md) — Project Vision and Scope.** The three-phase plan (BLE foundation, local standalone remote, Home Assistant integration), the standalone-operation requirement, and Home Assistant kept optional throughout.
- **[ADR-0002](ADR-0002-volcano-component-architecture.md) — Volcano Component Architecture.** The one-way dependency from control interfaces down through the Volcano abstraction layer to the BLE communication layer, and the rule that the component owns the single authoritative state model.
- **[ADR-0003](ADR-0003-esphome-as-firmware-framework.md) — ESPHome as Firmware Framework.** ESPHome is the firmware platform; the Volcano component ships as an ESPHome external component; the native `api` is one optional consumer, never a dependency.
- **[ADR-0004](ADR-0004-development-hardware-strategy.md) — Development Hardware Strategy.** Phase 1 develops on a plain ESP32-S3 dev board rather than the Dial, so BLE protocol work is not entangled with display/touch/encoder bring-up.
- **[ADR-0005](ADR-0005-volcano-ble-discovery-methodology.md) — Volcano BLE Discovery Methodology.** Discovery is evidence-driven; every finding is classified Confirmed, Probable or Unknown, and only Confirmed findings back default production behaviour.
- **[ADR-0006](ADR-0006-protocol-documentation-structure.md) — Protocol Documentation Structure.** The fixed `docs/protocol/` file layout and the standard finding format (ID, observation, interpretation, confidence, implementation status).
- **[ADR-0007](ADR-0007-ble-connection-lifecycle.md) — BLE Connection Lifecycle.** The component holds a persistent BLE connection; releasing it back to the official app is a deliberate action a control interface must make discoverable.
- **[ADR-0008](ADR-0008-temperature-units-handling.md) — Temperature Units Handling.** The component's interface speaks Celsius only, matching the wire encoding; presenting Fahrenheit and its rounding are a control interface's concern.
- **[ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md) — Volcano Abstraction Layer Interface.** The three-type split (`VolcanoBleClient`, `VolcanoDevice`, `VolcanoComponent`), the per-field validity wrapper, and requested-versus-confirmed write handling.
- **[ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md) — Dial Hardware and UI Framework.** LVGL via ESPHome's `lvgl` component, and how the M5Stack Dial's display, touch panel, rotary encoder, button and buzzer are wired through ESPHome's own components.
- **[ADR-0011](ADR-0011-dial-ui-navigation-architecture.md) — Dial UI Navigation Architecture.** The Dial's fixed page set, the rotary/button/touch input semantics, rotary-write coalescing, and connection-state handling.
- **[ADR-0012](ADR-0012-home-assistant-integration.md) — Home Assistant Integration.** An encrypted `api` on both example configurations, the `api` and `wifi` reboot timeouts disabled so the device never reboots for want of a network, and a read-only Home Assistant status row on the Dial's Connections page.
