# ADR-0011: Dial UI Navigation Architecture

## Status

Accepted

## Context

[ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md) settles the Dial's hardware and rendering foundation: LVGL, a 240×240 round touchscreen, a rotary encoder that only ever reports turns, and a separate physical button felt as "pressing the dial." This ADR is about what the Dial local UI — the control interface [ADR-0001](ADR-0001-project-vision.md) names as one of three consumers of the Volcano abstraction layer — actually shows and how it is navigated.

[ADR-0002](ADR-0002-volcano-component-architecture.md) requires a control interface to consume `VolcanoDevice`'s interface only and to own every presentation concern itself; [ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md) explicitly defers "how the Phase 2 Dial UI renders any of this, including how it presents an unknown value or an outstanding request" to this document. Two constraints from ADR-0009's own design bear directly on navigation:

**`VolcanoDevice` refuses a second write to a field while a previous write to that same field is still outstanding**, and a confirmed round trip is on the order of 150–250ms on real hardware — far slower than a rotary encoder can generate turn events. A UI that writes on every encoder detent will have most of those writes silently dropped.

**Every field is already unknown, and every `set_*()` call already a no-op, while `connection_state()` is not `READY`.** The existing `web_server` page relies on exactly this: its `connected` binary sensor is "the one control worth trusting" during a disconnect, while every switch simply holds its last value rather than resetting.

The screen itself is small: 1.28" and round, so content near the corners of a naive square layout is clipped, and there are exactly two physical inputs beyond touch — a rotary turn and a single button press.

## Decision

### Page model

The UI is a fixed set of full-screen pages: **Home**, a **Navigation Menu**, **Settings**, three instances of a shared **numeric value page** (LED Brightness, Auto-Shutoff Duration, Dial Brightness), a **Dial Sound** toggle page, and **Information**. Each page optionally uses touch buttons and optionally the rotary encoder; none of them share screen space with another.

### Input semantics

- **Rotary turn** — page-specific: adjusts a value on Home and the numeric pages, moves the highlighted selection on the Navigation Menu, does nothing on Settings or Information.
- **Physical button press** — context-dependent but consistent per page: confirms the highlighted item on the Navigation Menu; returns to the Navigation Menu from every other page.
- **Touch** — page-specific; the Navigation Menu's items are also directly selectable by touch, not only by rotary-and-button.

### Home page

- Current temperature and target temperature, rendered in DSEG7 (ADR-0010) and coloured to echo the Volcano's own display — current in orange, target in cyan — against a dark theme. Exact colour values and layout are implementation detail, not fixed here.
- Touch buttons for the heater and pump.
- A small auto-shutoff countdown readout.
- Small BLE and WiFi connection-state icons: steady colour for connected, flashing for connecting, a strikethrough variant for disconnected, using the same orange/cyan-family colour convention. These are read-only indicators, not touch targets, so the touch-sizing rule below does not apply to them.
- Rotary turn adjusts target temperature, subject to the write-coalescing rule below.
- A button press opens the Navigation Menu.

### Navigation Menu

Lists every other page. Rotary turn moves the highlighted selection; a button press or a direct touch on an item opens that page.

### Settings page

Touch toggle buttons for vibration, display-on-cooling, and display units (Celsius/Fahrenheit) — one full-width row each, mirroring `VolcanoDevice`'s existing boolean fields. A button press returns to the Navigation Menu.

### Numeric value page (shared pattern)

One page template, reused for three values that share an identical interaction shape — a value shown large, adjusted by the rotary encoder, a button press returning to the Navigation Menu:

- **LED Brightness** — 0–100%, a `VolcanoDevice` field.
- **Auto-Shutoff Duration** — shown in minutes, within the confirmed 1–360 minute range, a `VolcanoDevice` field.
- **Dial Brightness** — 0–100%, controlling the Dial's own backlight. This is *not* a `VolcanoDevice` field: it is local Dial hardware configuration, read and written directly against ESPHome's own facilities and persisted so it survives a reboot. Reusing the numeric-page template here is a UI convenience; it is not a claim that this value flows through the Volcano abstraction layer, and it must not be implemented as though it did — see [ADR-0002](ADR-0002-volcano-component-architecture.md)'s boundary.

### Audio feedback

Every rotary turn plays a short click, and every button press — physical or a touch on a button/switch/menu row — plays a short beep, via the Dial's onboard buzzer ([ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md)). This applies uniformly across every page, independent of whatever else a given turn or press does there — it is feedback that input was registered, not a page-specific behaviour.

A dedicated **Dial Sound** page mutes it: a single on/off toggle, not a graduated volume — the Dial's onboard buzzer has no true amplitude control (it is a fixed PWM-driven piezo buzzer, not a speaker with a real volume range), so a numeric percentage would misrepresent what is actually adjustable. Same touch-toggle pattern as Settings, standalone on its own page rather than folded into Settings since it configures the Dial's own UI rather than a `VolcanoDevice` field.

### Information page

