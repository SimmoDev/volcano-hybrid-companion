# Open questions

This file tracks anything classified **Unknown** per [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md). Per [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md), each entry names the question, what's already been observed, and what would resolve it.

Several entries below include a raw GATT property listing: `R` = Read, `W` = Write, `N` = Notify. Third-party sources cited below are listed in [`README.md`](README.md#third-party-sources).

Most of what remains needs this project's own BLE client rather than further observation of the official app. That app exercises a fixed set of operations, clamps its own inputs, subscribes to 8 of the device's 16 notify-capable characteristics, and never writes most of the writable ones — so any question that turns on writing an unexercised characteristic, or on issuing a value outside the range the app will send, is beyond what observing it can answer. Those are gated on the Volcano component reaching the point where it can drive the device itself. A few of the entries below need neither: they turn on device behaviour that can be observed with the app, a second host, or simply time.

## `0x0015` — unknown string value

See [CHAR-004](characteristics.md#char-004--unknown-string-value). Reads as ASCII `"222"` (space-padded). No corresponding value observed in the app UI. One third-party source (unverified) names this Bootloader Version. A second third-party source shows `"222"` appearing in a different context — as part of a bootloader-mode handshake response string (`"RV0 222 BL"`), reached via a separate firmware-update procedure, not confirmed to be the same value as this characteristic in normal operation. Would be resolved by correlating a normal-operation read against a firmware-update-mode capture directly.

## `0x0042` / `0x0044` — unknown values

See [CHAR-011](characteristics.md#char-011--unknown-value-hist1) and [CHAR-012](characteristics.md#char-012--unknown-value-hist2). `0x0042` reads as ASCII `"616161…"`; `0x0044` reads as ASCII all-zero-character text (not raw null bytes). A third-party source (unverified) names these HIST1 and HIST2 respectively; no other third-party source mentions either. What either value means is unknown, and neither moves. Read repeatedly across pump activations, heating cycles, target changes, reconnections, an auto-shutoff expiry, and a change to each of the four settings the app exposes — vibration, display on cooling, LED brightness and auto-shutoff duration — `0x0042`'s 16 bytes were identical every time, and `0x0044` never left its all-zero state. Every action a user can perform through the app has now been tried, and none moves either value. Neither value corresponds to anything the app displays. The official client reads both as part of its Analysis function and, when a fault is present, renders them into a plain-text block alongside the serial number and a timestamp for the user to send to Storz & Bickel support — so they appear to be service/diagnostic history intended for the manufacturer rather than anything a user-facing client interprets. That is taken from the client only and has not been independently verified, and it does not establish what the values encode. A value that stays byte-identical across every action a user can perform looks more like a fixed per-device characteristic than a counter, which would change what this question is asking. Would be resolved by reading both after a period of days of ordinary use, and by reading them on a second unit: a counter would move over time on one device, whereas a per-device constant would differ between two.

## `0x002b` — remaining bits still unresolved

Heater-on/off tracking, the vibration-motor pulse, the pump-active bits, bit 9's latch behaviour and bit 10's clear-while-heating behaviour are Confirmed — see [STATE-008](state-model.md#state-008--statusflags-register-partial), corroborated by two independent third-party sources for the heater/pump bit positions specifically. What remains open:

- **The error mask.** Both third-party sources and the official client agree on `0x4018` (bits 3, 4 and 14), but no value with any of those bits set has ever been observed, so the mask is independently unverified. Would be resolved by reading the register during an actual error condition, if one can be safely induced.
- **What bit 9's latch is consumed for.** Its behaviour is established — it sets on the first target-reach after heater-on and clears only at heater-off — but not its purpose. The client's name for it, `ENABLE_AUTOBLESHUTDOWN`, is contradicted by that behaviour and is not adopted. Would be resolved by finding a device behaviour that changes depending on whether the target has been reached at least once in the current heating cycle.
- **What governs bit 10's set edge.** The clear edge is exact — the target must sit at least 3 °C above current. The set edge ([STATE-008](state-model.md#state-008--statusflags-register-partial)) matches no fixed rule: neither the remaining gap, the elapsed delay, nor the implied time to arrival is constant, though both gap and delay grow with the size of the climb. Whatever the device is actually tracking is not visible in temperature and time alone. Would be resolved by correlating the set edge against something other than the temperature curve — heater duty cycle would be the natural candidate if it were observable, so in practice this may need a vendor source rather than more measurement. Low priority: the practical conclusion — that the bit is unusable as a near-target indicator — does not depend on the answer.
- **Bits 0 and 1.** Set in every value observed while the heater is on and clear when it is off, with no independent variation, so they carry no distinguishable meaning yet. Unnamed by the client. Would be resolved by finding any condition that moves them independently of bit 5.

Low priority overall: the practically useful bits — heater, pump and vibration — are already understood.

## `0x002e` bit 13 — what the temperature-change pulse is for

See [STATE-009](state-model.md#state-009--temperature-step-pulse-on-displayunits-register). The mechanism is established: a pulse per 1 °C change of current temperature, in either direction, merging into a continuous clear when the temperature moves faster than the pulse can complete. What the pulse is *for* is still unexplained — the display does not blink while cooling, and nothing else visible on the device changes in step with it. Two loose ends remain within the model. The bit rests clear, with no pulses, while the device is showing no temperature at all, which a pure per-change pulse does not by itself account for. And the spacing thresholds do not match the pulse length: a pulse of about a second should resolve at 1.5-second spacing and largely does not, so either the pulse is longer than it appears or something beyond spacing governs whether it resolves. Would be resolved by finding an observable device behaviour that coincides with the pulse, or by identifying the bit in a vendor source. Low priority: a controller can read temperature changes from `0x0047` directly and has no need for this bit.

## Vendor services `00000001` and `01000001` — contents largely unexplored

See SVC-003 and SVC-004 in [`gatt-services.md`](gatt-services.md#svc-003--vendor-service-a). SVC-003's read+notify characteristic (`00000003-1989-0108-1234-123456789abc`) has been read: single byte, value `0xAE`, meaning unknown. Its write-only characteristic (`00000002`) has never been written to. SVC-004's single characteristic (`01000001-1989-0108-1234-123456789abc`) has no Read property at all, so it can't be explored this way — it has never been written to or notified on. Would be resolved by writing exploratory values to the write-capable characteristics and observing any response/notification, or by capturing app traffic that exercises them.

## `10100…` characteristics with confirmed raw values but unknown meaning

See [`characteristics.md`](characteristics.md) for the 12 of 22 characteristics that have a dedicated finding entry (CHAR-004 through CHAR-012, CHAR-024, CHAR-025, CHAR-026) — all 12 have a directly observed handle/UUID pairing. Three of those 12 (CHAR-004, CHAR-011, CHAR-012) have a Confirmed value but a still-Unknown meaning. The remaining 10 UUIDs, all with a UUID base of `...-5354-4f52-5a26-4249434b454c`, have a confirmed handle and raw value but no identified meaning. All values below were stable on this device (not observed to change); whether they are identical across units has not been tested:

| UUID prefix | Handle | Properties | Value |
|---|---|---|---|
| `10100002` | `0x0017` | R | ASCII, 8 digits (possibly a device- or batch-specific code — exact value not recorded here) |
| `10100005` | `0x001d` | R | ASCII `"V01.03.00.00"` — duplicate of the firmware version string (CHAR-005); purpose unknown |
| `10100009` | `0x0025` | R W | ASCII, 10 digits, zero-padded (possibly a device- or batch-specific code — exact value not recorded here) |
| `1010000a` | `0x0027` | R W | ASCII `"000000"` |
| `1010000b` | `0x0029` | R | ASCII `"000000"` |
| `1010000f` | `0x0034` | N R W | `0x00003000` (12288) |
| `10100010` | `0x0037` | N R | `0x00000014` (20) |
| `10100012` | `0x003c` | R W | `0x00` |
| `10100013` | `0x003e` | R W | `0x00` |
| `10100014` | `0x0040` | R W | `0x00` |

Would be resolved by reading each across a change of device state — before and after a heating cycle, a pump run and a settings change — to find any that move, and by writing to the R/W ones one at a time and observing whether the device's behaviour or display alters. The two Notify-capable entries (`1010000f`, `10100010`) should be subscribed first, since a value that notifies will identify itself without any write. One concrete lead: the official client writes a page count for the firmware binary during its update sequence, alongside the code number it writes to `10100011` ([CHAR-026](characteristics.md#char-026--firmware-update-code-number)). The three unidentified `R W` characteristics reading `0x00` — `10100012`, `10100013`, `10100014` — sit in the same UUID neighbourhood and are the obvious candidates for it.

## `10110…` characteristics with confirmed raw values but unknown identity

See [`characteristics.md`](characteristics.md) for the 11 of 17 characteristics with an identified meaning (CHAR-013 through CHAR-023) — all 11 have a directly observed handle/UUID pairing. The remaining 6 split into two groups.

Four standalone characteristics, none of them described by any third-party source. The first three are stable (not observed to change); the fourth is not, as noted:

| UUID prefix | Handle | Properties | Value | Notes |
|---|---|---|---|---|
| `10110002` | `0x004a` | R | `0x00000000` | Sits between current temperature (CHAR-013) and target temperature (CHAR-014) by handle order |
| `10110004` | `0x004f` | N R W | `0x00000000` | Sits immediately after target temperature |
| `1011000e` | `0x0059` | N R | `0x00B4` (180) | Sits between Auto-shutoff duration (CHAR-017) and the trigger-characteristic cluster below |
| `10110017` | `0x006e` | N R | An incrementing value, growing steadily between reads (exact value not recorded here, since it behaves like a usage counter) | Last characteristic in the service, immediately after Minutes of Operation (CHAR-023) |

For these four, would be resolved by subscribing to the three Notify-capable ones (`10110004`, `1011000e`, `10110017`) and exercising the device through heating, pumping and auto-shutoff while watching which operations move them, and by polling the read-only `10110002` across the same operations. The official app subscribes to none of them, so nothing about their notification behaviour has been observed passively. `10110017`'s growth in particular should be correlated against a counted action, as with `0x0042` above — and, given that both operating-time counters turned out to advance only while the heater is on ([STATE-001](state-model.md#state-001--hours-of-operation), [STATE-006](state-model.md#state-006--minutes-of-operation)), its growth should be checked against heater state before being assumed to track connected time.

The remaining two are part of the same cluster of six 1-byte, Read/Write, value-`0x00` trigger characteristics as CMD-006 through CMD-009, sitting in the handle range between Auto-shutoff duration (CHAR-017, `0x0057`) and Hours of Operation (CHAR-022, `0x0068`). Four of the six are identified as CHAR-018 through CHAR-021 (see above); the other two, at handles `0x0060` (UUID `10110011`) and `0x0062` (UUID `10110012`), are additional triggers whose purpose is unidentified. All six read identically (`0x00`) and are indistinguishable by passive read alone — identifying the purpose of the remaining two would require writing to one at a time and observing which UUID's write causes a real device action, done deliberately rather than as a blind sweep (writing the wrong one fires a real heater/pump action).

## `10130…` characteristics — fully read, entirely unidentified

See [`gatt-services.md`](gatt-services.md#svc-007--unidentified-service). All 12 characteristics have been read, but nothing about their meaning is known — no third-party source or app-displayed value corresponds to any of them. All 12 values are stable rather than counter-like (unlike the flagged values elsewhere in this file), but stable only in the sense of not changing on this one device: the non-zero values in particular (`0x000080DC`, `0x00000049`, `0x002C`, `0x00000221`, `0x0000005B`) have the shape of per-unit calibration or trim constants, and whether they are identical across units has not been tested.

| UUID prefix | Handle | Properties | Value |
|---|---|---|---|
| `10130001` | `0x0072` | R | `0x000080DC` (32988) |
| `10130002` | `0x0074` | R | `0x00000000` |
| `10130003` | `0x0076` | R | `0x00000000` |
| `10130004` | `0x0078` | R | `0x00000049` (73) |
| `10130005` | `0x007a` | R | `0x002C` (44) |
| `10130006` | `0x007c` | R | `0x00000000` |
| `10130007` | `0x007e` | R | `0x0000` |
| `10130008` | `0x0080` | R | `0x00000000` |
| `10130009` | `0x0082` | R | `0x0000` |
| `1013000a` | `0x0084` | R | `0x00000221` (545) |
| `1013000b` | `0x0086` | R | `0x0000005B` (91) |
| `101300ff` | `0x0088` | N R W | 20 bytes read, all zero. The 23-byte ATT MTU caps a read response at 22 bytes of value (see [CONN-001](gatt-services.md#conn-001--connection-security-and-subscription-procedure)), so 20 bytes sits below the cap and is most likely the true length rather than a truncation. A read-blob request at offset 20 would confirm it |

Would be resolved by subscribing to the one Notify-capable entry (`101300ff`) to see whether anything moves it, issuing the read-blob check its table row proposes, and correlating the rest against app UI fields not yet mapped to a handle or against third-party documentation of this service.

## Static bits in the settings registers

See [CHAR-010](characteristics.md#char-010--vibration-setting) and [CHAR-009](characteristics.md#char-009--display-on-cooling--units-register). Reading `0x0031` with vibration enabled and then disabled changes exactly two bits — the setting bit and bit 16 — leaving several other bits set in both states. Those remaining bits correspond to nothing the app displays and do not move with the only setting the register is known to carry, so either they encode further settings or state not yet exercised, or they are fixed device characteristics. The same question applies to `0x002e`, whose reads carry bits beyond the four identified there. Would be resolved by reading both registers across every setting the app can change and every device state that can be reached, and treating whatever never moves as static — then probing the static bits individually with the mask-and-action write form the register already accepts.

## `0x002e` units bit — whether it can be written

See [STATE-010](state-model.md#state-010--temperature-display-units-bit). Bit `0x0200` of this register tracks the device's Celsius/Fahrenheit display setting, and the register accepts the settings-bit write form used by [CMD-004](commands.md#cmd-004--set-vibration) and [CMD-005](commands.md#cmd-005--set-display-on-cooling). Whether that form works for this particular bit — changing the display units remotely — has never been attempted: the official app exposes no control for it, and the setting is changed with a physical panel gesture instead. A controller that wants to present temperatures in the user's preferred units needs to know whether it can set this or must read it and convert. Would be resolved by writing the settings-bit form for mask `0x0200` in each direction and observing whether the device's display units follow.

## Whether a target-temperature write can be silently dropped

See [STATE-013](state-model.md#state-013--target-temperature-notifications). A write is occasionally answered within a fraction of a second by a notification carrying the previously-set target rather than the one just written, and that write then has no effect until the client sends it again. These are the only occasions on which a client-written value has come back as a notification, and they are consistent with the write being rejected or dropped while the device was still applying the previous one. Each occurrence followed a run of writes a few seconds apart, with the value notified back being a target the current temperature had just reached. If writes can be dropped this way, a controller cannot treat a successful ATT write response as confirmation that the target changed. Would be resolved by issuing closely spaced target writes and reading the characteristic back after each, which also overlaps with the write-pacing question below.

## What bit 16 of the settings registers signifies

See the setting-bit note in [`commands.md`](commands.md#note-setting-bit-writes). Bit 16 of both `0x0031` and `0x002e` moves with that register's named setting — clear when the feature is enabled, set when disabled — reproduced in both directions on both registers. It therefore carries no information the setting bit does not, which is what makes it odd. Its position is a lead: byte 2 of the write payload, the clear/set selector, sits at exactly bit 16 when that payload is read as a 32-bit little-endian value, so the register may simply be echoing the last write's action byte rather than holding a distinct flag. Would be resolved by writing the register with a mask naming a bit other than the setting bit and then reading it back: if bit 16 follows the action byte of whatever write was last issued, it is an artefact of the write path rather than a field of its own. A no-write version of the same test is already available: the units bit on `0x002e` is changed by a physical panel gesture ([STATE-010](state-model.md#state-010--temperature-display-units-bit)), so toggling units at the device and reading the register back shows whether bit 16 responds to a setting change that involved no write at all. The status register `0x002b`, which has the same shape and is never written, has never shown bit 16 set — which points the same way.

## Whether the device retains sub-degree target precision

See [CMD-001](commands.md#cmd-001--set-target-temperature). The app sends sub-degree targets when running in Fahrenheit — a write of 229.5 °C has been captured — but whether the device stores that or rounds it on receipt is unknown. The target characteristic reads back in the same 0.1 °C encoding as it is written, and is a different attribute from current temperature, whose whole-degree rounding does not apply to it. Would be resolved by writing a sub-degree target and immediately reading `0x004c` back.

## Target temperature — valid/safe range not established

See [CMD-001](commands.md#cmd-001--set-target-temperature). The write format is confirmed, and the official app's UI exposes a settable range of 40–230 °C (104–446 °F). Writes at both ends of that range have been captured: the minimum is `90 01 00 00` (400 = 40.0 °C = 104.0 °F) and the maximum `fc 08 00 00` (2300 = 230.0 °C = 446.0 °F), so the app's displayed bounds correspond exactly to the raw values it sends, in both unit modes.

What remains unresolved is what the device itself does outside that range — whether it accepts, clamps, rejects, or ignores such a write — and therefore whether the app's range is a device-enforced limit or only a UI constraint. Since this characteristic drives the heating element directly, an implementation must treat 40–230 °C as the safe bound and must not write outside it until the device's own behaviour there is known. Would be resolved by writing values beyond each bound and observing the device's response.

## What the advertisement's manufacturer-data remainder encodes

See [ADV-001](gatt-services.md#adv-001--advertising-and-discovery). Fourteen bytes follow the company identifier and serial number in the manufacturer-specific data, and what they hold is unidentified. They are byte-identical whether the device is idle or actively heating, at an unchanged advertising interval, so heater activity cannot be detected by scanning; whether they vary with any other state is untested — the pump running alone, an armed auto-shutoff countdown, and an error condition have not been compared. This bounds what a client can learn without connecting, which matters because connecting is exclusive: anything knowable from a scan is knowable without taking the device away from other clients. Would be resolved by capturing advertisements in each of those states with nothing connected and diffing the remainder against the idle baseline, which would both settle the variance question and show which bytes, if any, are dynamic.

## BLE connection procedure — remaining unknowns

Most of the connection procedure is recorded in [ADV-001](gatt-services.md#adv-001--advertising-and-discovery), [CONN-001](gatt-services.md#conn-001--connection-security-and-subscription-procedure) and [CONN-003](gatt-services.md#conn-003--single-connection-at-a-time): advertising contents (including that no service UUIDs are advertised, and that advertising stops while connected), the 23-byte ATT MTU, the absence of pairing or encryption for every operation exercised, the CCCD writes required to subscribe, and the device's advertised write-type support (Write with response on every writable characteristic, with SVC-003's `00000002` additionally advertising Write Without Response). What the advertising payload itself carries is tracked separately above. What is still open here:

- Whether a directed connection to the device's known address behaves differently from a scan-then-connect. The device has only ever been connected to by scanning first, so whether a direct connection to a remembered address fails cleanly, hangs, or succeeds where a scan-then-connect could not is untested. This is not on the path this project takes — ESPHome's BLE client connects only after its tracker sees a matching advertisement, per [ADR-0007](../decisions/ADR-0007-ble-connection-lifecycle.md) — so it matters only to a client that bypasses that, and is low priority accordingly. It is also not reachable with the official app, whose connect action always opens a chooser and scans. Would be resolved by issuing a direct connection to the known address while a second host holds the link, and recording whether an error is returned or the attempt simply times out.
- Whether any characteristic not yet exercised requires an encrypted link. Service discovery, reads, CCCD writes, and the writes to target temperature, settings and the actuator triggers all succeeded over an unencrypted link with no pairing ([CONN-001](gatt-services.md#conn-001--connection-security-and-subscription-procedure)), but a device only reveals a security requirement when an operation is actually attempted, and several writable characteristics have never been written. Would be resolved by attempting a read or write on each untouched characteristic and recording whether the device returns an Insufficient Authentication or Insufficient Encryption error.

## How long a client can be absent before the device intervenes

A client disconnect does not stop the heater or the pump: both keep running and the auto-shutoff countdown continues uninterrupted ([CONN-004](gatt-services.md#conn-004--actuator-behaviour-across-a-client-disconnect)). No cut-off has been seen on any disconnection. Whether one exists on a longer timescale than has been waited out is unestablished. The auto-shutoff countdown is the only known backstop, and it is a substantially better one than the app's own UI suggests: the app's shortest configurable duration is 30 minutes, but the device itself accepts far less, down to at least 60 seconds, honoured through to an actual expiry ([CMD-003](commands.md#cmd-003--set-auto-shutoff-duration)). What remains open is whether some independent device-side cut-off exists on top of that countdown, for a client absence longer than has been waited out. Would be resolved by starting the heater, disconnecting, and leaving the client off for 15 minutes and then for a full auto-shutoff period, reconnecting each time to read the status register and countdown.

## What the trigger characteristics do with a value other than `00`

See [CMD-006](commands.md#cmd-006--heater-on) through [CMD-009](commands.md#cmd-009--pump-off). Every write ever issued to the four heater and pump triggers has carried the value `00`, so what a different value does — ignored, rejected, or treated as a distinct command — is unknown. The two unidentified triggers beside them (`0x0060` and `0x0062`) have never been written at all. This matters more than most entries here because these characteristics actuate a heating element: an implementation that assumes the payload is ignored has no basis for that assumption, and writing to the wrong handle or with the wrong value is not a recoverable mistake. Would be resolved by writing a non-`00` value to a trigger whose action is harmless to repeat — pump-off with the pump already off — and recording whether the device returns an ATT error, actuates, or does nothing.

## Device error responses

Aside from the out-of-range temperature write above, nothing records how the device responds to a malformed or disallowed operation — a wrong-length write, a read of a write-only characteristic, or a write to a read-only one. An implementation has no basis for deciding which ATT error codes to expect, retry, or treat as fatal. Would be resolved by issuing each such operation deliberately against a harmless characteristic and recording the error code returned.

## Whether write pacing is a device requirement

[CMD-001](commands.md#cmd-001--set-target-temperature) records that the app debounces a burst of temperature-stepper presses into a single write, but not whether the device *requires* that pacing or whether the app does it purely for its own reasons. A client issuing one write per step has no guidance on whether it risks dropped writes or a queue overrun. Would be resolved by issuing rapid successive writes and checking whether each is acknowledged and reflected in a subsequent read.

See [`docs/protocol/README.md`](README.md) for how this directory as a whole works.
