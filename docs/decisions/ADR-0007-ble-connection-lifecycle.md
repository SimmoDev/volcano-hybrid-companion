# ADR-0007: BLE Connection Lifecycle

## Status

Accepted

## Context

[ADR-0002](ADR-0002-volcano-component-architecture.md) places the BLE connection inside the Volcano component's BLE communication layer, and [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) commits the project to ESPHome. Neither decides *when* that connection is held. Protocol discovery has since established device behaviour that makes the question consequential rather than an implementation detail, and that constrains the answer.

Three properties of the device shape it, each recorded under [`docs/protocol/`](../protocol/README.md):

**The device accepts one connection at a time, and stops advertising while connected** ([ADV-001](../protocol/gatt-services.md#adv-001--advertising-and-discovery), [CONN-003](../protocol/gatt-services.md#conn-003--single-connection-at-a-time)). A controller holding the link makes the Volcano invisible to every other client, including the official Storz & Bickel app. There is no sharing arrangement to negotiate: whoever holds the connection has exclusive control until they release it.

**A connected client observes activity it did not initiate** ([STATE-008](../protocol/state-model.md#state-008--statusflags-register-partial), [STATE-013](../protocol/state-model.md#state-013--target-temperature-notifications), [STATE-011](../protocol/state-model.md#state-011--auto-shutoff-behaviour), [CONN-002](../protocol/gatt-services.md#conn-002--notification-delivery-model)). Heater and pump changes made at the device's own control panel notify indistinguishably from commanded ones, as do panel target changes and auto-shutoff expiry. Notifications are delivered on change, whatever the cause.

**A disconnected client cannot tell whether the device is in use** ([ADV-001](../protocol/gatt-services.md#adv-001--advertising-and-discovery)). The advertisement carries a fixed payload — company identifier, serial number, and a 14-byte remainder — at an unchanged advertising interval, and neither the payload nor its timing distinguishes a device that is heating from one that is idle. Detecting that a session has begun requires connecting.

Together these mean the choice is not a spectrum. Either the controller holds the connection and has complete, live state, or it does not and knows nothing whatsoever until it reconnects — during which the device is free for other clients. Resynchronising after connecting takes around six seconds for the official app ([CONN-001](../protocol/gatt-services.md#conn-001--connection-security-and-subscription-procedure)) — a component subscribing to fewer characteristics would be quicker, but the sequence is not optional, since anything whose value matters has to be read once. Whatever it costs, it lands on whatever action prompted the connection.

## Decision

**The Volcano component holds a persistent BLE connection.** It connects once configured and the device is advertising, maintains the connection, and re-establishes it if dropped.

**Connect and disconnect are exposed as explicit commands on the component's interface**, expressed in domain terms alongside the rest of the interface per [ADR-0002](ADR-0002-volcano-component-architecture.md). Releasing the connection is a deliberate action a control interface can offer, not a side effect of device state.

**An explicit release suspends reconnection until an explicit connect.** Otherwise the reconnect loop would take the device straight back and the command would achieve nothing. The suspension does not survive a restart: after a reboot the component connects, on the reasoning that coming up with no connection and no way to know why is worse than reclaiming a link the user might no longer need.

**Releasing the connection does not stop the device.** Actuators keep running when a client disconnects and the device applies no cut-off of its own ([CONN-004](../protocol/gatt-services.md#conn-004--actuator-behaviour-across-a-client-disconnect)), so a release issued with the heater running leaves it running, unattended, until the auto-shutoff countdown expires. A control interface offering release must make that consequence visible when an actuator is active.

**Device state is marked unknown while disconnected.** [ADR-0002](ADR-0002-volcano-component-architecture.md) already makes connection state part of what the component exposes; what this decision adds is that the values alongside it must not read as current when the link is down. A control interface must be able to distinguish "the heater is off" from "the heater's state is unknown".

**Contention is handled by retrying quietly.** Where no release is in force and the device cannot be reached, the component keeps trying in the background and reports its connection state rather than treating the condition as an error. The cause cannot be determined — a device connected elsewhere is indistinguishable from one powered off or out of range — but per [CONN-003](../protocol/gatt-services.md#conn-003--single-connection-at-a-time) a control interface should present "in use elsewhere" as a likely explanation rather than implying the device is absent.

**Device state is read on every connection, never assumed.** Notifications are not delivered on subscribing ([CONN-002](../protocol/gatt-services.md#conn-002--notification-delivery-model)), and actuators keep running across a client disconnect ([CONN-004](../protocol/gatt-services.md#conn-004--actuator-behaviour-across-a-client-disconnect)), so the component must read current state on connecting rather than presume the device is idle.

## Consequences

**Benefits**

- Complete state, including panel-originated activity. Someone operating the Volcano directly is visible to the controller and to anything consuming it, which no demand-driven design can offer.
- No latency on control actions. The connection is already established when a command is issued, rather than costing seconds of setup on whichever action happens to come first after an idle period.
- Works with ESPHome rather than against it: its BLE client is built around a held connection with automatic reconnection. That reconnection is scan-gated, though, and the device does not advertise while connected elsewhere, so the client will simply not see it until the other holder releases — which is the behaviour wanted here, but is worth knowing rather than discovering.
- A simpler state model. There is no window in which device state is unknown but assumed idle, and no inference about whether a session is under way to get wrong.

**Trade-offs**

- **The official app cannot connect while the controller holds the connection.** This is the substantial cost. A user reaching for their phone finds the Volcano absent from the scan, indistinguishable from it being switched off. Any control interface built on this component should make releasing the connection discoverable, and should explain why the device disappears when it does.
- Until Phase 2 provides a user-facing control interface, release is reachable only through ESPHome automations or the API. That is acceptable for Phase 1, which is validation work rather than live use of the device as a controller, but it makes handing the device back to the app a deliberate and inconvenient act rather than a quick one.
- Offering a release control creates a way to leave a running heater with nothing attached to it. The device behaves no differently — a released connection is indistinguishable to it from a crashed controller, and the auto-shutoff countdown remains the only backstop either way — but it turns that situation into something a user can reach deliberately rather than only by failure.
- The controller holds the link indefinitely while powered, including when nobody is using either device. Nothing observed suggests the device minds: connections have been held unbroken for hours with every disconnection in evidence originating from the client rather than the device ([STATE-012](../protocol/state-model.md#state-012--sub-40-c-reporting-and-the-idle-display-state), [STATE-011](../protocol/state-model.md#state-011--auto-shutoff-behaviour)). But the app stays unavailable until someone acts.

## Alternatives considered

**1. Connect on demand**

Connect when a command is issued or state is wanted, and release immediately afterwards. Rejected because connecting and resynchronising is measured in seconds rather than milliseconds, and that cost falls on every action — including switching the heater on, which is the most common one. It also gives up panel visibility entirely, and requires the component to represent "state unknown" as its normal resting condition.

**2. Hybrid — hold the connection only while the device is in use**

Connect on user interaction or when an actuator is running, and release once the device returns to idle. Attractive because it frees the device for the app between sessions, and the device signals idle observably. Rejected because it sacrifices exactly what a persistent connection is for: with no connection held, a session started at the device's own control panel is invisible until the controller happens to reconnect, and nothing observed in the advertisement or its timing allows that to be detected passively. It also reintroduces connection latency on the first action of each session, and requires the component to infer when a session has started and ended — an inference a persistent connection never has to make.

**3. Periodic polling while otherwise disconnected**

Leave the connection released, but connect briefly at intervals to read state. Rejected as the worst of both: it still misses panel activity between polls, and each poll makes the device unavailable to other clients for as long as a connect-and-resynchronise takes — so it interferes with app use without buying reliable state.

**4. Timed or handover release**

Hold the connection persistently, but release it for a bounded window on request and re-acquire automatically when the window expires. This targets the substantial cost directly, and unlike Alternative 2 it needs no inference about device state — the trigger is an explicit user request, not a lifecycle policy. Not adopted because a timer that silently reclaims the device could surprise someone still using the app, and because routine automatic re-acquisition runs against this ADR's rule that a release holds until an explicit connect. The reboot exception above is not the same thing: a restart is an event the user can see and did not expect the component to survive, whereas a timer reclaims the device while nothing appears to have changed. Adopting it later would therefore mean superseding this decision rather than refining it — which is the right bar for a change that can take the device back from a user who did not ask for it.

## Notes

- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for the component boundary this decision sits inside, and for the state model that already carries connection state.
- Reference [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) for the framework whose BLE client this builds on.
- Reference [ADR-0005](ADR-0005-volcano-ble-discovery-methodology.md) for why only Confirmed findings back default behaviour. The behaviours this decision rests on — exclusivity, advertising suppression, panel-originated notification, on-change delivery with no notification on subscribing, actuator persistence across a disconnect, the invariance of the advertisement with heater activity, and the official app's time to reach a synchronised state (a property of that client's sequence, not a device floor) — are each Confirmed. Several of the findings carrying them also record Unknown aspects that this decision touches: whether the advertisement varies with states other than heater activity, whether a target write can be silently dropped, and whether a client absence longer than has been waited out eventually triggers a device-side cut-off. None of the three changes the decision, but each bounds a claim made above, and the notes below say where.
- ESPHome's BLE client initiates a connection only after its tracker sees an advertisement from the configured address, and takes the peer address type from that advertisement — which matters here, because the device advertises with a static random address ([ADV-001](../protocol/gatt-services.md#adv-001--advertising-and-discovery)). Reconnection therefore uses the same scan-then-connect path every observation has covered, and this decision does not depend on the directed-reconnection question left open in [`open-questions.md`](../protocol/open-questions.md).
- Confirming a client's *own* writes is a separate problem this ADR does not solve. Target writes are not echoed back, and a write has been seen answered with the previous target and then having no effect until re-sent ([STATE-013](../protocol/state-model.md#state-013--target-temperature-notifications), where whether writes can be silently dropped is Unknown), while [ADR-0002](ADR-0002-volcano-component-architecture.md) requires the component to confirm state changes from the device before updating the state model. A held connection is a precondition for read-back confirmation but does not supply it; how the component confirms its own writes belongs with the implementation of that requirement.
- The exclusivity this decision accepts is a property of the device, not of the design: see [CONN-003](../protocol/gatt-services.md#conn-003--single-connection-at-a-time). Should a future firmware permit concurrent connections, the trade-off recorded here changes and this ADR should be revisited.
