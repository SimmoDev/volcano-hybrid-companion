# Volcano Hybrid protocol documentation

This directory records what is actually known about the Volcano Hybrid's Bluetooth Low Energy (BLE) protocol, per the structure defined in [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md).

**Status: what the official app can reach is documented.** The full set of service and characteristic UUIDs is enumerated, every handle, UUID and property is known from a full GATT service-discovery capture, and every characteristic has been read at least once except SVC-004's and SVC-003's write-only characteristic (`00000002`) — neither has a Read property.

Every command the official app issues is recorded and Confirmed: setting target temperature, LED brightness, auto-shutoff duration, vibration and display-on-cooling; switching the heater and pump on and off; and reading current and target temperature, heater and pump state, the auto-shutoff countdown and the operating-time counters. See [`commands.md`](commands.md) and [`state-model.md`](state-model.md).

So is the connection procedure, in [`gatt-services.md`](gatt-services.md): advertising contents, the ATT MTU, the absence of pairing, the CCCD writes needed to subscribe, that the device advertises only while unconnected and therefore accepts one connection at a time, and that it leaves its actuators running when a client disconnects.

The device also exposes two further triggers of the same shape as the heater and pump ones, whose purpose is unidentified, and several writable characteristics the app never writes — some of those have a likely purpose recorded at Probable, others none at all. Many characteristics have a confirmed raw value and no identified meaning. All of that is tracked in [`open-questions.md`](open-questions.md), along with what would resolve each one. Most of what remains needs a client written for this project, because it turns on writes the app never issues or values it clamps rather than sending.

## What these findings apply to

