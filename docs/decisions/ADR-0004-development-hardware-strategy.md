# ADR-0004: Development Hardware Strategy

## Status

Accepted

## Context

[ADR-0001](ADR-0001-project-vision.md) establishes two hardware-relevant phases: Phase 1 develops on an ESP32-S3-WROOM-1-N16R8 development board, and Phase 2 ports the working firmware to the M5Stack Dial. Neither that ADR nor [ADR-0002](ADR-0002-volcano-component-architecture.md) or [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) explains *why* development should begin on the dev board rather than directly on the Dial. That choice matters enough to record on its own: which board is in hand while the BLE protocol is still unknown determines how hard it is to observe, debug, and correct that protocol work.

The project has two stages because the two hardware targets solve different problems. The ESP32-S3-WROOM-1-N16R8 development board is a simple, general-purpose development platform — easy to reflash, reset, and probe, with no display, touch, or encoder to initialise or get in the way. The M5Stack Dial is the intended user-facing remote platform: it adds a round touchscreen, rotary encoder, and a specific physical form factor, all of which are irrelevant to whether the Volcano BLE protocol has been correctly understood.

BLE discovery should happen before UI development because it is the riskiest and least understood part of the project — per ADR-0001, the protocol is being documented and validated through observation and testing, not assumed. Debugging BLE behaviour (connection drops, unexpected notification payloads, timing issues) is already hard; doing it while also bringing up unfamiliar display/touch/encoder drivers on new hardware would conflate two independent sources of failure and slow down the one piece of work — the Volcano component itself — that every later phase depends on.

## Decision

**The ESP32-S3-WROOM-1-N16R8 development board is the Phase 1 development platform.** All BLE protocol investigation, validation, and initial Volcano component implementation happen on this board.

**The M5Stack Dial is the Phase 2 target hardware** and the intended user-facing remote platform. It is not used for initial BLE/component development.

**The Volcano component and the Dial's user-facing hardware are developed as separate concerns on separate schedules.** Per [ADR-0002](ADR-0002-volcano-component-architecture.md), the Volcano component is already hardware-independent by design; this ADR adds the practical consequence that it can and should be built and validated entirely on the dev board, with no dependency on Dial-specific hardware (display, touch, encoder) existing yet.

**Hardware-specific UI work must not begin until the BLE/component foundation is validated.** Phase 2 does not start until the Volcano component's BLE communication and state model have been verified working against the real Volcano Hybrid device on the dev board. Migrating to the Dial means porting an already-working component to new hardware, not developing BLE and UI simultaneously.

## Consequences

**Benefits**

- Simpler debugging: a bare dev board with no display/touch/encoder stack removes variables when diagnosing BLE-specific problems.
- Faster BLE iteration: reflashing and resetting a plain dev board is quicker and lower-risk than iterating on the final product hardware.
- Avoids coupling unknown protocol work to final hardware: if the BLE protocol turns out to need workarounds or unexpected handling, that gets discovered and resolved before it has to coexist with Dial-specific constraints.
- Enables reusable component development: because the Volcano component is built hardware-independently from the start (per ADR-0002), the validation work done on the dev board carries over directly to the Dial rather than being redone.

**Trade-offs**

- Some work will be repeated when moving to the Dial: board-specific configuration (pin mappings, peripheral setup) still has to be adapted even though the Volcano component itself does not change.
- Display/touch/encoder integration is delayed until Phase 2, meaning there is no user-facing UI to demonstrate or use during Phase 1 — only the validated BLE/component foundation.
- Requires maintaining hardware abstraction boundaries deliberately: it is only safe to defer Dial-specific work because ADR-0002's boundary is actually enforced; if BLE or state logic were to leak into hardware-specific code on the dev board, this strategy's benefits would be undermined.

## Alternatives considered

**1. Start directly on the M5Stack Dial**

Develop BLE communication and the Volcano component directly on the Dial hardware. Rejected because it adds UI and hardware complexity (display, touch, encoder bring-up) before the BLE protocol is even understood, and makes debugging harder by mixing two independent, unfamiliar problem spaces at once.

**2. Build on a Raspberry Pi/Linux system first**

Prototype BLE communication on a Raspberry Pi or other Linux host before targeting ESP32 hardware at all. Rejected because the project's target platform is ESP32/ESPHome, and Linux BLE stack behaviour (connection handling, timing, stack quirks) differs from ESP32 BLE behaviour enough that findings wouldn't reliably transfer — the validation would need to be redone on the actual target anyway.

**3. Use the ESP32-S3 development board permanently**

Skip the Dial migration and ship the dev board itself as the final product. Rejected because it does not provide the intended user interface hardware — no touchscreen, rotary encoder, or the physical form factor the project's Phase 2 goal (per ADR-0001) requires for a standalone local remote.

## Notes

- Reference [ADR-0001](ADR-0001-project-vision.md) for the project phases this hardware strategy implements.
- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for the component boundary that makes deferring Dial-specific work safe.
- Reference [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) for the ESPHome framework choice this strategy is built on. That ADR's Decision left ESPHome's support for the Dial's display/touch/encoder/buzzer hardware as a caveat to verify in Phase 2; [ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md) has since settled it, and the resulting Dial UI is built and hardware-verified.