- The five device-information strings: firmware version, BLE firmware version, serial number, power supply, product line.
- Hours and minutes of operation combined onto a single line (e.g. "Time of Operation: 1234h 56m") rather than as two separate rows, to save space.
- A button press returns to the Navigation Menu.

### Write coalescing for rotary-adjusted values

Every rotary-adjusted `VolcanoDevice` field — target temperature, LED brightness, auto-shutoff duration — debounces locally: the displayed value updates immediately on every turn, but the actual `set_*()` call fires only once turning has paused, not once per detent. This keeps the UI visually responsive without fighting `VolcanoDevice`'s single-outstanding-write rule. Dial Brightness follows the same debounce shape for consistency, even though it never reaches `VolcanoDevice` at all.

### Connection-state handling

This UI adds no blocking behaviour beyond what `VolcanoDevice` already provides: every field is already unknown, and every `set_*()` call already a no-op, while not `READY` (ADR-0009). Touch buttons and rotary-adjusted controls therefore remain live on every page regardless of connection state, exactly as the existing `web_server` page's switches already behave. The Home page's connection-state icons are the only additional treatment this ADR adds — a visible indicator, not an interactive gate.

### Touch target sizing

Every touchable element must be large enough to hit reliably with a fingertip and must sit within the display's visible round area, not merely within its square bounding box. Touch buttons and menu rows are full-width, one item per row; the Navigation Menu shows a handful of items at a time with scrolling rather than a dense list. Exact pixel dimensions are implementation detail.

## Consequences

**Benefits**

- One page template covers three distinct settings, so a further rotary-adjusted numeric value, if one is ever added, is a new page instance rather than new interaction code.
- "A button press returns to the Navigation Menu" is a single, consistent rule across every non-Home, non-Menu page, so no page needs its own bespoke way out.
- Debouncing rotary writes keeps the UI responsive without fighting `VolcanoDevice`'s single-outstanding-write rule, and applies uniformly rather than needing a special case per field.
- Connection-state icons give the same "do not trust a stale value" signal ADR-0009 established for the web page's `connected` sensor, without introducing a second, Dial-specific state machine to reason about.

**Trade-offs**

- Six distinct elements on the Home page (two temperatures, two buttons, a countdown, two status icons) on a small round screen is a real layout risk this ADR does not fully resolve; the precise arrangement needs validating against real hardware, not just decided on paper.
- The button's dual meaning — confirm on the Menu, back everywhere else — is simple as a rule but means the same physical action does different things depending on which page is showing, which needs to be learnable in practice; that is a presentation concern for implementation, not something this ADR can guarantee on its own.
- Leaving Dial Brightness's persistence and read/write path outside `VolcanoDevice` is a deliberate, narrow carve-out; this ADR does not formally establish a general pattern for where further local-only Dial settings would live beyond "not the Volcano component." Dial Sound, added later, followed the identical shape (a plain restore_value global, its own page), so a de facto pattern exists in practice even without being named as a rule here.

## Alternatives considered

**1. Confirm-then-write for rotary-adjusted values**

Change nothing on the device until the button explicitly confirms a new value; the rotary turn only edits a local, uncommitted value until then. Rejected: it adds an extra step to every ordinary adjustment — nudging the target temperature by a few degrees, say — to guard against a problem (writes fired mid-turn) that debouncing already solves by delaying the write past a turn still in progress.

**2. A full-screen "reconnecting" page blocking all input while not READY**

Rejected: `VolcanoDevice` and the existing `web_server` page already establish that a no-op write while disconnected is an acceptable, well-understood outcome. A blocking page would be new, Dial-specific behaviour introduced with no need identified for it.

**3. Disabling or greying individual controls while not READY**

A stricter alternative to the status-icon approach. Rejected in favour of matching the `web_server` page's existing precedent — indicator only, controls remain live — rather than introducing a second, Dial-specific connection-handling behaviour alongside the one ADR-0009 already defined.

**4. A bespoke page per numeric value, no shared template**

Separate, independently-implemented LED Brightness, Auto-Shutoff Duration, and Dial Brightness pages. Rejected once it was clear the three share an identical interaction shape; a shared template avoids three near-duplicate implementations of the same page.

## Notes

- Reference [ADR-0002](ADR-0002-volcano-component-architecture.md) for the control-interface boundary this architecture stays inside — in particular, Dial Brightness's and Dial Sound's deliberate exemption from `VolcanoDevice` — [ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md) for the connection-state and write-confirmation behaviour this UI reads rather than reimplements, and [ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md) for the hardware and rendering foundation this architecture is built on.
- This ADR fixes the page set and navigation model; it does not fix pixel layout, exact colours, or icon artwork, all of which are implementation detail expected to be refined once real Dial hardware is available to test against.
- The Decision's "Information page" section describes one page; the implementation splits it into two, **About** and **Diagnostics**, each its own Navigation Menu entry — a single "Label: value" line combining both read too plain on real hardware, per user feedback (see `examples/dial/info-pages.yaml`'s own comment). Left as a difference from the Decision text rather than rewritten, per the [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) Notes precedent; the page count elsewhere in this ADR's Page model should be read as nine pages, not eight.
