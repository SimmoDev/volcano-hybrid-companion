# ADR-0008: Temperature Units Handling

## Status

Accepted

## Context

[ADR-0002](ADR-0002-volcano-component-architecture.md) places a hardware-independent interface between the Volcano abstraction layer and every control interface. Temperature is the value that interface carries most of, and the unit it carries it in has to be decided before the interface exists, because changing it afterwards changes every consumer.

Protocol discovery has settled what the device does, recorded under [`docs/protocol/`](../protocol/README.md):

**The wire is always Celsius.** Current temperature ([STATE-007](../protocol/state-model.md#state-007--current-actual-temperature)) and target temperature ([CMD-001](../protocol/commands.md#cmd-001--set-target-temperature)) are both 4-byte little-endian values in units of 0.1 °C, regardless of what the device is displaying. Fahrenheit never appears on the wire in any form.

**The display unit is a separate, writable setting.** Bit 9 of the display/units register selects it ([STATE-010](../protocol/state-model.md#state-010--temperature-display-units-bit), [CMD-010](../protocol/commands.md#cmd-010--set-display-units)). It can be changed at the device's own panel gesture or written over BLE, and it alters what the device shows and nothing that it reports.

**Conversion is not clean in the direction a UI needs.** The confirmed bounds convert exactly — 40 °C is 104 °F and 230 °C is 446 °F — but 1 °F is 0.56 °C, so a Fahrenheit control stepping by one degree lands between Celsius values. The device accepts and retains sub-degree targets ([CMD-001](../protocol/commands.md#cmd-001--set-target-temperature)), so those writes work, but they do not round-trip: converting back gives a Fahrenheit value that is not the one the user chose. The official app demonstrates the same problem, writing 229.5 °C for a displayed 445 °F when 445 °F is 229.44 °C.

**ESPHome fixes an entity's unit at compile time.** A `number`'s `unit_of_measurement`, `min_value`, `max_value` and `step` are set when the firmware is built, so no entity can follow the device's units bit at runtime. Home Assistant performs its own conversion for entities carrying a temperature device class; an ESPHome `web_server` page does not.

## Decision

**The Volcano abstraction layer speaks Celsius only.** Every temperature crossing the hardware-independent interface — read or written — is Celsius, matching the wire encoding. No Fahrenheit value exists inside the component.

**Unit conversion is a control interface's concern, not the component's.** A control interface that wants to present Fahrenheit converts at its own boundary, and owns the rounding decisions that come with it.

**The device's display unit is exposed as an ordinary device setting, not as a UI preference.** It is offered like vibration or display-on-cooling: something a control interface can read and change on the device. It does not influence anything the component reports, and no part of the component branches on it.

## Consequences

- The interface has one temperature representation, so no consumer has to ask which unit a value is in, and no conversion happens anywhere it is not visible.
- Home Assistant gets Fahrenheit for free where a user's locale asks for it, because the entities carry a temperature device class and Home Assistant converts them.
- A control interface that wants Fahrenheit takes on the round-trip problem itself. It must decide whether to echo the user's chosen value or the device's actual one, because the two differ. That decision is a presentation matter and belongs where the presentation is.
- A user can have the device displaying °F while a control interface displays °C, or the reverse. The two are independent, which is a direct consequence of the display unit changing nothing the component reads.
- The Phase 2 Dial UI, which renders temperature itself rather than delegating to Home Assistant, is where this decision has real work attached. It will need its own conversion and its own rounding policy if it offers Fahrenheit at all.
- Should the abstraction layer ever need to present a unit-aware value directly, this ADR has to be revisited rather than worked around, since the alternative is unit information leaking into an interface defined not to carry it.

## Alternatives considered

**1. Carry a unit alongside every temperature**

Have the interface pass a value and its unit, letting consumers receive whichever they prefer. Rejected because it makes every consumer handle both cases forever, to represent a device that only ever speaks one of them. The complexity would exist purely to serve presentation, in the layer explicitly defined to be independent of presentation.

**2. Follow the device's display unit**

Have the component read the units bit and present temperatures in whatever the device is currently showing. Rejected on two grounds. It makes the interface's contract depend on a mutable device setting, so the same call returns different numbers before and after someone touches the panel — and ESPHome cannot express it anyway, since an entity's unit is fixed at compile time.

**3. Make the unit a compile-time configuration option**

Let a YAML option select the unit the component works in. Rejected because it converts inside the component rather than at the presentation boundary, so the conversion and its rounding become invisible to the consumer that actually cares about them. It would also make the component's own range checks — which enforce the Celsius bounds [CMD-001](../protocol/commands.md#cmd-001--set-target-temperature) confirms — operate on converted values, adding a rounding step in front of a safety check.

## Notes

- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for the interface this decision constrains, and [ADR-0001](ADR-0001-project-vision.md) for the control interfaces that will consume it.
- Reference [ADR-0005](ADR-0005-volcano-ble-discovery-methodology.md) for why only Confirmed findings back default behaviour. Every device behaviour above is Confirmed: the Celsius wire encoding, the units bit and its polarity, that the bit is writable, that the device retains sub-degree target precision, and the confirmed 40–230 °C range.
- The sub-degree behaviour cuts both ways and is worth stating plainly: a Fahrenheit UI *can* write what the user chose, because the device stores it. What it cannot do is display that value back unchanged after a round trip. This is a rounding-policy problem, not a protocol limitation.
- This decision says nothing about *displaying* temperature on the Phase 2 Dial, only about what crosses the component's interface. How that UI formats, rounds or steps a temperature is its own concern.
- The device's own display unit is settable over BLE, which was not known when this question was first raised — it had only ever been changed by a panel gesture. Had it turned out to be read-only, the third alternative above would have been more attractive, since a controller could then only follow the device rather than set it.
