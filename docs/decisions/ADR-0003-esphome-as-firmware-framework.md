# ADR-0003: ESPHome as Firmware Framework

## Status

Accepted

## Context

[ADR-0001](ADR-0001-project-vision.md) commits the project to an ESP32-S3-based standalone controller, and [ADR-0002](ADR-0002-volcano-component-architecture.md) defines the Volcano component, whose abstraction layer control interfaces depend on, never the other way around. Neither decision specifies what firmware platform the device actually runs on. That choice needs to be made deliberately, since it shapes how the Volcano component is packaged, how BLE and networking are accessed, and how Home Assistant integration is eventually wired in — all without being allowed to compromise the boundary ADR-0002 already established.

A firmware framework decision is needed now, before any BLE or component code is written, because the framework determines the shape of the code from the very first line: how the project is structured, how configuration is expressed, and what "an external component" even means in this codebase.

**Relationship between ESPHome, the Volcano component, and Home Assistant**

ESPHome is a configuration-driven firmware framework for ESP32/ESP8266 devices that compiles YAML configuration plus C++ components into firmware, and separately offers a native API that Home Assistant (or any other client) can connect to. It is a firmware platform and an optional integration surface — it is not itself part of the Volcano domain. The Volcano component, per ADR-0002, is the device-specific intelligence that talks to the Volcano Hybrid and owns its state model; ESPHome is simply the platform that component runs on and one of the ways its state gets exposed. Home Assistant, in turn, is one optional consumer that reaches the Volcano component through ESPHome's API — it has no more privileged a position than the M5Stack Dial UI or a direct automation call.

## Decision

**ESPHome is the selected firmware framework** for this project. Firmware for both the Phase 1 ESP32-S3 development board and the Phase 2 M5Stack Dial will be built as ESPHome configurations/components rather than bespoke ESP-IDF or Arduino applications, subject to verifying that ESPHome provides sufficient support for the M5Stack Dial hardware requirements.

**The Volcano component is implemented as an ESPHome external component.** It lives inside ESPHome's external component system (per the project's planned `components/volcano/` layout) so it can be compiled into any ESPHome device configuration, but its internal design continues to follow ADR-0002 exactly: it owns BLE communication and device state, and exposes a hardware-independent interface that integrates with ESPHome's entity and service model without leaking BLE details upward.

**Home Assistant integration through the ESPHome API is optional.** ESPHome's native API is what Home Assistant connects to, but this is one consumer among the three control paths defined in ADR-0001. Enabling or disabling the `api` component in an ESPHome configuration must never change how the Volcano component itself behaves.

**Core Volcano control logic remains independent of ESPHome-specific and Home-Assistant-specific concerns.** The Volcano component may use ESPHome's component base classes and conventions to integrate with the framework, but it must not encode Home Assistant terminology, entity presentation choices, or assumptions about API connectivity into its core logic. The device must remain fully controllable — including from the M5Stack Dial's local UI in Phase 2 — with the ESPHome `api` component absent or disconnected.

## Consequences

**Benefits**

- Reduced boilerplate: ESPHome already provides WiFi, networking, OTA updates, logging, component lifecycle infrastructure, and a build/config pipeline, so the project doesn't need to reimplement this infrastructure.
- Established ESP32 ecosystem: ESPHome is actively maintained, widely used on ESP32-S3 hardware, and has existing BLE client support to build on rather than starting from raw BLE stack calls.
- Easier Home Assistant integration: Phase 3's Home Assistant support (per ADR-0001) is largely "enable the `api` component and expose entities," rather than writing and maintaining a custom integration protocol.
- Reusable external component model: ESPHome's external component mechanism is a natural fit for packaging the Volcano component as something that can be dropped into different device configurations (ESP32-S3 dev board now, M5Stack Dial later) without rewriting it.

**Trade-offs**

- Dependency on ESPHome's architecture and release cadence: the project inherits ESPHome's update cycle, breaking changes, and component API conventions rather than controlling its own foundation entirely.
- Limitations imposed by framework conventions: ESPHome's YAML-driven configuration and component lifecycle model constrain how the Volcano component is structured and initialised, compared to a bespoke application with full control over program flow.
- Possible need to work around ESPHome abstractions for specialised hardware behaviour: if the Volcano BLE protocol requires connection handling or timing that doesn't fit ESPHome's existing BLE client assumptions, the project may need to work within or extend those abstractions rather than implementing whatever is most direct.

## Alternatives considered

**1. Custom ESP-IDF firmware**

Write the firmware directly against Espressif's ESP-IDF, with full control over BLE stack usage, task scheduling, and memory layout. Offers maximum control. Rejected because it would require implementing, from scratch, infrastructure ESPHome already provides — configuration management, OTA updates, WiFi provisioning, logging, and a Home Assistant-compatible API — none of which is specific to the Volcano problem and all of which would need to be built and maintained just to reach parity with what ESPHome offers out of the box.

**2. Arduino framework without ESPHome**

Build firmware using the Arduino framework for ESP32 directly, implementing BLE handling with the Arduino BLE libraries. A possible BLE implementation path exists here. Rejected because Home Assistant integration and general device management (configuration, OTA, entity exposure) would require substantially more custom work than ESPHome already provides, without a corresponding benefit over ESPHome for this project's needs.

**3. Home Assistant-first architecture**

Treat Home Assistant as the primary control and integration point, with firmware built mainly to relay data to and from it. Rejected because it contradicts the standalone/offline-first requirements established in ADR-0001 — the device must keep controlling the Volcano Hybrid when Home Assistant is unreachable, which rules out any architecture where Home Assistant is anything more than one optional consumer.

## Notes

- Reference [ADR-0001](ADR-0001-project-vision.md) for the project vision and phase ordering that this framework choice must support.
- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for the Volcano component architecture boundary that this decision explicitly does not relax, even though the component is packaged as an ESPHome external component.
- This ADR does not specify the concrete ESPHome configuration layout, build targets, or component registration details — those are implementation decisions to be made once Phase 1 development begins.
- The Decision's "subject to verifying that ESPHome provides sufficient support for the M5Stack Dial hardware requirements" is resolved: [ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md) settled which ESPHome components the Dial's display/touch/encoder/buzzer hardware uses, and the resulting Dial UI is built and hardware-verified. Left as an open caveat here rather than rewritten, since this ADR's own Decision text is otherwise unchanged.
