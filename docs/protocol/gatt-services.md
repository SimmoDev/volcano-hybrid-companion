# GATT services

Findings about how the Volcano Hybrid presents itself over BLE: how it is discovered, and what GATT services it exposes. Format per [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md); confidence per [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md). See [`characteristics.md`](characteristics.md) for the individual characteristics within these services; the service structure recorded here comes from the same repeated GATT service discovery described at the head of that file.

## ADV-001 — Advertising and discovery

- **Observation**: The device is discoverable by scanning, using a static random BLE address that is unique per device and not recorded here. The address is unchanged across a mains power cycle, across the device's own Bluetooth being switched off and on, and across the power lead being unplugged and reconnected, so it is usable as a stable identifier for a given unit. The advertising payload carries:
  - Flags `0x06` (LE General Discoverable, BR/EDR not supported).
  - Manufacturer-specific data with company ID 1736 (Storz & Bickel GmbH & Co. KG, per the Bluetooth SIG assigned-numbers list). The 10 bytes immediately following the 2-byte company ID are the device's serial number as ASCII — the complete value, matching the full 10-byte length of the serial characteristic ([STATE-002](state-model.md#state-002--serial-number)), and likewise not recorded here. The remainder of the manufacturer data is 14 bytes and is not decoded. It does not track heater activity: across advertisements captured in both conditions it is byte-identical whether the device is idle or actively heating, with the advertising interval unchanged at about 40 ms. Other states — pump running alone, an armed auto-shutoff, an error condition — have not been compared.

  The scan response carries:
  - Complete Local Name `S&B VOLCANO H`.
  - TX Power Level, and a Peripheral Connection Interval Range of 20–70 ms (matching the preferred connection parameters in [CHAR-003](characteristics.md#char-003--peripheral-preferred-connection-parameters)).

  **No service UUIDs are advertised** — no 16-bit or 128-bit Service UUID AD types are present in either the advertisement or the scan response.

  **The device does not advertise while a connection is established.** Observed with a second host scanning continuously throughout: it reported no advertisement from the device for as long as the first host held a connection, and reported it within seconds of that connection being dropped. Reproduced by alternating the connection between the two hosts.
- **Interpretation**: Heater activity cannot be detected by scanning, so a client that needs to know what the device is doing has to connect. A scanner finding no advertisement cannot conclude the device is absent or powered off — it may simply be connected to something else, which is indistinguishable from the scanner's side. Filtering by service UUID is not possible; a scanner must match on the local name or the manufacturer-specific data. Note these need different scan modes: the local name is carried only in the scan response, so name filtering requires an **active** scan, whereas the manufacturer data — and therefore the serial number — is in the advertisement itself and visible to a passive scan. A consequence of the latter is that a specific unit can be identified by a passive scanner without ever connecting to it. The `S&B VOLCANO H` string is separately readable after connecting as the GAP Device Name ([CHAR-001](characteristics.md#char-001--device-name)), but only the advertised copy is usable for scan-time filtering.
- **Confidence**: Confirmed (the address and its stability across power cycles, advertising and scan-response contents, the absence of service UUIDs, that advertising stops while connected, and that neither the manufacturer-data remainder nor the advertising interval changes with heater activity); Unknown (what the 14-byte remainder encodes, and whether it varies with any state other than heater activity)
- **Implementation status**: Not implemented

## CONN-001 — Connection, security, and subscription procedure

- **Observation**: Establishing a working connection requires the following, as exercised by the official Storz & Bickel web app driving the device through the browser's Web Bluetooth API. Observed identically across separate connection setups:
  - **ATT MTU**: the client requested 517; the device responded **23**, the BLE default. That caps an ATT read response at 22 bytes of value (`ATT_MTU − 1`), and caps notification, Write Request and Write Without Response payloads at 20 bytes (`ATT_MTU − 3`). Write Request is the type every writable characteristic on this device supports, and no documented write exceeds 4 bytes.
  - **Security**: no SMP traffic and no encryption-change events occur during connection setup. Every operation exercised — service discovery, reads, CCCD writes, and writes to target temperature, LED brightness, auto-shutoff duration, both settings registers, and all four heater and pump triggers — succeeded over an unencrypted link with no pairing or bonding.
  - **Subscription**: notifications require an explicit CCCD write. The client wrote `0100` to eight descriptor handles immediately after connecting — `0x000c`, `0x002c`, `0x002f`, `0x0048`, `0x004d`, `0x0055`, `0x0069`, and `0x006c`.
  - **Time to synchronised**: the official app's sequence from connection complete to the last initial read takes about 6 seconds, consistently, comprising eight CCCD writes and fifteen reads.
  - **Write type**: the device's property bitmasks show `00000002` in SVC-003 is the only characteristic that advertises Write Without Response; every other writable characteristic supports Write (with response) only. (Which write type a client *uses* is the client's choice and says nothing about the device; the property bitmasks are what establish support.)
- **Interpretation**: The six-second synchronisation time is a property of that client's sequence rather than a floor imposed by the device: a client subscribing to fewer characteristics, or reading fewer initial values, would reach a usable state sooner. The shape is not optional though, since [CONN-002](#conn-002--notification-delivery-model) requires an initial read of anything whose value matters.

  The device requires no proprietary handshake or security setup for the operations exercised: a standard GATT client reached and drove it through a browser API. The eight subscribed descriptors correspond to the status/flags register, the display/units register, current and target temperature, the auto-shutoff countdown, hours and minutes of operation, and SVC-003's read+notify characteristic — this client subscribed to 8 of the device's 16 notify-capable characteristics, not all of them.
- **Confidence**: Confirmed (MTU, CCCD requirement, the time to reach a synchronised state, write-type support per the property bitmasks, and that every operation exercised succeeded with neither pairing nor encryption); Unknown (whether any characteristic not yet exercised requires an encrypted link, which a device only reveals on access)
- **Implementation status**: Not implemented

## CONN-002 — Notification delivery model

- **Observation**: Across the subscribed characteristics, every notification carried a value different from the previous one — no notification repeated the preceding value. No notification arrives on subscribing: after each CCCD write the first notification came only once the value next changed, between 8 seconds and over 4 minutes later depending on the characteristic. Observed intervals reflect how fast each value moves rather than a fixed timer:
  - Current temperature (`0x0047`) and the auto-shutoff countdown (`0x0054`) — median about 1 second while the value is moving.
  - Minutes of operation (`0x006b`) — exactly 60 seconds while the heater is on, and nothing at all while it is off ([STATE-006](state-model.md#state-006--minutes-of-operation)).
  - The display/units register (`0x002e`) — a pair of notifications per 1 °C change of current temperature, so about every 3 seconds just after the heater goes off, lengthening to minutes as the device nears ambient, and nothing at all during a fast climb or while holding at target ([STATE-009](state-model.md#state-009--temperature-step-pulse-on-displayunits-register)). Sizing a budget from the temperature notification rate alone understates this handle by a factor of two.
  - The status/flags register (`0x002b`) — irregular, with gaps exceeding a minute when nothing changes.
  - Target temperature (`0x004c`) — when the target is changed at the device itself, and rarely otherwise. Client writes are not echoed, with an intermittent exception whose cause is unresolved ([STATE-013](state-model.md#state-013--target-temperature-notifications)).

  The extreme case is a device sitting below 40 °C with the heater off: a connection has been held for over four hours without a single notification on any subscribed characteristic, while the device remained fully responsive to commands throughout ([STATE-012](state-model.md#state-012--sub-40-c-reporting-and-the-idle-display-state)).
- **Interpretation**: Notifications are sent on change, not on a schedule. Two consequences for an implementation: a client must read a characteristic after subscribing to establish its initial value, since none is pushed; and the absence of notifications means only that nothing changed, so it cannot be used as a liveness or connection-health signal — an idle device stays silent indefinitely while remaining connected.
- **Confidence**: Confirmed (on-change delivery, absence of an initial notification, and the 60-second minutes-of-operation tick, which is a device rule rather than a measured rate — see [STATE-006](state-model.md#state-006--minutes-of-operation)); Probable (that the remaining intervals generalise, since those reflect how fast a value happened to be moving)
- **Implementation status**: Implemented, for the status/flags register only, in `components/volcano/volcano.cpp` — the initial value is read explicitly after subscribing rather than assumed. Verified against real hardware.

## CONN-003 — Single connection at a time

- **Observation**: The device accepts one connection at a time. With the device idle and visible to two scanning hosts, both attempted to connect: one established a link and the other's attempt failed. Reproduced by alternating which host connected first.

  This follows from the device suppressing advertising as soon as it is connected ([ADV-001](#adv-001--advertising-and-discovery)): a peripheral that is not advertising is not connectable, so the second host has nothing to connect to. Nothing observed requires the device to reject a second connection actively, and the two behaviours have not been separated.
- **Interpretation**: A controller holding a persistent connection makes the Volcano invisible to every other client, including the official app — a user cannot fall back to their phone without the controller releasing the link first. Any controller intended to coexist with app use therefore needs a deliberate way to give up the connection, not merely a policy of holding it. In the other direction, a controller that finds no advertisement on startup should treat "connected elsewhere" as a likely cause rather than assuming the device is off.
- **Confidence**: Confirmed (that only one host can be connected at a time, and that a second host's attempt fails while the first holds the link); Unknown (whether a directed connection to the known address behaves differently from a scan-then-connect — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

## CONN-004 — Actuator behaviour across a client disconnect

- **Observation**: The device keeps its actuators running when the controlling client disconnects, and applies no cut-off of its own. With the heater and pump both running, the client closed its connection (`0x16`, terminated by local host); on reconnecting it read the status/flags register and got `0x3623` — heater and pump still active — matching what the device's own panel showed throughout the gap.

  Reproduced across disconnections of a few seconds and of several minutes, with the heater alone as well as with both actuators. The status register reads back unchanged on reconnection and the device is still holding its target.

  The auto-shutoff countdown runs on real time regardless of whether a client is attached. Read back after each gap it stood within a second of the value continuous decrement from the original heater-on predicts, with nothing lost or reset.

  Both actuators respond to commands from the reconnected client.
- **Interpretation**: The connection is a control channel, not a dead-man's handle. A client that crashes, loses power, or is force-quit leaves the heater and pump running, and the only thing that will stop them is the auto-shutoff countdown ([STATE-005](state-model.md#state-005--auto-shutoff-countdown)). How long that leaves the device unattended depends on the configured duration, and the shortest the official app will set is 30 minutes — whether the device itself accepts less is untested ([CMD-003](commands.md#cmd-003--set-auto-shutoff-duration)), which makes it the most valuable open question for anyone relying on this backstop. Any controller must be designed on the basis that its own failure does not make the device safe.

  For a reconnecting client, the corollary is useful: device state survives the gap intact, so a controller that drops and reconnects can read current state rather than assuming the device reset to idle.
- **Confidence**: Confirmed (that heater and pump both continue running across a client disconnect, and that the countdown continues uninterrupted on real time); Unknown (whether an absence longer than has been waited out eventually triggers a cut-off)
- **Implementation status**: Not implemented

## SVC-001 — Generic Access (standard)

- **Handles**: `0x0001`–`0x0007`
- **UUID**: `1800` (Bluetooth SIG standard)
- **Observation**: 3 characteristics — see CHAR-001 through CHAR-003 in [`characteristics.md`](characteristics.md).
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## SVC-002 — Generic Attribute (standard)

- **Handles**: `0x0008`
- **UUID**: `1801` (Bluetooth SIG standard)
- **Observation**: Service declaration only; no characteristics are exposed.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## SVC-003 — Vendor service A

- **Handles**: `0x0009`–`0x000e`
- **UUID**: `00000001-1989-0108-1234-123456789abc`
- **Observation**: 2 characteristics, in handle order: `00000003` at `0x000b` (Read/Notify, CCCD at `0x000c`) and `00000002` at `0x000e` (Write and Write Without Response, no Read). Note the UUIDs run in the reverse of handle order. `00000003` returns a single byte whose meaning is unexplored, and `00000002` has never been written to — see [`open-questions.md`](open-questions.md). `00000002` is the only characteristic on the device that supports Write Without Response.
- **Interpretation**: The write-only plus read+notify pairing has the shape of a command/response pair.
- **Confidence**: Confirmed (structure and partial contents); Unknown (meaning)
- **Implementation status**: Not implemented

## SVC-004 — Vendor service B

- **Handles**: `0x000f`–`0x0012`
- **UUID**: `01000002-1989-0108-1234-123456789abc`
- **Observation**: 1 characteristic, `01000001` at `0x0011` (Write and Notify, no Read; CCCD at `0x0012`). It has never been written to or notified on, so its contents are unexplored — see [`open-questions.md`](open-questions.md).
- **Confidence**: Confirmed (structure only); Unknown (contents)
- **Implementation status**: Not implemented

## SVC-005 — Settings/info service

- **Handles**: `0x0013`–`0x0044`
- **UUID**: `10100000-5354-4f52-5a26-4249434b454c` — the trailing 12 bytes (last four UUID groups) decode as ASCII `STORZ&BICKEL`; characteristics use the same base with `10100001`–`10100016` as the leading group
- **Observation**: 22 characteristics, all read at least once. See [`characteristics.md`](characteristics.md) for the ones with a known identity, and [`open-questions.md`](open-questions.md) for the rest, which have confirmed raw values but unknown meaning.
- **Interpretation**: Holds device information (serial number, firmware versions), general settings (vibration, display-on-cooling), and the heater/pump/vibration status register.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## SVC-006 — Control/actuator service

- **Handles**: `0x0045`–`0x006f`
- **UUID**: `10110000-5354-4f52-5a26-4249434b454c`; characteristics use `10110001`–`10110017` (`10110006`–`1011000b` unused/not assigned)
- **Observation**: 17 characteristics, all read at least once. See [`characteristics.md`](characteristics.md) for the ones with a known identity, and [`open-questions.md`](open-questions.md) for the rest.
- **Interpretation**: Holds the device's controllable state: temperature, heater, pump, LED brightness, and auto-shutoff.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## SVC-007 — Unidentified service

- **Handles**: declared as `0x0070`–`0xffff` (the service runs to the end of the attribute database); its attributes occupy `0x0070`–`0x0089`
- **UUID**: `10130000-5354-4f52-5a26-4249434b454c`; characteristics `10130001`–`1013000b` plus `101300ff`
- **Observation**: 12 characteristics, all read at least once — raw values confirmed but meaning entirely unknown. Eleven are read-only; `101300ff` is Read/Write/Notify and carries the service's only CCCD, at `0x0089`. See [`open-questions.md`](open-questions.md).
- **Confidence**: Confirmed (structure and raw values); Unknown (contents/meaning)
- **Implementation status**: Not implemented
