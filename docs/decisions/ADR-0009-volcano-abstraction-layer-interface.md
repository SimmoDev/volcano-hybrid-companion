# ADR-0009: Volcano Abstraction Layer Interface

## Status

Accepted

## Context

[ADR-0002](ADR-0002-volcano-component-architecture.md) defines the architectural boundary between the Volcano abstraction layer, the BLE communication layer, and the control interfaces above them. Its Notes explicitly defer "the concrete API/entity design of the Volcano component" to a later ADR, once the protocol has been observed and verified. That condition is now met: every **Confirmed** finding in [`docs/protocol/`](../protocol/README.md) is implemented and verified against real hardware, and [ADR-0008](ADR-0008-temperature-units-handling.md) has settled the unit question that the interface carries most of.

Three facts about the current implementation shape this decision.

**There is no state model.** Decoded values pass straight from the component's decode methods into `publish_state()` on an ESPHome entity. The entities *are* the only record of device state, which inverts ADR-0002: control interfaces are meant to read *from* the component, but a control interface that is not an ESPHome entity — the Phase 2 Dial UI — currently has nothing to read. ADR-0002 requires the component to hold "the single, authoritative copy" of that state.

**Nothing distinguishes a known value from a stale one.** A published value persists after a BLE dropout with no indication that it is no longer live, and the component exposes no connection state at all, which ADR-0002 lists as one of its five responsibilities. The protocol has a second, non-connection source of the same problem: current temperature below 40 °C is not a low reading but the absence of one ([STATE-012](../protocol/state-model.md#state-012--sub-40-c-reporting-and-the-idle-display-state)).

**The two layers are one class.** `VolcanoComponent` is simultaneously the ESPHome integration, the BLE communication layer and everything that stands in for an abstraction layer. The boundary ADR-0002 defines has no structural existence, and a control interface cannot be built or exercised without BLE hardware present.

Two constraints from outside the project bear on the design:

**ESPHome cannot express "unknown" on a switch.** `sensor` and `number` publish `NAN`, which the `web_server` page renders as `NA` and the native API reports as `missing_state`; `text_sensor` carries the same flag through `has_state()`. `SwitchStateResponse` has no such field — a switch is on or off to every consumer, always. The heater, pump, vibration, display-on-cooling and display-units entities are all switches.

**The device does not confirm every write the same way.** Heater, pump, vibration and the display register are confirmed by notification. Target temperature, LED brightness and auto-shutoff duration are not: [STATE-013](../protocol/state-model.md#state-013--target-temperature-notifications) records that the device almost never echoes this project's own writes, so those are confirmed by an explicit read-back after the write completes. STATE-013 also records a write that was silently dropped — answered by the *previous* target and having no effect until re-sent — with whether that can happen in the general case still classified Unknown.

## Decision

### Three types, one per responsibility

The Volcano component is split into three types:

- **`VolcanoBleClient`** — the BLE communication layer. Owns characteristic UUIDs and handles, GATTC event handling, subscription and read sequencing, wire encoding and decoding, and the connection lifecycle ([ADR-0007](ADR-0007-ble-connection-lifecycle.md)). It reports decoded domain values upward and accepts domain values to write. No BLE type appears above it.
- **`VolcanoDevice`** — the Volcano abstraction layer. Owns the state model, per-field validity, connection state, the pending-write mechanism, and state-change notification. It contains no BLE detail and no presentation concern, and it must be constructible and exercisable without any BLE hardware.
- **`VolcanoComponent`** — the ESPHome integration. Owns `Component` and `ble_client::BLEClientNode` participation and the optional entities, and is a consumer of `VolcanoDevice` like any other control interface.

Dependencies flow strictly downward, per ADR-0002. `VolcanoBleClient` must not name `VolcanoDevice`: it reports upward through an observer interface that it declares itself and that `VolcanoDevice` implements.

The seam between the two lower layers carries **decoded domain values, not raw payloads** — ADR-0002 assigns wire encoding and decoding to the BLE communication layer, so a `float` in °C crosses the seam, never a byte array or a decidegree count.

### Connection state

`VolcanoDevice` exposes connection state as a three-value enum:

```cpp
enum class ConnectionState {
  DISCONNECTED,  // no link
  CONNECTING,    // link up or coming up; discovery, subscription and the
                 // initial read sweep have not all completed
  READY,         // connected and the initial read sweep has completed
};
```

`READY` must mean the state model is populated, not merely that the link is up. The distinction is load-bearing: subscriptions register and then seven non-notify characteristics are read one at a time, so there is a window of seconds during which the link exists and much of the state model is still unknown. A control interface must be able to tell "not there" from "still arriving".

ADR-0002 names three states as "connected, disconnected, reconnecting". Reconnection is a `CONNECTING` state here; the component does not distinguish a first connection from a recovery, and no consumer has a reason to.

### The state model

Every value is held in a wrapper that carries its own validity, so an unknown value cannot be read as a stale one:

```cpp
template<typename T> class DeviceValue {
 public:
  bool is_known() const;
  T value() const;                  // meaningful only when is_known()
  optional<T> requested() const;    // set while a write is outstanding
};
```

A field **must** report `is_known() == false` whenever the component does not have a live value for it, specifically:

- Before the field has ever been read on the current connection.
- Whenever `connection_state()` is not `READY`, for every field the device reports. Device-information strings are exempt: they are properties of the unit, not live state, and remain known across a dropout.
- For current temperature, while the device is reporting no temperature at all ([STATE-012](../protocol/state-model.md#state-012--sub-40-c-reporting-and-the-idle-display-state)). A sub-40 °C reading must never surface as a temperature.

Consumers are notified of change through `add_on_state_callback()`. Callbacks fire on any transition, including a field becoming unknown.

### Commands, and requested versus confirmed

Commands are requests, per ADR-0002. Command methods return `void` and never write to the confirmed value.

Issuing a command sets the affected field's `requested()`. It is resolved by that field's confirmation source — a notification for the heater, pump, vibration and display settings, and the existing post-write read-back for target temperature, LED brightness and auto-shutoff duration:

- The confirmation source reports the requested value: `requested()` clears and the confirmed value updates.
- The confirmation source reports a **different** value: the confirmed value updates to what the device reported and `requested()` clears. This is exactly STATE-013's silent-drop signature, and it must be detectable as a distinct outcome rather than being indistinguishable from success.
- No confirmation arrives before a timeout: `requested()` clears, the confirmed value is left untouched, and the outcome is reported as a timeout.

A control interface may render `requested()` as a provisional value while it is outstanding. It must not treat it as device state.

The component **must not** automatically re-send a write that failed to confirm. A dropped write is reported to the consumer, which decides what to do about it. The evidence for silent drops is a single observed occurrence with the general case Unknown, which is not enough to back a retry policy under [ADR-0005](ADR-0005-volcano-ble-discovery-methodology.md).

### The interface

Every temperature is Celsius, per [ADR-0008](ADR-0008-temperature-units-handling.md), expressed as a `float` in degrees rather than the wire's decidegrees. Decidegrees are a wire encoding and must not appear above the BLE communication layer:

```cpp
void set_target_temperature(float celsius);
void set_heater(bool on);
void set_pump(bool on);
void set_auto_shutoff_duration(uint16_t seconds);
void set_led_brightness(uint8_t percent);
void set_vibration(bool enabled);
void set_display_on_cooling(bool enabled);
void set_display_units_fahrenheit(bool fahrenheit);
```

Range enforcement stays in the BLE communication layer, as the last gate before the wire. The confirmed ranges are protocol facts ([CMD-001](../protocol/commands.md#cmd-001--set-target-temperature), [CMD-002](../protocol/commands.md#cmd-002--set-led-brightness), [CMD-003](../protocol/commands.md#cmd-003--set-auto-shutoff-duration)), and a value outside them must not reach the device however it arrived. `VolcanoDevice` exposes those bounds so a control interface can constrain its own input, but must not be the only thing enforcing them.

### The ESPHome entities

The entities are retained, and are reclassified as a control interface consuming `VolcanoDevice` rather than as the state model. They remain the manual test surface for the `web_server` page.

Where an entity can express an unknown value it must: `sensor` and `number` publish `NAN`, `text_sensor` clears `has_state()`. Where it cannot — every `switch` — the entity holds its last known value, and a new `connected` binary sensor carries the authority on whether that value can be trusted. This limitation is confined to the ESPHome entity surface; `VolcanoDevice` reports those fields as unknown regardless, so a control interface reading the state model directly is unaffected.

A switch must not be forced to a value on disconnect. Publishing `off` for an unknown heater state asserts that the heater is not running when that is not known, which is the single worst field to be confidently wrong about.

## Consequences

- ADR-0002's dependency arrow becomes real. The Phase 2 Dial UI and the ESPHome entities are peers, both reading the same authoritative state, and neither can reach a BLE detail.
- A control interface can be built and exercised against `VolcanoDevice` with a substitute BLE layer, with no ESP32 and no Volcano present. This is the first point in the project at which anything is testable without hardware.
- Consumers must handle unknown values explicitly. This is more work per call site than reading a bare value, and it is the point: the previous surface made a stale value indistinguishable from a live one, silently.
- A dropped target-temperature write becomes visible at the moment the read-back returns the old value, rather than only when a user notices the device did not change.
- The component gains a `connected` binary sensor, which must be exposed on the example's `web_server` page like every other capability.
- Roughly 1,400 lines of hardware-verified C++ are restructured. The change must be behaviour-preserving with respect to the wire — no characteristic, encoding, ordering or range check changes as part of it — and the result must be re-verified against real hardware before the protocol findings' implementation statuses are considered to still hold.
- A future control interface that needs a value the state model does not carry requires extending `VolcanoDevice` and the observer seam, not just the UI. ADR-0002 already accepts this trade-off.

## Alternatives considered

**1. One class with a documented public/private split**

Keep `VolcanoComponent` as it is, marking the BLE work private and the domain interface public. Rejected because the boundary would be enforced only by access control and review discipline, and because the abstraction layer would remain inseparable from `ble_client::BLEClientNode` — leaving the Dial UI untestable without hardware, which is the concrete benefit the split exists to buy. ADR-0002 already warns that a boundary the compiler does not enforce erodes at each point of friction.

**2. A plain `connected` boolean**

Expose connectivity as one flag. Rejected because it collapses the seconds-long window in which the link is up and the state model is still filling, so a UI cannot distinguish a device that is absent from one that is still arriving, and would show a screen of unknown values as though that were the settled state.

**3. Observed state only, with no pending concept**

Have commands be void requests and let the state model reflect only what the device reported, with no record of an outstanding write. Rejected because a dropped write is then indistinguishable from a slow one for as long as the user cares to wait, and STATE-013 establishes that dropped writes happen. The confirmation sources needed to detect them already exist in the implementation; not using them discards information the component is already receiving.

**4. Forcing switches off when the value is unknown**

Publish `off` for the heater and pump on disconnect, so nothing appears to be running. Rejected because it replaces an honestly stale value with a confidently false one. The device may well still be heating, and the auto-shutoff countdown ([STATE-005](../protocol/state-model.md#state-005--auto-shutoff-countdown)) is the only thing stopping it.

**5. Automatic retry of unconfirmed writes**

Re-send a write that fails to confirm, up to some limit. Rejected as premature under ADR-0005: one silent drop has been observed, the general case is Unknown, and a retry policy built on that would be an assumption encoded as behaviour. Retrying a heater command on unreliable evidence also actuates hardware the user is not watching.

## Notes

- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for the boundary this ADR makes concrete, and [ADR-0001](ADR-0001-project-vision.md) for the three control interfaces the interface must serve.
- The class names map onto the terms [`docs/CONVENTIONS.md`](../CONVENTIONS.md) defines: `VolcanoBleClient` is the BLE communication layer and `VolcanoDevice` is the Volcano abstraction layer. CONVENTIONS forbids "transport" as a synonym for the former, which is why the type is not named for one.
- Unit suffixes are dropped from method names where the ADR fixes the unit — `set_target_temperature(float celsius)` rather than `set_target_temperature_decidegrees()`. Decidegrees was a wire encoding leaking into the interface; degrees Celsius and seconds are the domain units, carried in the parameter names.
- The switch limitation is a property of ESPHome's API, not of this design. Should ESPHome gain an unknown state for switches, the entity layer can adopt it without any change to the state model, which already carries the information.
- This ADR says nothing about how the Phase 2 Dial UI renders any of this, including how it presents an unknown value or an outstanding request. Those are presentation decisions and belong with the presentation, per ADR-0002.
- **`CONNECTING` begins at service discovery completion, not at the raw physical link coming up.** The Decision section above describes `CONNECTING` as "link up or coming up; discovery, subscription and the initial read sweep have not all completed," which reads as starting the moment the physical link exists. The implementation instead stays `DISCONNECTED` for the brief window where the link is up but discovery has not yet started or completed, only reporting `CONNECTING` once discovery finishes. This is deliberate, not an oversight: both states already carry the same meaning to a consumer ("do not treat this as live"), and reporting a `CONNECTING` transition off the raw link-up event instead would require verifying the underlying BLE stack's exact event ordering and timing against ESPHome/ESP-IDF source and re-testing against real hardware before landing — not yet done. Revisit if a future consumer needs to distinguish "the link exists but nothing has started yet" from "no link at all."
- **Resolved: a control interface that needs the "link up but not `READY`" distinction reads it from the underlying transport, not from an entity.** The preceding note's revisit condition has since been met, by the Phase 2 Dial UI: its Home and Connections pages show a distinct "connecting" state. `VolcanoDevice` carries the three-value `ConnectionState`, but the "The ESPHome entities" section above only specifies a binary `connected` sensor (true at `READY`), and a YAML control interface cannot link against `VolcanoDevice` to read the enum directly. Rather than promote `ConnectionState` to an entity, the Dial derives "connecting" from the `ble_client` link coming up while `connected` is still false — the link-liveness of the underlying transport being local plumbing a control interface may observe directly, the same boundary [ADR-0007](ADR-0007-ble-connection-lifecycle.md)'s Notes draw for the release control. This is the settled position: `VolcanoDevice`'s connection state stays a C++ interface for a consumer that links against it, and the entity surface stays a single `connected` bool. Revisit only for a consumer that needs the distinction with no view of the underlying `ble_client` link.
- **A mismatch only resolves `requested()` for a field whose confirmation source is exclusively tied to a specific write.** The Decision section's "Commands, and requested versus confirmed" states that a differing confirmation-source value resolves `requested()` as STATE-013's silent-drop signature, without qualification. That holds for target temperature, auto-shutoff duration and LED brightness, each confirmed by a read-back nothing else ever triggers, so a differing read-back is unambiguously that write's own outcome. It does not hold for heater, pump, vibration, display-on-cooling and display-units: these are confirmed only by notification, which carries no request identity and can be triggered by a bit unrelated to the field being resolved (e.g. STATE-008's vibration-alert pulse sharing the status register with the pump bit, or STATE-009's temperature-change pulse sharing the display register with display-on-cooling/units). There is also no protocol evidence anywhere in `docs/protocol/` that any of these five ever exhibits STATE-013's specific silent-drop-with-unchanged-echo behaviour — only target temperature does. A mismatch on these five therefore never resolves `requested()` by itself; only a later match, an explicit write failure, or the timeout does. This is a correction, not a relaxation: applying STATE-013's evidence-backed drop signature to characteristics with no such evidence was itself inconsistent with ADR-0005's discipline.
