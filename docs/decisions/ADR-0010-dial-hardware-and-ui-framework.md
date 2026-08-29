# ADR-0010: Dial Hardware and UI Framework

## Status

Accepted

## Context

[ADR-0001](ADR-0001-project-vision.md) names Phase 2 as porting the firmware to the M5Stack Dial and adding a local UI. [ADR-0004](ADR-0004-development-hardware-strategy.md) established *why* Phase 1 develops on a plain dev board rather than the Dial, and named the Dial as the Phase 2 target, but deliberately left the Dial's own hardware bring-up until Phase 2 begins. [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) selected ESPHome as the firmware framework "subject to verifying that ESPHome provides sufficient support for the M5Stack Dial hardware requirements" — that verification has not yet happened. This ADR is where Phase 2 picks that up: what renders the UI, and how the Dial's specific hardware — display, touch, rotary encoder, physical button — is wired into an ESPHome configuration.

This ADR does not touch the Volcano component. [ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md) already made `VolcanoDevice` hardware-independent specifically so a control interface's hardware could be built and iterated on without it; this decision is entirely about the plumbing a new control interface — the Dial local UI — needs underneath it, per [ADR-0002](ADR-0002-volcano-component-architecture.md)'s boundary. What that control interface actually shows and how it is navigated is [ADR-0011](ADR-0011-dial-ui-navigation-architecture.md)'s concern, not this one.

The Dial's relevant hardware: a 1.28", 240×240 round TFT (GC9A01 driver, SPI) with a capacitive touch panel (FT3267, I²C); a rotary encoder that reports turn direction and position but has no push action of its own; a separate physical button beneath the screen, felt by a user as "pressing the dial" even though it is electrically independent of the encoder; and an onboard PWM-driven buzzer, the Dial's only audio output. All of this needs to coexist with the WiFi/`api`/`web_server` stack Phase 1 already built and hardware-verified for the dev board example.

## Decision

**LVGL is the UI rendering framework**, via ESPHome's `lvgl` component. The page-based, touch-and-encoder-driven navigation [ADR-0011](ADR-0011-dial-ui-navigation-architecture.md) defines needs a widget/event system — screens, buttons, labels, hit-testing, input group focus for the encoder — that LVGL already provides; ESPHome configurations targeting this exact board combination (GC9A01 + FT3267 + a quadrature encoder) already exist in the wider ESPHome community, which is corroborating evidence that the combination is workable, not a claim that this project's exact configuration is verified yet.

**The display, touch panel, and rotary encoder are wired up through ESPHome's own native components** — a `display` platform for the GC9A01, a `touchscreen` platform for the FT3267, and a `rotary_encoder` platform for the encoder's quadrature signal — rather than a bespoke driver. The exact platform identifiers and pin assignments are implementation detail, confirmed when Dial hardware is in hand and `esphome compile` first succeeds against it, not fixed by this ADR.

**The physical button is a separate input from the rotary encoder**, wired as its own discrete input (e.g. a `binary_sensor` on its GPIO), not modelled as an "encoder click." This corrects an assumption this project has not yet stated explicitly but was at risk of carrying forward from ADR-0001's "adding rotary encoder input" phrasing: the encoder only ever reports turns; every press action in [ADR-0011](ADR-0011-dial-ui-navigation-architecture.md)'s navigation model comes from this separate button.

