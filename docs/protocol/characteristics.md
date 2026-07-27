# Characteristics

Individual characteristics within the Volcano Hybrid's GATT services. Format per [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md); confidence per [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md). See [`commands.md`](commands.md) for write-triggered behaviour and [`state-model.md`](state-model.md) for readable/notified state, both with full finding detail. See [`gatt-services.md`](gatt-services.md) for the service each characteristic belongs to. Third-party sources cited below are listed in [`README.md`](README.md#third-party-sources).

Every handle, UUID, and property listed below comes from full GATT service discovery, performed afresh on each of many separate connections and returning an identical map every time, so handle-to-UUID pairings are directly observed rather than inferred. Every read and write recorded elsewhere in this directory used those handles and reached the expected characteristic.

Each notify-capable characteristic's Client Characteristic Configuration descriptor sits at its value handle plus one, which is how the eight CCCD handles listed in [CONN-001](gatt-services.md#conn-001--connection-security-and-subscription-procedure) relate to the characteristics they subscribe to.

## CHAR-001 — Device Name

- **Handle**: `0x0003`
- **UUID**: `00002a00-0000-1000-8000-00805f9b34fb` (Bluetooth SIG standard)
- **Service**: SVC-001 (GAP)
- **Properties**: Read/Write
- **Observation**: Value `"S&B VOLCANO H"`.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-002 — Appearance

- **Handle**: `0x0005`
- **UUID**: `00002a01-0000-1000-8000-00805f9b34fb` (Bluetooth SIG standard)
- **Service**: SVC-001 (GAP)
- **Properties**: Read
- **Observation**: Value `0x0000`.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-003 — Peripheral Preferred Connection Parameters

- **Handle**: `0x0007`
- **UUID**: `00002a04-0000-1000-8000-00805f9b34fb` (Bluetooth SIG standard)
- **Service**: SVC-001 (GAP)
- **Properties**: Read
- **Observation**: Standard-format value: minimum connection interval 20 ms, maximum connection interval 70 ms, peripheral latency 0, supervision timeout 400 × 10 ms = 4 s.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-004 — Unknown string value

