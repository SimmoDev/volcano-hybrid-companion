# ADR-0001: Project Vision and Scope

## Status

Accepted

## Context

Before any BLE protocol investigation, ESPHome component, or UI work begins, the project needs a stable, written reference point for what it is trying to build and what it is deliberately not trying to build. Without this, individual technical decisions (how the BLE layer is structured, whether Home Assistant is required, when the M5Stack Dial enters the picture) risk being made ad hoc, and scope can drift — for example, toward a Home-Assistant-only integration, or toward coupling the UI to the BLE layer for short-term convenience.

This ADR formalizes the vision already stated in the README as an explicit decision, so later architectural ADRs (e.g. how the Volcano component is structured, how control interfaces are separated) can be evaluated against it rather than re-litigating the project's purpose each time.

## Decision

The project will build a standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, developed in three phases:

1. **Phase 1 — BLE foundation.** Develop on an ESP32-S3-WROOM-1-N16R8 board using ESPHome. Document and validate the Volcano Hybrid BLE protocol through observation and testing, then expose it through a hardware-independent Volcano communication/abstraction layer as a reusable ESPHome external component.
2. **Phase 2 — Local standalone remote.** Port the firmware to the M5Stack Dial, adding rotary encoder input, a touchscreen, and a local UI. The Dial must fully control the Volcano with no external dependencies.
3. **Phase 3 — Home Assistant integration.** Add Home Assistant integration through the ESPHome API as an additional, optional control surface. The device must keep controlling the Volcano if Home Assistant is unreachable.

The end state supports three independent control paths — the M5Stack Dial local UI, Home Assistant, and direct ESPHome API/automation control — all interacting with the same underlying Volcano abstraction layer, so behaviour is consistent regardless of which path issues a command.

Phases are sequential gates, not parallel workstreams: Phase 2 UI work does not begin until Phase 1's BLE layer is verified against real hardware, and Phase 3's Home Assistant integration does not begin until Phase 2's standalone remote works independently.

## Consequences

**Benefits**

- Gives every later technical decision a fixed point to be judged against: does this keep the device standalone-capable, does this keep Home Assistant optional, does this keep the Volcano layer hardware-independent.
- Sequencing phases as gates means each layer (BLE, then UI, then Home Assistant) is verified working on its own before the next is built on top of it, reducing the risk of discovering a foundational problem late.
- Makes scope boundaries explicit and citable, so contributions or design proposals that would violate them (e.g. a Home Assistant dependency) can be identified quickly and by reference to a decision record rather than re-argued from scratch.

**Trade-offs**

- Slower path to a Home Assistant-integrated device, since Phase 3 is deliberately last rather than done first, even though Home Assistant integration is likely the most immediately useful feature for many potential users.
- Requires access to real hardware during each phase: ESP32-S3 hardware during Phase 1 and M5Stack Dial hardware when Phase 2 begins; the plan does not proceed on simulation or assumption alone.
- Commits the project to supporting a fully offline/standalone mode indefinitely, which constrains future design choices (e.g. no architecture that quietly assumes network/cloud availability).

## Alternatives considered

**Home Assistant-first integration**

Build a Home Assistant component/integration first, treating the ESP32 primarily as a BLE-to-Home-Assistant bridge. Rejected because it would make Home Assistant load-bearing from day one, contradicting the requirement that it remain optional, and would make it harder to retrofit standalone operation later.

**Phone app instead of dedicated hardware**

Build a mobile app that talks to the Volcano over BLE directly, skipping ESP32/ESPHome entirely. Rejected because it does not produce a standalone physical remote (the original goal), ties control to a phone being present and app being open, and does not lead toward the M5Stack Dial hardware target.

**Target the M5Stack Dial from the start, skip the ESP32-S3 dev-board phase**

Develop directly on the Dial hardware rather than a plain ESP32-S3 dev board first. Rejected because it couples early, unverified BLE protocol work to the Dial's more constrained and less flexible development/debugging environment, making Phase 1's verification loop slower and harder.

## Notes

- This ADR is deliberately about scope and sequencing, not architecture. How the Volcano component is internally structured, and how UI/Home Assistant/automation consumers are kept separate from BLE details, is expected to be its own ADR (e.g. ADR-0002).
- Future ADRs should reference this one when a proposed change would affect phase ordering, the standalone-operation requirement, or the optional status of Home Assistant.