**`wifi`, `api`, and `web_server` are retained on the Dial firmware, unchanged from the dev-board example.** (In the firmware as built, only `wifi` and `web_server` are retained; neither example has ever declared `api` — see this ADR's Notes.) [ADR-0001](ADR-0001-project-vision.md) requires the Dial to fully control the Volcano *with no external dependency* — that constrains what the Dial must be able to do without WiFi or Home Assistant present, not whether WiFi may exist on the device at all. Phase 2 is additive: the Dial local UI becomes a third live control interface alongside the two Phase 1 already built (the `web_server` page and any Home Assistant/direct-API consumer of the ESPHome `api`), not a replacement for either.

**DSEG7, a digital/seven-segment-style font released under the SIL Open Font License, is embedded for numeric display** — chosen so the Dial's temperature and value readouts can visually echo the Volcano's own display, and embeddable in firmware without a licensing concern. Exact colour values, widget styling, and pixel layout are left to implementation and code, not fixed here.

**The onboard buzzer is wired up through ESPHome's own PWM output and melody-playback components**, the same "reuse ESPHome's component model" reasoning as the display/touch/encoder above, rather than a bespoke tone generator. It provides the audio feedback [ADR-0011](ADR-0011-dial-ui-navigation-architecture.md) defines. Exact pin assignment and component configuration are implementation detail, confirmed when compiled and verified against real hardware, not fixed by this ADR.

## Consequences

**Benefits**

- Reuses ESPHome's own component model for every piece of new hardware (display, touch, encoder, button, buzzer), consistent with ADR-0003's reasoning for choosing ESPHome in the first place — no bespoke ESP-IDF driver stack to build and maintain alongside it.
- LVGL's widget/event system gives ADR-0011's page-and-navigation model a foundation to build on rather than requiring bespoke hit-testing, focus handling, and screen management to be written from scratch.
- Because `VolcanoDevice` already has zero hardware dependency (ADR-0009), this entire hardware/framework layer can be developed and iterated on independently of BLE work, the same separation of concerns ADR-0004 established between Phase 1 and Phase 2 hardware.
- Retaining `wifi`/`api`/`web_server` means Phase 1's hardware-verified manual test surface — the `web_server` page — keeps working unchanged; nothing about Phase 2 regresses it or requires re-validating it from scratch.

**Trade-offs**

- ESPHome's native support for this exact display/touch/encoder combination is not verified as of this decision, only plausible from community precedent. First Dial bring-up must confirm each driver actually compiles and functions before any UI work proceeds — the same "verify the foundation before building on it" discipline ADR-0004 applied to Phase 1's BLE work.
- LVGL is a substantial dependency with its own build-time and learning-curve cost compared to ESPHome's simpler non-LVGL display primitives. Justified here because ADR-0011's page-based navigation is exactly the case LVGL exists for.
- Retaining `wifi`/`api`/`web_server` means the Dial firmware carries every Phase 1 dependency in addition to the new display/touch/LVGL stack — a larger, more complex firmware image with more surface to validate than a BLE-and-display-only build would have.

## Alternatives considered

**1. Bespoke, non-LVGL rendering**

Use ESPHome's plain `display` component with manual drawing calls instead of LVGL. Rejected because page-based navigation with touch buttons and rotary-driven focus is exactly the problem LVGL solves; hand-rolling equivalent hit-testing and screen/state management would duplicate a mature library for no benefit.

**2. A custom driver stack outside ESPHome's component model**

Write the Dial's display/touch/encoder handling as bespoke ESP-IDF code rather than ESPHome components. Rejected for the same reason ADR-0003 rejected bespoke firmware generally: it forfeits ESPHome's config-driven, reusable external-component model that the Volcano component itself already depends on.

**3. Model the encoder's press as part of the encoder**

Treat the physical button as though it were the encoder's own click, matching how some other rotary-input hardware works. Rejected because it does not match this hardware: the M5Dial's encoder and its button are electrically and logically separate, and modelling them as one would misrepresent the hardware to every later reader of the configuration.

**4. Drop `wifi`/`api`/`web_server` from the Dial, BLE-and-local-UI only**

Considered given ADR-0001's "no external dependency" framing for Phase 2. Rejected: that requirement governs what the Dial must be able to do *without* WiFi, not whether WiFi may be present at all, and dropping it would discard the working `web_server` test surface for no corresponding benefit.

## Notes

- Reference [ADR-0001](ADR-0001-project-vision.md) for the phase this decision executes, [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) for the ESPHome-hardware-support question this ADR partially resolves, [ADR-0004](ADR-0004-development-hardware-strategy.md) for the hardware-strategy sequencing this continues, and [ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md) for the `VolcanoDevice` interface this hardware stack exists to serve.
- See [ADR-0011](ADR-0011-dial-ui-navigation-architecture.md) for what is actually shown and how it is navigated — this ADR fixes only the hardware and rendering foundation underneath that.
- The Context's and Decision's references to `api` being part of an "already built and hardware-verified" Phase 1 stack, and to `api` being "retained on the Dial firmware, unchanged from the dev-board example," are inaccurate: neither the dev-board example nor the Dial example has ever declared an `api:` block. Home Assistant/direct-API integration is Phase 3, not yet started — see [ADR-0001](ADR-0001-project-vision.md)'s phase list and [`docs/DEVELOPMENT.md`](../DEVELOPMENT.md#current-phase). `wifi` and `web_server` are retained on the Dial firmware exactly as this ADR states; `api` is not. Left as an inaccuracy in the Decision/Context text itself rather than rewritten, per the [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) Notes precedent.
