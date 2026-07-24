# ADR-0002: Volcano Component Architecture

## Status

Accepted

## Context

[ADR-0001](ADR-0001-project-vision.md) establishes that the project supports three independent control paths — the M5Stack Dial local UI, Home Assistant, and direct ESPHome API/automation control — all interacting with the same underlying Volcano abstraction layer. That decision states the goal; it does not yet define where the boundary of that abstraction layer sits, what it owns, or what each control surface is and is not allowed to do.

Without an explicit boundary, there is nothing stopping BLE details from leaking into whichever layer is convenient at the time — for example, a UI screen reading a BLE characteristic directly because the abstraction layer doesn't yet expose the value it needs, or Home Assistant-specific formatting logic creeping into the same code that talks to the device over BLE. Once that happens once, it tends to happen again at the next point of friction, and the three control paths drift toward three subtly different, duplicated implementations of "talk to the Volcano" — exactly what ADR-0001 already ruled out at the vision level.

This ADR defines that boundary concretely: what the Volcano component owns, what it exposes, what shape the dependency between layers takes, and where device state actually lives.

## Decision

**Dependency direction**

Dependencies flow strictly one way, from control interfaces down to the device:

```
Control interfaces
        |
        v
Volcano abstraction layer
        |
        v
BLE communication layer
        |
        v
Volcano Hybrid device
```

The Volcano abstraction layer and the BLE communication layer beneath it have no knowledge of, and no dependency on, anything above them. Control interfaces depend on the Volcano abstraction layer; the Volcano abstraction layer depends on the BLE communication layer; nothing depends upward.

**Ownership boundaries**

The Volcano component owns the device communication domain, comprising the Volcano abstraction layer and the BLE communication layer it contains. Control interfaces own everything involved in presenting and acting on that information for a particular surface (touchscreen, Home Assistant entity, automation call). Nothing about how the device is presented belongs inside the Volcano component; nothing about how the device is actually communicated with belongs inside a control interface.

**Responsibilities of the Volcano component**

- Communicating with the Volcano Hybrid: establishing, maintaining, and recovering the BLE connection.
- Owning all BLE implementation details: services, characteristics, read/write/notify handling, and encoding/decoding of the wire protocol. These details do not appear anywhere outside this component.
- Managing connection state (connected, disconnected, reconnecting) as part of what it exposes to consumers, so control interfaces can react to connectivity without implementing any BLE logic themselves.
- Maintaining the Volcano device state model — current temperature, target temperature, heater state, valve state, and any other observable device state — as the single, authoritative copy of that state.
- Providing a hardware-independent interface for control consumers, expressed purely in Volcano domain terms (e.g. "set target temperature," "current state changed") with no BLE types, service UUIDs, or characteristic handles in its public surface.

**Responsibilities of UI / Home Assistant / API layers**

- Consume the Volcano component's interface and state model; do not query or cache device state independently.
- Translate that state into whatever a given surface needs (a rendered screen, a Home Assistant entity, an automation-callable service) without reimplementing or reinterpreting protocol behaviour.
- Contain all surface-specific concerns — layout, navigation, entity naming, automation triggers — entirely within themselves. None of this logic is permitted inside the Volcano component.

**What control consumers must not do**

- Directly access BLE services or characteristics. All device communication happens through the Volcano component's interface only.
- Contain Volcano-specific protocol logic (parsing notification payloads, encoding commands, interpreting raw BLE values). Any such logic belongs in the Volcano component.
- Depend on a specific hardware platform. A control interface written against the Volcano component's interface must work whether the component is running on an ESP32-S3 dev board or an M5Stack Dial.

**State ownership**

The Volcano component's device state model is the single source of truth. Control interfaces read from it and request changes through it; they do not maintain their own independent notion of device state that could drift from what the device actually reports.

Commands issued by control interfaces are requests, not authoritative state changes. The Volcano component is responsible for confirming device state changes from the device itself before updating the shared state model.

## Consequences

**Benefits**

- BLE protocol logic exists in exactly one place, so protocol fixes, quirks, and refinements apply to every control surface simultaneously.
- Control interfaces can be built, tested, and iterated on independently of BLE hardware, as long as they only depend on the Volcano component's interface.
- The strict one-way dependency direction makes violations structurally visible: any code that needs to reach "upward" (e.g. Volcano component code referencing UI or Home Assistant concepts) is immediately a signal something is wrong.
- A new control surface can be added by writing a consumer of the existing interface, without touching or duplicating BLE logic.

**Trade-offs**

- Every capability a control surface needs must first be exposed by the Volcano component's interface; a UI feature that needs a device value the component doesn't yet expose requires extending the component first, not just the UI.
- The interface must be designed generically enough to serve all three control paths, which takes more upfront design care than optimizing it for whichever surface is built first.
- Enforcing the boundary is a code-review and structure concern, not something a compiler guarantees on its own; discipline is required to keep BLE details from leaking upward over time.

## Alternatives considered

**UI directly controlling BLE**

The M5Stack Dial UI (or any other control surface) opens its own BLE connection and speaks the Volcano protocol directly, bypassing a shared component. Rejected because it duplicates protocol logic outside the Volcano component, ties UI code to low-level BLE details, and gives each surface its own view of device state with no guarantee of consistency — the exact drift ADR-0001 and this ADR both exist to prevent.

**Separate BLE implementations for each interface**

Each control path (Dial, Home Assistant, direct API) implements its own independent BLE communication code, tuned to that surface's needs. Rejected because it multiplies the amount of unverified BLE logic that needs to be tested against real hardware, and any protocol correction has to be repeated in every implementation rather than made once.

**Home Assistant as the owner of device state**

Device state lives in Home Assistant, with the ESP32 relaying raw values to it and other surfaces (the Dial, direct API) reading state back through Home Assistant. Rejected because it makes Home Assistant load-bearing for basic device state, directly contradicting ADR-0001's requirement that the device keep working when Home Assistant is unreachable, and because Home Assistant is one control surface among three, not a privileged one.

## Notes

- This ADR defines the architectural boundary; it does not specify the actual BLE protocol (services, characteristics, byte layouts) or the concrete API/entity design of the Volcano component. Those are expected to be documented in their own ADRs once the protocol has been observed and verified, per [ADR-0001](ADR-0001-project-vision.md)'s phased approach.
- Future ADRs should reference this one when a proposed change would move protocol logic into a control interface, add a second source of device state, or introduce a dependency from the Volcano component back up toward a specific UI or platform.
