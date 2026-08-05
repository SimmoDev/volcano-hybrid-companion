# State model

Readable/notified device state. Format per [ADR-0006](../decisions/ADR-0006-protocol-documentation-structure.md); confidence per [ADR-0005](../decisions/ADR-0005-volcano-ble-discovery-methodology.md). Third-party sources cited below are listed in [`README.md`](README.md#third-party-sources).

## STATE-001 — Hours of Operation

- **Handle**: `0x0068`
- **Observation**: 4-byte little-endian integer, an incrementing count of hours, paired with [STATE-006](#state-006--minutes-of-operation) for the minutes component. It counts heater-on time rather than powered-on time: it advances only while the heater is on, and notifies in the same second that the minutes counter wraps from 59 to 0. Matches the app's displayed hours value exactly, read consistently across multiple sessions.
- **Interpretation**: An element-usage meter rather than a device-uptime meter — the pair measures how long the heater has run, not how long the device has been plugged in.
- **Confidence**: Confirmed
- **Implementation status**: Implemented (read-only) in `components/volcano/volcano.cpp`. Verified against real hardware, including the carry: [STATE-006](#state-006--minutes-of-operation) wrapping from 59 to 0 was seen to increment this counter. The interpretation above is not encoded: the component reports the counter rather than deriving a runtime from it.

## STATE-002 — Serial Number

- **Handle**: `0x0023`
- **Observation**: 10-byte ASCII string, unique per device. The app displays a truncated 8-character prefix of it. The full 10 bytes are also broadcast in the advertising payload (see [ADV-001](gatt-services.md#adv-001--advertising-and-discovery)). Read consistently across multiple sessions.
- **Confidence**: Confirmed
- **Implementation status**: Implemented (read-only) in `components/volcano/volcano.cpp`, read once per connection. Verified against real hardware. Never written.

## STATE-003 — Firmware version

- **Handle**: `0x0019`
- **Observation**: ASCII string `"V01.03.00.00"`. The app displays a truncated `"V01.03.0"`. Read consistently across multiple sessions.
- **Confidence**: Confirmed
- **Implementation status**: Implemented (read-only) in `components/volcano/volcano.cpp`, read once per connection. Verified against real hardware.

## STATE-004 — Firmware BLE version

- **Handle**: `0x001b`
- **Observation**: ASCII string `"V01.00.00.00"`, exact match to the app's displayed value. Read consistently across multiple sessions.
- **Confidence**: Confirmed
- **Implementation status**: Implemented (read-only) in `components/volcano/volcano.cpp`, read once per connection. Verified against real hardware.

## STATE-005 — Auto-shutoff countdown

- **Handle**: `0x0054`
- **Observation**: 2-byte little-endian value in seconds, notifying roughly once per second while it runs. It is armed by *any* actuator, not by the heater specifically. Its full cycle:
  - It reads `0` whenever the device is fully idle — heater off and pump off.
  - It loads with the configured auto-shutoff duration ([CMD-003](commands.md#cmd-003--set-auto-shutoff-duration)) within a second of the device leaving that idle state — `1800` for the 30-minute setting — and then decrements by one per elapsed second. Both heater-on and pump-on arm it, including a pump-on with the heater off, which arms it just as a heater-on does.
  - It resets to `0` immediately when the device returns to fully idle, in the same second the status/flags register clears. It does not run on.
  - Toggling one actuator while the other keeps running does not disturb it: a pump run started and stopped during a heating cycle leaves the countdown decrementing straight through, losing only the elapsed seconds. Observed across many pump toggles.
  - Each arming loads the full duration afresh. A heater-off immediately followed by a heater-on reloads from `1800` rather than resuming.
  - Writing a new duration while it is running does not reload it: the running countdown continues from where it was, and the new duration takes effect at the next arming. Confirmed in both directions, raising and lowering the duration mid-countdown.
  - Reaching `0` switches off every actuator then running — heater, pump, or both — about a second later. See [STATE-011](#state-011--auto-shutoff-behaviour).
  - It also resets to `0` when the current temperature falls below 40 °C with the heater off, which stops any running actuator and blanks the display in the same event — see [STATE-012](#state-012--sub-40-c-reporting-and-the-idle-display-state).

  The load, the decrement rate, and the reset on returning to idle have each been reproduced across separate heating cycles and separate pump-only runs, and a full expiry has been observed end to end across many separate sessions.
- **Interpretation**: Live "time remaining until the device shuts its actuators off", armed by activity rather than free-running, and snapshotted at the moment of arming rather than tracking the configured duration continuously. Because pump-on arms it too, it is a general unattended-operation backstop rather than a heater-specific one.
- **Confidence**: Confirmed (encoding, load on heater-on and on pump-on, decrement rate, reset on return to idle, reload-in-full on each arming, indifference to a pump toggle mid-run, that a duration change does not reload a running countdown, and that reaching zero switches off whichever actuators are running)
- **Implementation status**: Implemented (the value's read and decode) in `components/volcano/volcano.cpp`. Verified against real hardware, including the load at arming, the per-second decrement, and the reset on returning to idle. The component reports the countdown rather than acting on it: none of the arming, reload or expiry behaviour documented above is encoded as logic.

## STATE-006 — Minutes of Operation

- **Handle**: `0x006b`
- **Observation**: 2-byte little-endian integer, values 0–59, matching the app's displayed minutes value. It notifies live, incrementing by exactly 1 every 60 seconds — but only while the heater is on, not merely while connected:
  - It stops advancing the moment the heater goes off, whether switched off by command or by auto-shutoff ([STATE-011](#state-011--auto-shutoff-behaviour)), and sends no notification at all for the duration of the off period. Off periods of several hours have passed with no tick and no lost count. Running the pump does not advance it — only the heater does.
  - It resumes on the next heater-on, carrying the sub-minute remainder across the off period rather than discarding it: the first tick after heater-on completes the minute that was in progress when the heater went off, arriving 60 seconds of heater-on time after the previous tick regardless of how long the device spent off in between.
  - On wrapping from 59 to 0 it carries into [STATE-001](#state-001--hours-of-operation), which notifies in the same second.

  Observed across many separate heating cycles and off periods, in both directions, and confirmed as 2 bytes wide.
- **Interpretation**: The minutes component of a heater-runtime meter, paired with [STATE-001](#state-001--hours-of-operation), ticking in real time rather than only refreshing on read. Because it is gated on heater state, an arriving tick is positive evidence that the heater is on. The inverse does not hold: ticks are 60 seconds apart, so silence is the normal condition for up to a minute of heater-on time. Bit 5 of the status/flags register ([STATE-008](#state-008--statusflags-register-partial)) gives heater state immediately and is what a controller should use.
- **Confidence**: Confirmed
- **Implementation status**: Implemented (read-only) in `components/volcano/volcano.cpp`. Verified against real hardware, including a live tick while the heater was on. The gating behaviour this finding documents is not encoded as logic: the component reports the counter rather than inferring heater state from it, which the interpretation above advises against anyway.

## STATE-007 — Current (actual) temperature

- **Handle**: `0x0047`
- **Observation**: 4-byte little-endian value, units 0.1 °C — the same width and ×10 Celsius encoding as the target temperature ([CMD-001](commands.md#cmd-001--set-target-temperature)). Notifies live as the device heats or cools, converging on the target value when reached. Correlated against the device's own display across multiple independent heating and cooling cycles, in both Celsius and Fahrenheit display modes. Matches the app's displayed current temperature, converted from the app's Fahrenheit display where applicable. It moves independently of `0x004c` (target): the two are separate values, and neither notifies as a side effect of the other changing.

  Two further properties of the reported value:
  - **Resolution is one whole degree Celsius.** Every value observed is an exact multiple of 1.0 °C, without exception, in both display-unit modes. While cooling the value steps down in exact 1 °C decrements; while heating it can jump several degrees between notifications, so the notification rate rather than the resolution is what limits it there.
  - **Reporting is gated on heater state below 40 °C.** With the heater on, the true value is always reported, including below 40 °C. With the heater off, the value is reported down to 40.0 °C and reads `0` below that. Switching the heater off while the temperature is already under 40 °C flips the value to `0` in the same second, and switching it back on restores the true reading just as promptly — see [STATE-012](#state-012--sub-40-c-reporting-and-the-idle-display-state).
- **Interpretation**: The raw value is always Celsius-encoded (×10) regardless of the device's display unit setting — Fahrenheit is a display-layer conversion only, never reflected in the BLE value itself. The ×10 encoding therefore carries more precision than the device ever populates on the read side.
- **Confidence**: Confirmed
- **Implementation status**: Implemented (read-only) in `components/volcano/volcano.cpp`. Verified against real hardware, including the sub-40 °C gating behaviour — see [STATE-012](#state-012--sub-40-c-reporting-and-the-idle-display-state).

## STATE-008 — Status/flags register (partial)

- **Handle**: `0x002b`
- **Observation**: The attribute is 4 bytes; the upper two are zero in every value observed, so it behaves as a 16-bit little-endian register in the low half, matching two independent third-party sources' description. Bit 5 (`0x0020`) is set whenever the heater is on, clear when off. On top of that heater-on baseline:
  - A momentary pulse adding bit `0x2000`, lasting about one second, when the vibration motor fires.
  - A sustained addition of bits `0x2000` and `0x1000` for as long as the pump is running, reverting immediately on pump-off.
  - Bit 10 (`0x0400`) clears while the device is heating toward a target meaningfully above the current temperature, and sets again as that target is approached. What it responds to is the gap between current temperature and target, not the size of the change: the boundary lies between 2 and 3 °C. Setting a target that leaves a gap of 2 °C or less leaves bit 10 set and produces no register activity at all; a gap of 3 °C or more clears it within a second, setting again as the new target is approached. Each side of that boundary has been observed across separate heating cycles.

    The set edge follows no fixed rule. The gap remaining when bit 10 sets again grows with the size of the climb — zero for a 3 °C rise, around 4 °C for a 12 °C rise, and settling at 5–6 °C for rises of 19 °C and above. The delay from clear to set grows too but far more slowly, spanning roughly 9 to 17 seconds. Neither the remaining gap, the elapsed delay, nor the implied time to arrival is constant, so the bit cannot be read as a "near target" indicator against any fixed threshold.

    The delays are not continuously distributed: they fall close to multiples of about 0.98 seconds from the shortest observed.
  - The bit `0x2000` pulse lands on the second the current temperature first equals the target, but it is the vibration event rather than a target-reached signal. It is absent when vibration is disabled (see the isolation note below), and it only fires on a target change large enough to have cleared bit 10 — runs of 1 °C and 2 °C steps produce no pulse and no vibration. It cannot be used to detect target-reach.
  - Bit 9 (`0x0200`) is clear at heater-on and sets at the same instant bit 10 first sets. It then stays set for the rest of the heater-on period, including across later target changes that clear bit 10 again, and returns to clear only when the register goes to all-zero at heater-off. It behaves as a latch — "the target has been reached at least once since the heater was switched on" — rather than as a flag that tracks a live condition.

  Decode summary:

  | Bit | Mask | Meaning | Confidence |
  |---|---|---|---|
  | 0, 1 | `0x0003` | Set whenever the heater is on, clear whenever it is off; never seen to vary independently of bit 5 | Confirmed (correlation); Unknown (meaning) |
  | 3, 4, 14 | `0x4018` | Error mask, per two third-party sources and the official client; no value with any of these bits set has ever been seen | Unknown |
  | 5 | `0x0020` | **Heater on** | Confirmed |
  | 9 | `0x0200` | Latches on the first target-reach of a heater-on period, clears at heater-off | Confirmed (behaviour); Unknown (purpose) |
  | 10 | `0x0400` | Clear while climbing to a target 3 °C or more above current; set otherwise | Confirmed (clear edge); Unknown (set edge) |
  | 12 | `0x1000` | **Pump running** | Confirmed |
  | 13 | `0x2000` | Set for the duration of a pump run, and pulsed for about a second by the vibration alert | Confirmed (behaviour); Probable (that it marks a motor being active) |

  These were isolated rather than merely correlated: the vibration bit appears when the target is reached with vibration enabled and is absent with it disabled, the pump bits track the pump alone with heater state held constant, and the bit 9 and bit 10 transitions were followed across several separate target changes, in both directions, and across separate heater-on periods.

  The heater bits and the pump bits are fully independent, which running the pump with the heater off makes explicit: that produces `0x3000` — bits 12 and 13 alone, with bits 0, 1 and 5 clear — and returns to all-zero on pump-off. Bits 0 and 1 are set in every value observed while the heater is on and clear in every value observed while it is off, so they track heater state alongside bit 5 rather than carrying independent meaning. Distinct values observed (low half): `0x0000`, `0x0023`, `0x0223`, `0x0623`, `0x2223`, `0x2623`, `0x3000`, `0x3023`, `0x3623`. The `0x2223` case is the vibration pulse firing while bit 10 is still clear, with bit 9 already latched from an earlier target-reach in the same heater-on period.

  The register reports state changes whatever caused them. Pump-on, pump-off and heater-on performed at the device's own control panel, with no corresponding write anywhere in the traffic, each produced the same register notification as the equivalent command would have — `0x3000` for a panel pump-on with the heater off, `0x0023` for a panel heater-on — with nothing in the value distinguishing origin. Observed across separate panel actions in both directions. The same holds for the target characteristic ([STATE-013](#state-013--target-temperature-notifications)), though not symmetrically: there, panel changes notify and the client's own writes do not.
- **Interpretation**: A connected client observes activity it did not initiate, since nothing in the register distinguishes a panel action from a commanded one.

  **To detect the pump, test bit 12, not bit 13.** Bit 13 is set during a pump run but is also pulsed by every vibration alert, so a client testing bit 13 alone will report the pump running each time the device signals that it has reached temperature.

  The spacing of bit 10's set-edge delays suggests the register is evaluated on a cycle of roughly a second rather than continuously, which would also explain why the delays cluster rather than spreading smoothly.

  No bit in this register is a reliable target-reached signal. Bit 10 sets too early and by a margin that varies, bit 9 latches and never re-arms within a heating cycle, and the `0x2000` pulse depends on a user setting. A controller wanting target-reach should compare current temperature against target ([STATE-007](#state-007--current-actual-temperature), [CMD-001](commands.md#cmd-001--set-target-temperature)) rather than watch this register.

  Bit `0x2000` is not uniquely "vibration" — it's set during both vibration and pump activity, so it more likely represents something like "an auxiliary motor is active," with bit `0x1000` specifically distinguishing pump from vibration. Two independent third-party sources name bit `0x2000` "Air Pump On" without mentioning the vibration overlap — plausibly an incomplete third-party description rather than a contradiction.
- **Confidence**: per-bit levels are given in the decode table above. Overall: Confirmed (the heater, pump and vibration bit behaviours; bit 9's latch; bit 10's clear edge; that the `0x2000` pulse is conditional on the vibration setting; and that panel-originated changes notify indistinguishably from commanded ones); Probable (that bit 13 marks a motor being active rather than the pump specifically, and that the register is evaluated on a roughly one-second cycle — the latter inferred from the distribution of bit 10's set-edge delays rather than measured directly); Unknown (what bits 0 and 1 mean, what bit 9's latch is used for, what governs bit 10's set edge, and the error mask)
- **Note**: the official client treats this characteristic as a register it calls `PRJSTAT1`, and names three of its bits: bit 5 `HEIZUNG_ENA` (heater enable), bit 9 `ENABLE_AUTOBLESHUTDOWN` (automatic BLE shutdown enable), and bit 13 `PUMPE_FET_ENABLE` (pump FET enable). It also defines an error mask of `0x4018` (bits 3, 4 and 14), matching the value two third-party sources give. Bits 5 and 13 agree with what was observed here. The bit 13 name also explains the vibration/pump overlap noted above: it drives a motor FET rather than the pump specifically. Bit 9 does **not** agree: it changes as a consequence of heating, latching on the first target-reach and clearing at heater-off, with no settings change involved. Nor does it track the auto-shutoff countdown: that arms on any actuator starting, including a pump-on with the heater off, whereas bit 9 requires a heating cycle and sets only as the target is approached ([STATE-005](#state-005--auto-shutoff-countdown)). The observed behaviour is recorded above; the client's name for the bit is not adopted. Bits 0, 1, 10 and 12 are not named by the client.
- **Implementation status**: Implemented (read-only), for bit 5 (heater) and bit 12 (pump) only, in `components/volcano/volcano.cpp`. Verified against real hardware, including that panel-originated toggles are reported indistinguishably from commanded ones. The remaining bits (0, 1, 9, 10, 13, and the error mask) are not implemented.

## STATE-009 — Temperature-step pulse on display/units register

- **Handle**: `0x002e`
- **Observation**: Bit 13 (`0x2000`) of the 4-byte value notified on this handle — distinct from the Display on Cooling setting bit ([CMD-005](commands.md#cmd-005--set-display-on-cooling)) and the Celsius/Fahrenheit units bit ([STATE-010](#state-010--temperature-display-units-bit)) — has both a resting state and a pulse.

  **Pulse.** The bit rests set and drops clear for about a second at each 1 °C change of the current temperature ([STATE-007](#state-007--current-actual-temperature)), then returns set. It is governed by the *rate* of change rather than its direction. Steps more than three seconds apart almost always carry a pulse; steps closer together than about 1.5 seconds almost never do; the proportion climbs steadily in between. Direction has no bearing once spacing is accounted for — falling steps nearly all pulse only because cooling is slow, and stepping a target up 1 °C at a time while at temperature produces pulses on the way up exactly as a cool-down does.

  **Continuous clear while climbing.** During a heat-up from cold, where current temperature moves several degrees per second, the bit is simply clear for the whole climb and settles set as the temperature stops moving. A 28-minute hold at target recorded no transition at all. It is also clear, with no pulses, while the device is showing no temperature ([STATE-012](#state-012--sub-40-c-reporting-and-the-idle-display-state)).

  The bit is independent of the others in the register: all of this was observed with the units bit both set and clear. It is on register `0x002e` and is unrelated to the same-numbered bit 13 on register `0x002b`, which marks motor activity ([STATE-008](#state-008--statusflags-register-partial), where that reading is Probable) — the two are easily confused, since both are `0x2000` and both move around the moment the target is reached.

  This bit and bit 10 of `0x002b` are not the same signal, despite both appearing to mark "climbing" versus "settled". Bit 10 responds to the size of the gap between current and target and ignores a 1 °C rise entirely; this bit responds to the rate at which the current temperature is moving and pulses on a 1 °C rise like any other. A 3 °C target rise clears bit 10 while this bit keeps pulsing throughout the climb.
- **Interpretation**: One mechanism rather than two: a pulse per degree of change which merges into a continuous clear whenever the temperature moves faster than the pulse can complete. That accounts for the apparent asymmetry between heating and cooling, which is just their different rates. It does not fully account for the thresholds, though — a pulse of about a second should still resolve at 1.5-second spacing, and it largely does not — so either the pulse is longer than it appears or something beyond spacing governs whether it resolves. What the pulse is *for* is still unexplained: nothing visible on the device changes in step with it.
- **Confidence**: Confirmed (that the pulse occurs, that it accompanies a temperature change in either direction, and that its resolution depends on the rate of change rather than the direction); Unknown (what the pulse is for, and why the spacing thresholds do not match a pulse of the length observed — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Not implemented

## STATE-010 — Temperature display units bit

- **Handle**: `0x002e`
- **Observation**: Bit 9 (`0x0200`) of the 4-byte value notified on this handle (the same register as [CMD-005](commands.md#cmd-005--set-display-on-cooling)) tracks the device's Celsius/Fahrenheit display setting: **set = Fahrenheit, clear = Celsius**. The setting is changed using the device's own simultaneous `+`/`-` panel gesture; the app exposes no control for it. Reproduced across multiple sessions, and agrees with a third-party source. The official client calls this register `PRJSTAT2` and names this bit `FAHRENHEIT_ENA`, treating it with the same polarity: the bit clear selects Celsius, set selects Fahrenheit. This bit is on register `0x002e` and is unrelated to the same-numbered bit on register `0x002b` described in [STATE-008](#state-008--statusflags-register-partial).
- **Interpretation**: The unit setting is a display-layer property only — temperature values on the wire are always Celsius-encoded regardless of it, per [STATE-007](#state-007--current-actual-temperature).
- **Confidence**: Confirmed (polarity); Unknown (whether the bit can be written to change units remotely — never attempted)
- **Implementation status**: Not implemented

## STATE-011 — Auto-shutoff behaviour

- **Handles**: `0x0054`, `0x002b`
- **Observation**: When the auto-shutoff countdown ([STATE-005](#state-005--auto-shutoff-countdown)) reaches `0`, the status/flags register ([STATE-008](#state-008--statusflags-register-partial)) goes to all-zero about a second later — the same value it takes when the actuators are switched off by command. Reproduced across many separate sessions and at different target temperatures. The delay from the final `0` on the countdown to the register clearing is consistently under two seconds, and varies within that range from one expiry to the next with no relationship to what was running.

  **Expiry stops the pump as well as the heater.** Where an expiry landed with both actuators running, the register went straight from `0x3623` to all-zero, ending pump runs of markedly different lengths. In each, the countdown that expired had been armed by an earlier heater-on, so the pump was stopped by a timer it did not itself arm, and its own running time had no bearing on when the stop came.

  Nothing else about the connection changes: the link is not dropped, and notifications continue afterwards on every subscribed characteristic whose value is still moving. Current temperature falls steadily as the device cools, and the display/units register resumes its per-step pulse per [STATE-009](#state-009--temperature-step-pulse-on-displayunits-register). The device remains powered with its display active, showing the current temperature, and the heater-runtime counters stop advancing ([STATE-006](#state-006--minutes-of-operation)). Where an expiry landed with the heater running and the pump off, the device went on notifying for over an hour afterwards without the link dropping.
- **Interpretation**: Auto-shutoff switches off every actuator that is running, not the heater alone. It is neither a device power-down nor a BLE teardown, so a controller does not need a reconnect loop to survive one. It is detectable as the status register going to zero with no command having been issued — indistinguishable in the register alone from a heater-off issued elsewhere, but distinguishable by the countdown having reached `0` rather than being reset from a non-zero value. Much later, once the device has cooled to 40 °C, it stops reporting a temperature and blanks the temperature readout; that is a consequence of cooling rather than a second timeout, and it also leaves the link up — see [STATE-012](#state-012--sub-40-c-reporting-and-the-idle-display-state).
- **Confidence**: Confirmed (that the countdown reaching zero switches off the heater and any running pump, and that the link and the device's powered state are unaffected)
- **Implementation status**: Not implemented

## STATE-012 — Sub-40 °C reporting and the idle display state

- **Handles**: `0x0047`, `0x002e`
- **Observation**: Current temperature ([STATE-007](#state-007--current-actual-temperature)) is reported below 40 °C only while the heater is on. The rule is a live gate on heater state, not a timeout:
  - With the heater **on**, the true value is reported at any temperature. Heater-on from a cold device reports the true temperature within a second of the write — values as low as 32.0 °C — and rises from there as from any other starting point. Reproduced across separate heater-on events.
  - With the heater **off**, the value is reported down to exactly 40.0 °C and reads `0` below that. On a passive cool-down it steps down in 1 °C decrements to 40.0 °C, never reports 39 °C, and notifies `0` one step-interval later.
  - The gate follows the heater immediately in both directions, well inside a second. A heater-off issued at 36 °C flipped the value to `0` 0.18 seconds later; heater-on from a blanked device returned a true reading 0.2 seconds after the write. Reproduced across several heater toggles below 40 °C.
  - Running the **pump** with the heater off does not lift the gate: during pump-only runs at around 34 °C the value stayed `0` and did not notify.

  Reproduced on the cooling path across separate sessions, with 40.0 °C the last non-zero value every time, and separately on the heater-on path.

  **The downward crossing stops a running pump.** With the heater off, the pump running and the temperature falling through 40 °C, the following happen together, with no command issued: current temperature notifies `0`, the status/flags register goes to all-zero — stopping the pump — bit 13 of the display/units register goes clear and stays clear, and the auto-shutoff countdown resets to `0`. The temperature notification comes first and the rest follow within about a fifth of a second, the register and display landing within 2 milliseconds of each other and the countdown some 65 milliseconds behind them.

  Reproduced on separate crossings. The interrupted runs differed in length by more than a third, so the stop tracks the temperature rather than any elapsed time.

  It is the crossing that stops the pump, not the state and not the pump's own running time:
  - A pump run started while the device was *already* below 40 °C ran for 29 seconds until switched off by command. Being below 40 °C neither prevents pumping nor stops it.
  - A pump run held below 40 °C ran for **4 minutes 16 seconds** and stopped only when commanded. There is no pump run-time limit anywhere near the duration of the interrupted runs.

  While the value reads `0` the device is in a matching display state. The physical display drops the temperature readout while retaining its Bluetooth and power indicators, and the official app shows a connected device reading 0 °C. Bit 13 of `0x002e` ([STATE-009](#state-009--temperature-step-pulse-on-displayunits-register)) goes clear for the transition to `0` and stays clear rather than returning set. Whether the status/flags register ([STATE-008](#state-008--statusflags-register-partial)) marks the transition depends on what was running: with no actuator active it is already all-zero and does not change, but with the pump running it goes from `0x3000` to all-zero as the pump is stopped.

  **The device stays connected and fully responsive in this state.** The device never drops the link: a connection has been held unbroken for over four hours past the transition, and across more than one transition in a single session, with every disconnection in evidence originating from the client rather than the device. Nothing notifies while nothing is moving — across those four hours not a single notification arrived on any subscribed characteristic — but commands issued into that silence take effect:
  - Target-temperature writes are accepted with no visible change at the device. A target written while the display was blank governed the next heating cycle, which stopped at exactly that value.
  - Pump-on runs the pump, notifies the pump bits on the status register, and adds an air icon to the otherwise blank display.
  - Heater-on starts heating and restores the full display and notification behaviour immediately.
- **Interpretation**: `0` on `0x0047` means "no reading available" rather than "0 °C". The device suppresses the readout below 40 °C — the same value as the lowest target the app will set ([CMD-001](commands.md#cmd-001--set-target-temperature)) — whenever it is not heating, and drives its display from the same rule, so a blank display is a consequence of temperature and heater state rather than an inactivity timeout or a power-saving mode. Nothing about the BLE interface changes with it.

  The crossing is a single coordinated "return to idle" event rather than several coincidences: the display blanks and any running actuator stops, both off the back of the temperature notification and within milliseconds of each other. The countdown reset follows from the actuator stopping rather than being a separate action — it resets whenever the device returns to fully idle, by whatever route ([STATE-005](#state-005--auto-shutoff-countdown)).

  Four consequences for a controller: `0` on `0x0047` must never be treated as a temperature; a silent connection must not be treated as a dead one; no wake-up or reconnect step is needed before commanding a device whose display has gone blank; and a pump run started on a cooling device will be cut short without any command when the temperature crosses 40 °C, so a controller must not assume the pump is still running merely because it never switched it off.
- **Confidence**: Confirmed (the heater-gated 40 °C threshold in both directions, that the pump does not lift it, the accompanying display state, that the link is not dropped, that target, pump and heater commands all take effect in this state, and that a downward crossing stops a running pump — reproduced, with a pump run started below 40 °C ruling out the state as the cause and a 4-minute run ruling out a run-time limit)
- **Implementation status**: Implemented (the `0` → "no reading" gate on current temperature, in both directions) in `components/volcano/volcano.cpp`. Verified against real hardware, reproduced multiple times. The pump- and display-related aspects of this finding are not implemented, since this component does not yet read the pump or display state in this context.

## STATE-013 — Target temperature notifications

- **Handle**: `0x004c`
- **Observation**: The target temperature characteristic ([CMD-001](commands.md#cmd-001--set-target-temperature)) notifies when the target is changed at the device's own control panel, in the same 4-byte ×10 Celsius encoding used for writes. Holding a panel button produces a stream of notifications carrying each intermediate value, roughly one per second, with the step size accelerating the longer the button is held, from around 2 °C per notification at the start of a hold to around 10 °C after a few seconds. The final notification carries the value the target settles on.

  Writes issued by the connected client are almost never echoed: hardly any write to this handle is followed by a notification, and none has been followed by a notification carrying the value just written. There is one exception, and it matters. A write is sometimes answered within a fraction of a second by a notification carrying the *previous* target rather than the one just written, and that write then has no effect — the device neither heats toward the new value nor reports it — until the client sends it again. Observed in separate sessions, each time on a write following a run of writes a few seconds apart, each time with the temperature only moving after the re-send, and each time with the value notified back being a target the current temperature had just reached. It is intermittent: the same write spacing does not reliably reproduce it.
- **Interpretation**: The device reports target changes it originates itself, which lets a controller stay in step with the panel rather than assuming it is the only thing setting the target. Since a held button emits every intermediate value, a controller should treat the stream as a live drag and act on where it settles rather than on each value in turn. Whether the absence of an echo on client writes is a general property or specific to the writing client is undetermined: no session has ever had a second client connected to compare against.
- **Confidence**: Confirmed (that the characteristic notifies on device-side target changes, the encoding, and the intermediate-value stream); Unknown (why some writes are answered with the previously-set target, and whether a write can be silently dropped — see [`open-questions.md`](open-questions.md))
- **Implementation status**: Implemented (this finding's notify/read side) in `components/volcano/volcano.cpp`. Verified against real hardware, including that panel-driven target changes are reported indistinguishably from commanded ones, matching [STATE-008](#state-008--statusflags-register-partial)'s finding for the status register. The write side is covered separately by [CMD-001](commands.md#cmd-001--set-target-temperature).