Every finding recorded here was observed against a single Volcano Hybrid running firmware version `V01.03.00.00` with BLE firmware `V01.00.00.00` (see [STATE-003](state-model.md#state-003--firmware-version) and [STATE-004](state-model.md#state-004--firmware-ble-version)). One device, one firmware revision — nothing here has been checked against a second unit or a second firmware version.

This scoping matters most for ATT handles: handles are assigned by the device's own GATT database layout and are only guaranteed stable within a firmware version, so every handle recorded in these documents should be treated as specific to the firmware above. Service and characteristic UUIDs are far more likely to be stable across firmware revisions, since they identify what a characteristic *is* rather than where it sits. An implementation should therefore prefer locating characteristics by UUID, treating the recorded handles as a cross-check rather than as fixed addresses.

One caution regardless of how characteristics are located: CHAR-018 through CHAR-021 (the heater and pump triggers) and the two unidentified triggers beside them all present identically — one byte, Read/Write, reading `0x00` — so nothing about a read distinguishes them. Writing to the wrong one actuates real hardware. An implementation must resolve these by UUID rather than by handle, and check the resulting handle against the entry for that UUID in [`characteristics.md`](characteristics.md) before issuing any write. The same care applies beyond that cluster: `10110004` at `0x004f` is writable, unidentified, and sits immediately after target temperature, so a client enumerating writable characteristics can reach it by accident.

## "The app", as used in these documents

Findings here are frequently correlated against the official Storz & Bickel client — referred to throughout as "the app". That client is the web app at `app.storz-bickel.com`, which drives the device through the browser's Web Bluetooth API. It is the only official client: the former native Android app is withdrawn, and no observation recorded in this directory involves it.

The client is delivered as unobfuscated JavaScript, so where a finding cites what the client calls something, or how it interprets a value, that comes from reading the code it serves publicly. Such a claim describes what the official client believes, which is not the same as observed device behaviour: it is recorded as a lead and cannot reach Confirmed on its own, exactly as third-party sources are treated under [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md). No code from the client is reproduced in this project.

Every GATT operation recorded in these documents — service discovery, reads, writes, and subscriptions — was performed by an ordinary client through a standard browser API, so none of them requires proprietary tooling to reproduce. The advertising contents in [ADV-001](gatt-services.md#adv-001--advertising-and-discovery) and the MTU exchange and security details in [CONN-001](gatt-services.md#conn-001--connection-security-and-subscription-procedure) are a different matter: raw advertising structures, the peer address type, and link-layer negotiation are not exposed by a browser API and were recorded from host-side Bluetooth capture. Reproducing those needs a capture setup; reproducing everything else does not. The link was not encrypted either — [CONN-001](gatt-services.md#conn-001--connection-security-and-subscription-procedure) records neither pairing nor encryption for any operation exercised. What it leaves open is whether a characteristic nobody has yet touched would demand one — several writable characteristics have never been written at all (see [`open-questions.md`](open-questions.md)), and a device reveals a security requirement only when an operation is actually attempted.

## BLE terms used in this directory

- **GATT** — Generic Attribute Profile, the standard structure (services containing characteristics) BLE devices use to expose data.
- **Service** — a named grouping of related characteristics, identified by a UUID.
- **Characteristic** — an individual readable/writable/notifiable value within a service, identified by a UUID and an ATT handle.
- **Handle** (also **ATT handle**, referring to the Attribute Protocol that GATT is built on) — the numeric address of an attribute (a service, characteristic, or descriptor) within the device's GATT database. Stable for a given firmware version; used throughout these documents to identify characteristics.
- **UUID** — the 16-bit (standard) or 128-bit (vendor-specific) identifier of a service or characteristic, distinct from its handle.
- **Controller** — used throughout these documents for whatever software drives the device over BLE: this project's firmware, the official app, or a test client. It is deliberately broader than "control interface" as [`CONVENTIONS.md`](../CONVENTIONS.md) defines that term, which names an architectural component of the Volcano component rather than anything on the wire.
- **Read / Write / Notify** — a characteristic's supported operations: reading its current value, writing a new value, or subscribing to be pushed a new value whenever it changes.
- **Little-endian** — the byte order used throughout this device's multi-byte values: the least-significant byte comes first on the wire.

## What lives here

- [`gatt-services.md`](gatt-services.md) — findings about the GATT services the Volcano Hybrid exposes.
- [`characteristics.md`](characteristics.md) — findings about individual characteristics within those services.
- [`commands.md`](commands.md) — findings about writes/commands and their observed effects.
- [`state-model.md`](state-model.md) — findings about how device state (temperature, heater, pump, etc.) is observed and reported.
- [`open-questions.md`](open-questions.md) — unresolved questions, tracked until enough evidence exists to turn them into a finding.

## Third-party sources

Findings and open questions throughout this directory that cite "a third-party source" or "third-party sources" draw on one or more of the following independent reverse-engineering projects:

- [Chuffnugget/volcano_integration](https://github.com/Chuffnugget/volcano_integration)
- [magikh0e/volcano-hybrid-control](https://github.com/magikh0e/volcano-hybrid-control)
- [SavageNL/home-assistant-volcano-hybrid](https://github.com/SavageNL/home-assistant-volcano-hybrid)
- [ImACoderImACoderImACoder/onyx](https://github.com/ImACoderImACoderImACoder/onyx)

Per [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md), these are treated strictly as leads, never as proof — every claim sourced from them is independently correlated against direct observation of real hardware before being recorded as Confirmed. Attribution is not tracked per finding: a citation of "a third-party source" may refer to any one, or more than one, of the four above.

## How this documentation works

This is a summary only — see [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md) and [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md) for the full methodology and structure this directory follows.

- Discovery is evidence-driven: nothing is recorded as protocol behaviour until it has been observed.
- Raw captures are not retained in this repository. Findings record what was observed and at what confidence, but the underlying sniffer logs and GATT dumps are not kept, so a finding cannot be re-derived from stored evidence — it can only be re-observed against hardware.
- Every finding is classified **Confirmed**, **Probable**, or **Unknown**, and only Confirmed findings back default production behaviour in the Volcano component.
- Every finding carries an **Implementation status** saying whether and where the Volcano component implements it. Three forms are used: *Implemented* (with what was implemented and how far it was verified — often narrower than the finding, where the component reads a value without encoding the behaviour around it); *Not implemented*; and *Not applicable*, for findings there is nothing to implement against. The last covers device behaviour a client can only live with (e.g. that the device accepts one connection at a time), and anything handled by ESPHome rather than by this component (e.g. scanning). It is distinct from *Not implemented*, which means the work is outstanding.
- See [`docs/CONVENTIONS.md`](../CONVENTIONS.md) for the finding ID format (`<area>-NNN`) used across these files.
