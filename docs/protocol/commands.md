# Commands

Write-triggered behaviour. Format per [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md); confidence per [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md). Third-party sources cited below are listed in [`README.md`](README.md#third-party-sources).

**Provenance of these findings.** Every write documented below was observed by capturing the official Storz & Bickel web app's BLE traffic while operating the device, then correlating it against the resulting physical behaviour. Each write's encoding and effect is Confirmed on that basis. None has yet been issued from this project's own ESP32/ESPHome client, whose MTU negotiation, write pacing and queueing differ from a browser's, so every finding here carries an **Implementation status** of *Not implemented*. Write type and the absence of a pairing requirement are covered by [CONN-001](gatt-services.md#conn-001--connection-security-and-subscription-procedure); [`open-questions.md`](open-questions.md) tracks what remains open about the connection.

## CMD-001 — Set target temperature

- **Handle**: `0x004c`
- **Observation**: Writing a 4-byte little-endian value in units of 0.1 °C sets the target temperature (e.g. raw bytes `fc 08 00 00` = 2300 = 230.0 °C). Repeated presses of the app's temperature stepper are debounced — one write carrying the final value is sent per burst, not one write per press. Reproduced across multiple stepping sequences and multiple sessions, and the 4-byte width confirmed directly from captured writes. The characteristic also notifies when the target is changed at the device itself — see [STATE-013](state-model.md#state-013--target-temperature-notifications).

  Separately observed: with the app in Fahrenheit mode and displaying 445 °F, it wrote `f7 08 00 00` (2295 = 229.5 °C). 445 °F is 229.44 °C, so the value sent is not a direct conversion.
- **Interpretation**: The value is always Celsius-encoded (×10) regardless of the device's display unit setting — see [STATE-007](state-model.md#state-007--current-actual-temperature) for the corresponding read-side behaviour. A third-party source additionally claims temperature reads always round to the nearest °C regardless of display units, while writes accept full precision. The read half of that claim is borne out directly: every current-temperature value observed is a whole number of degrees Celsius, without exception ([STATE-007](state-model.md#state-007--current-actual-temperature)). The write half is consistent with what the official client sends: the 229.5 °C write recorded above appears to be a conversion rounded to the nearest 0.5 °C — whether the device retains the sub-degree part or rounds it on receipt is not established. Note that the whole-degree rounding in STATE-007 is a property of current temperature (`0x0047`); the target characteristic is a separate attribute that reads back in the same 0.1 °C encoding, so reading `0x004c` after a sub-degree write would settle this directly — see [`open-questions.md`](open-questions.md).
- **Confidence**: Confirmed (write behaviour, Celsius encoding, that current-temperature reads are always whole degrees, and that the client issues sub-degree writes); Unknown (whether the device retains sub-degree write precision — see [`open-questions.md`](open-questions.md))
- **Note**: the official app's UI exposes a settable range of 40–230 °C (displayed as 104–446 °F). The app clamps to that range: once the maximum is reached, further increase presses re-send the same `fc 08 00 00` (2300 = 230.0 °C) rather than a higher value. What the device itself accepts, clamps, or rejects outside the range has not been tested — see [`open-questions.md`](open-questions.md).
- **Implementation status**: Not implemented

## CMD-002 — Set LED brightness

- **Handle**: `0x0052`
- **Observation**: 2-byte little-endian value (only the low byte significant), 0–100 scale (e.g. raw bytes `64 00` = 100). Observed across several distinct brightness settings, each write matching the level selected in the app.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CMD-003 — Set auto-shutoff duration

- **Handle**: `0x0057`
- **Observation**: 2-byte little-endian value in seconds (e.g. raw bytes `30 2a` = 10800s = 180 min). The app's UI spans 30 minutes (`08 07` = 1800s) to 360 minutes (`60 54` = 21600s); writes at both ends of that range have been captured. Observed across several distinct duration settings and reproduced across multiple sessions. The write sets the duration used the next time the countdown arms — which is any actuator starting from idle, not heater-on specifically; it does not reload a countdown already running. See [STATE-005](state-model.md#state-005--auto-shutoff-countdown).
- **Confidence**: Confirmed (encoding, that the app's UI spans that range, and that the write applies at the next arming rather than reloading a running countdown — see [STATE-005](state-model.md#state-005--auto-shutoff-countdown))
- **Note**: 30 minutes is the *app's* lower bound, not a demonstrated device limit. Whether the device accepts a shorter duration is untested and matters — a shorter auto-shutoff is the cheapest available mitigation for a controller dying with the heater on. See [`open-questions.md`](open-questions.md).
- **Implementation status**: Not implemented

## CMD-004 — Set vibration

- **Handle**: `0x0031`
- **Observation**: 4-byte write in the register-bit form described in the note below: raw bytes `00 04 00 00` = on, `00 04 01 00` = off. The first two bytes are the bit mask `0x0400`, and the third selects clear (`00`) or set (`01`). Reproduced across repeated toggles in both directions. A third-party source states `0x000400` = on and `0x010400` = off, which matches these bytes exactly once read as a 32-bit little-endian value.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CMD-005 — Set display on cooling

- **Handle**: `0x002e`
- **Observation**: 4-byte write in the same register-bit form as CMD-004: raw bytes `00 10 00 00` = on, `00 10 01 00` = off, where `0x1000` is the bit mask and the third byte selects clear or set. Reproduced across repeated toggles in both directions. The register notifies as 4 bytes (see [STATE-009](state-model.md#state-009--temperature-step-pulse-on-displayunits-register) and [STATE-010](state-model.md#state-010--temperature-display-units-bit)).

  The setting governs whether the device shows the **current** temperature on its own display while cooling. With it on (bit clear), the display hides the target and shows current. With it off (bit set), the display shows no temperature at all while cooling. Observed directly against the physical display in both states.

  Bit 16 (`0x10000`) of the notified value moves with this setting: every value seen while the setting is off carries both bit 12 and bit 16, and every value seen while it is on carries neither. The register is therefore wider than the 16 bits its other documented fields occupy.

  The setting is display-only. Current temperature continues to notify over BLE unchanged while the display shows nothing, so a controller reading `0x0047` is unaffected by it.
- **Confidence**: Confirmed (write encoding, reproduced in both directions; that the setting governs whether current temperature is displayed while cooling; that bits 12 and 16 of the notified value track it together; and that BLE notification is unaffected); Unknown (what bit 16 represents beyond tracking this setting)
- **Implementation status**: Not implemented

### Note: setting-bit writes

CMD-004 and CMD-005 are two uses of one mechanism rather than two ad-hoc encodings. Both write to a bit-register: `0x0031` for vibration ([CHAR-010](characteristics.md#char-010--vibration-setting)) and `0x002e` for display and units ([CHAR-009](characteristics.md#char-009--display-on-cooling--units-register), [STATE-009](state-model.md#state-009--temperature-step-pulse-on-displayunits-register), [STATE-010](state-model.md#state-010--temperature-display-units-bit)). The status/flags register `0x002b` ([STATE-008](state-model.md#state-008--statusflags-register-partial)) has the same bit-register shape but is Read/Notify only and takes no writes. A setting is changed by writing four bytes: a 2-byte little-endian bit mask, then `00` to clear that bit or `01` to set it, then a padding byte. The write names only the bit being changed, so the register's other bits do not need to be read first or preserved.

A second bit moves with each setting. Bit 16 (`0x10000`) of the register is clear whenever the feature is enabled and set whenever it is disabled, alongside the named setting bit — on `0x0031` for vibration and on `0x002e` for display on cooling, reproduced in both directions on both registers. Toggling one setting moves only that register's pair and leaves the other register untouched. Both registers therefore read wider than the 16 bits their named fields occupy. What bit 16 distinguishes beyond duplicating the setting is unidentified — though its position is suggestive: byte 2 of the write payload, the clear/set selector, occupies exactly bit 16 of that payload read as a 32-bit little-endian value, and the register's bit 16 carries the same polarity. See [`open-questions.md`](open-questions.md).

Note the polarity is inverted for both documented settings: the bit **clear** means the feature is **enabled**. Vibration on is `00 04 00 00` (clear `0x0400`) and vibration off is `00 04 01 00` (set it). The official client applies the same inversion when rendering its toggles, and its diagnostic report independently confirms the reading: with both bits set it names vibration and display on cooling as switched off, matching the decoding of the register reads it performs. It is not universal across the register, though: the units bit on `0x002e` is not inverted, with set meaning Fahrenheit ([STATE-010](state-model.md#state-010--temperature-display-units-bit)). The inversion is a property of these two settings rather than of the registers as a whole, so a new bit's polarity has to be established rather than assumed.

## CMD-006 — Heater on

- **Handle**: `0x005c`
- **Observation**: Writing value `00` triggers the action. Reproduced across multiple sessions. Reading this handle returns `0x00` — see the note below.
- **Confidence**: Confirmed (that a write of `00` switches the heater on); Unknown (whether any other value is accepted — only `00` has ever been written — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

## CMD-007 — Heater off

- **Handle**: `0x005e`
- **Observation**: Writing value `00` triggers the action, mirroring CMD-006's pairing shape (and the pump on/off pairing, CMD-008/CMD-009). Reproduced across multiple sessions. Reading this handle returns `0x00` — see the note below.
- **Confidence**: Confirmed (that a write of `00` switches the heater off); Unknown (whether any other value is accepted — only `00` has ever been written — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

## CMD-008 — Pump on

- **Handle**: `0x0064`
- **Observation**: Writing value `00` triggers the action. Reproduced across repeated pump toggles performed in isolation, with heater state held constant. Reading this handle returns `0x00` — see the note below.
- **Confidence**: Confirmed (that a write of `00` switches the pump on); Unknown (whether any other value is accepted — only `00` has ever been written — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

## CMD-009 — Pump off

- **Handle**: `0x0066`
- **Observation**: Writing value `00` triggers the action. Reproduced across repeated pump toggles performed in isolation, with heater state held constant. Reading this handle returns `0x00` — see the note below.
- **Confidence**: Confirmed (that a write of `00` switches the pump off); Unknown (whether any other value is accepted — only `00` has ever been written — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

### Note: the trigger characteristics

**Every trigger write recorded here carries the value `00`, and no other value has ever been sent to any of them.** Whether the device ignores the payload, requires `00`, or treats some other value as a different command is unknown, so an implementation must write `00` and nothing else. See [`open-questions.md`](open-questions.md).

Six characteristics in the control/actuator service (SVC-006) share the exact same shape — 1 byte, Read/Write, value `0x00` — in the handle range containing CMD-006 through CMD-009. Reading returns `0x00` for all six, the same value used to trigger the action; it is not an error and does not return, say, current heater/pump state (that's tracked separately by the status/flags register, [STATE-008](state-model.md#state-008--statusflags-register-partial)). CMD-006–009's UUIDs are recorded in the CHAR-018 through CHAR-021 entries in [`characteristics.md`](characteristics.md), Confirmed from service discovery. The remaining two, at handles `0x0060` (UUID `10110011`) and `0x0062` (UUID `10110012`), are additional triggers of the same shape whose purpose is unidentified — see [`open-questions.md`](open-questions.md).