- **Handle**: `0x0015`
- **UUID**: `10100001-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read
- **Observation**: 12-byte value; reads as ASCII `"222"` followed by space padding. No corresponding value observed in the app UI.
- **Interpretation**: A third-party source (unverified) names this Bootloader Version — see [`open-questions.md`](open-questions.md) for the unresolved lead.
- **Confidence**: Confirmed (value); Unknown (meaning)
- **Implementation status**: Not implemented

## CHAR-005 — Firmware version

- **Handle**: `0x0019`
- **UUID**: `10100003-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read
- **Observation**: See [STATE-003](state-model.md#state-003--firmware-version).
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-006 — Firmware BLE version

- **Handle**: `0x001b`
- **UUID**: `10100004-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read
- **Observation**: See [STATE-004](state-model.md#state-004--firmware-ble-version).
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-007 — Serial number

- **Handle**: `0x0023`
- **UUID**: `10100008-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read/Write
- **Observation**: 10-byte ASCII string, unique per device — see [STATE-002](state-model.md#state-002--serial-number). The characteristic is writable, and overwriting it would replace a real device's serial number, so it must not be written.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-008 — Status/flags register

- **Handle**: `0x002b`
- **UUID**: `1010000c-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read/Notify
- **Observation**: 4-byte value carrying heater on/off, pump-active, and vibration-motor-activation flags — see [STATE-008](state-model.md#state-008--statusflags-register-partial).
- **Confidence**: Confirmed (identity, partial bit layout, behaviour); Unknown (full bit layout — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

## CHAR-009 — Display on cooling / units register

- **Handle**: `0x002e`
- **UUID**: `1010000d-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read/Write/Notify
- **Observation**: Display on Cooling setting — see [CMD-005](commands.md#cmd-005--set-display-on-cooling); Celsius/Fahrenheit units bit — see [STATE-010](state-model.md#state-010--temperature-display-units-bit); an additional bit that pulses once per 1 °C change of current temperature in either direction, for reasons not yet understood — see [STATE-009](state-model.md#state-009--temperature-step-pulse-on-displayunits-register); and bit 16 — see the setting-bit note in [`commands.md`](commands.md#note-setting-bit-writes). A read returns the same 4-byte value as a notification, with the same bit layout. Read in both display-on-cooling states and with the temperature-change pulse bit both set and clear, the value returned matched what the register was notifying at the time. Bit 16 (`0x10000`) accompanies the display-on-cooling bit, clear when the feature is enabled and set when it is disabled, mirroring the same pairing on `0x0031` ([CHAR-010](#char-010--vibration-setting)). The write payload has a different layout again ([CMD-005](commands.md#cmd-005--set-display-on-cooling)): reads and notifications carry register state, writes carry a mask-and-action.
- **Confidence**: Confirmed (identity, the bit positions, and that a read returns the notified register value); Unknown (what the temperature-change pulse and bit 16 signify — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

## CHAR-010 — Vibration setting

- **Handle**: `0x0031`
- **UUID**: `1010000e-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read/Write/Notify
- **Observation**: Reads back as a 4-byte register value, not in the mask-and-action form used to write it ([CMD-004](commands.md#cmd-004--set-vibration)). The read carries the vibration setting at the same bit position the write names, with the same inverted polarity: reading the register with vibration enabled and then disabled changes exactly two bits, bit 10 (`0x0400`) and bit 16 (`0x10000`), both clear when the feature is on and set when it is off. Every other bit in the value is unchanged between the two states and is unidentified — see [`open-questions.md`](open-questions.md).
- **Confidence**: Confirmed (identity, that the setting is written here per CMD-004, and that bit 10 of the read value carries the setting with inverted polarity); Unknown (the meaning of the register's other bits)
- **Implementation status**: Not implemented

## CHAR-011 — Unknown value (HIST1)

- **Handle**: `0x0042`
- **UUID**: `10100015-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read
- **Observation**: 16-byte value; read as ASCII, a repeating `"61"` text pattern followed by zero-character text padding (not raw null bytes). The value is stable: read repeatedly across pump activations, heating cycles, target changes, reconnections and changes to both settings the app exposes, all 16 bytes were identical every time.
- **Interpretation**: A third-party source (unverified) names this HIST1. The name suggests an accumulating history, but nothing a user can do moves it, so it may equally be a fixed per-device characteristic — see [`open-questions.md`](open-questions.md).
- **Confidence**: Confirmed (value, and that it is unmoved by every action the app can perform); Unknown (meaning)
- **Implementation status**: Not implemented

## CHAR-012 — Unknown value (HIST2)

- **Handle**: `0x0044`
- **UUID**: `10100016-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read
- **Observation**: 16-byte value; read as ASCII, all zero-character text (not raw null bytes). Like [CHAR-011](#char-011--unknown-value-hist1), the value is stable: read repeatedly across pump activations, heating cycles, target changes, reconnections and changes to both settings the app exposes, it never left this state.
- **Interpretation**: A third-party source (unverified) names this HIST2 — see [`open-questions.md`](open-questions.md) for the unresolved lead.
- **Confidence**: Confirmed (value, and that it is unmoved by every action the app can perform); Unknown (meaning)
- **Implementation status**: Not implemented

## CHAR-013 — Current (actual) temperature

- **Handle**: `0x0047`
- **UUID**: `10110001-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Notify
- **Observation**: See [STATE-007](state-model.md#state-007--current-actual-temperature).
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-014 — Target temperature

- **Handle**: `0x004c`
- **UUID**: `10110003-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Write/Notify
- **Observation**: See [CMD-001](commands.md#cmd-001--set-target-temperature) for the write side and [STATE-013](state-model.md#state-013--target-temperature-notifications) for the notify side. Reads back in the same 4-byte, 0.1 °C encoding as CHAR-013; once the heater has brought the device to a whole-degree target, the two characteristics read the same value.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-015 — LED brightness

- **Handle**: `0x0052`
- **UUID**: `10110005-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Write
- **Observation**: Reads back as a 2-byte value on the same 0–100 scale as the write — see [CMD-002](commands.md#cmd-002--set-led-brightness).
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-016 — Auto-shutoff countdown

- **Handle**: `0x0054`
- **UUID**: `1011000c-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Notify
- **Observation**: 2-byte value in seconds — see [STATE-005](state-model.md#state-005--auto-shutoff-countdown). Behaves as a live countdown, decreasing at very close to one unit per elapsed second.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-017 — Auto-shutoff duration

- **Handle**: `0x0057`
- **UUID**: `1011000d-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Write
- **Observation**: See [CMD-003](commands.md#cmd-003--set-auto-shutoff-duration). Read value uses the same 2-byte-seconds encoding as the write.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-018 — Heater on trigger

- **Handle**: `0x005c`
- **UUID**: `1011000f-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Write
- **Observation**: See [CMD-006](commands.md#cmd-006--heater-on). Reading returns `0x00`.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-019 — Heater off trigger

- **Handle**: `0x005e`
- **UUID**: `10110010-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Write
- **Observation**: See [CMD-007](commands.md#cmd-007--heater-off). Reading returns `0x00`.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-020 — Pump on trigger

- **Handle**: `0x0064`
- **UUID**: `10110013-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Write
- **Observation**: See [CMD-008](commands.md#cmd-008--pump-on). Reading returns `0x00`.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-021 — Pump off trigger

- **Handle**: `0x0066`
- **UUID**: `10110014-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Write
- **Observation**: See [CMD-009](commands.md#cmd-009--pump-off). Reading returns `0x00`.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-022 — Hours of Operation

- **Handle**: `0x0068`
- **UUID**: `10110015-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Notify
- **Observation**: See [STATE-001](state-model.md#state-001--hours-of-operation).
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-023 — Minutes of Operation

- **Handle**: `0x006b`
- **UUID**: `10110016-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-006
- **Properties**: Read/Notify
- **Observation**: See [STATE-006](state-model.md#state-006--minutes-of-operation). Confirmed as a 2-byte value.
- **Confidence**: Confirmed
- **Implementation status**: Not implemented

## CHAR-024 — Power supply rating

- **Handle**: `0x001f`
- **UUID**: `10100006-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read/Write
- **Observation**: ASCII string `"230VAC"`.
- **Interpretation**: Very likely a static power-supply rating string (230V AC mains), based on content alone — not corroborated by any third-party source. Likely varies by market/power variant (e.g. a 120V AC region), so this exact value should not be assumed universal.
- **Confidence**: Confirmed (handle, UUID and value); Probable (that the value is a power-supply rating)
- **Implementation status**: Not implemented

## CHAR-025 — Product line name

- **Handle**: `0x0021`
- **UUID**: `10100007-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read/Write
- **Observation**: ASCII string `"HYBRID"`.
- **Interpretation**: Very likely a static product-line identifier, based on content alone — not corroborated by any third-party source.
- **Confidence**: Confirmed (handle, UUID and value); Probable (that the value is a product-line identifier)
- **Implementation status**: Not implemented

## CHAR-026 — Firmware-update code number

- **Handle**: `0x003a`
- **UUID**: `10100011-5354-4f52-5a26-4249434b454c`
- **Service**: SVC-005
- **Properties**: Read/Write
- **Observation**: Reads as a 2-byte value of `0x0000` in normal operation. Never written by the app during ordinary use.
- **Interpretation**: The characteristic appears to gate or initiate firmware update rather than to affect normal operation. Not independently verified — no firmware update has been observed.
- **Confidence**: Confirmed (value and properties); Probable (purpose, from the client only)
- **Note**: the official client names this the code-number characteristic and writes the constant `4711` to it only as part of its firmware-update sequence, alongside a page count for the firmware binary.
- **Implementation status**: Not implemented

Every characteristic has been read at least once except SVC-004's and SVC-003's write-only `00000002` — neither has a Read property, so neither can be. Several more characteristics beyond the ones listed above have confirmed raw values but no identified meaning; rather than list each as a stub finding here, they are tracked in [`open-questions.md`](open-questions.md), grouped by service. SVC-004's structure is known from service discovery, but nothing of its contents or behaviour is.
