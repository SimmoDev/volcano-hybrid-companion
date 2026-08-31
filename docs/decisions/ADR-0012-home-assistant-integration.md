# ADR-0012: Home Assistant Integration

## Status

Accepted

## Context

[ADR-0001](ADR-0001-project-vision.md) names Phase 3 as Home Assistant integration through the ESPHome API, added as an additional, optional control interface, with one hard constraint: the device must keep controlling the Volcano when Home Assistant is unreachable. Phases 1 and 2 are complete and hardware-verified — the Volcano component and the M5Stack Dial's local UI both work with no external dependency.

[ADR-0003](ADR-0003-esphome-as-firmware-framework.md) already fixed the shape of this integration: ESPHome's native `api` is what Home Assistant connects to, Home Assistant is one consumer among the three control paths ADR-0001 defines with no privileged position, and enabling or disabling `api` must never change how the Volcano component behaves. It also anticipated that Phase 3 would be "largely 'enable the `api` component and expose entities'" rather than a bespoke integration protocol.

Much of that groundwork is already in place. The `volcano` component's entities carry Home-Assistant-appropriate metadata — device class, state class, unit of measurement, entity category — chosen in Phase 1 so state and commands are reachable from a `web_server` page or Home Assistant without further work. Every writable value is already a single two-way entity. What Phase 3 adds is the `api` component itself and the decisions that come with turning it on.

Those decisions are: which configurations gain `api`; how the API is secured; what the device does when Home Assistant is absent, unreachable, or disconnected — the ESPHome `api` component's default behaviour here is to reboot the device, which is in direct tension with ADR-0001's standalone requirement; which entities Home Assistant sees; whether the Dial surfaces the API connection on-screen; and how the standalone guarantee is verified before Phase 3 is called complete.

One correction carries in from [ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md). Its Decision text describes `wifi`, `api`, and `web_server` as retained on the Dial "unchanged from the dev-board example," but its own Notes record that no example configuration has ever declared an `api` block. This ADR is where `api` actually enters the project.

## Decision

**`api` is enabled on both example configurations** — the M5Stack Dial configuration and the ESP32-S3 dev-board configuration. The dev board gains it so the component's Home Assistant entity surface can be exercised against a real Home Assistant instance without Dial hardware, the same separation-of-hardware-concerns reasoning [ADR-0004](ADR-0004-development-hardware-strategy.md) applies to keeping the dev board available for BLE-only work. The Dial configuration carries it because that configuration becomes the project's shipped firmware when Phase 3 completes. The dev-board example is consequently no longer "no Home Assistant integration."

**The native API is secured with a pre-shared encryption key.** The key is supplied from `secrets.yaml`, alongside the BLE address and WiFi credentials already kept there, and never committed. This matches Home Assistant's standard ESPHome onboarding, which expects an encrypted API, and it is the appropriate bar for an interface that carries write access to the device — distinct from the `web_server` page, whose no-authentication trust model is a deliberately lower bar for a local manual-testing surface.

**The device never reboots or degrades because Home Assistant is absent, unreachable, or disconnected.** The ESPHome `api` component's default of rebooting the device when no API client has connected within a timeout is disabled. Loss of the API connection has no effect on BLE control, the Dial UI, or the `web_server` page. This is ADR-0001's standalone requirement expressed at the level of the `api` component's own configuration.

**No Volcano-domain behaviour is conditional on the API.** Consistent with ADR-0003: the state model, write handling, range checks, and connection lifecycle are identical whether the API is connected, disconnected, or compiled out. The API is a read/write view onto the same state model the Dial UI and `web_server` already read from.

**Home Assistant reaches the same controls the `web_server` page exposes, and no more.** The entity set is the one Phases 1 and 2 defined: the named `volcano` entities plus the Dial's own firmware-version diagnostic. Entities kept internal for the Dial UI's own use, and Dial-local settings that sit outside `VolcanoDevice` by [ADR-0002](ADR-0002-volcano-component-architecture.md)'s boundary — the Dial's backlight brightness and its sound toggle, per [ADR-0011](ADR-0011-dial-ui-navigation-architecture.md) — are not promoted to the API surface here. Whether any of those later gain a Home Assistant entity is a separate decision.

**The Dial's Connections page gains a Home Assistant status indicator**, alongside its existing BLE and WiFi rows. It shows whether an API client is currently connected, as an indicator only — ADR-0011's "a visible indicator, not an interactive gate" treatment of connection state, the same as the BLE and WiFi rows beside it. This extends ADR-0011's page set and is recorded in that ADR's Notes when built, as the Connections page itself was.

**Releasing the BLE connection from Home Assistant is not given a dedicated entity.** Handing the Volcano back to the official app is reachable from any API client through the `ble_client` component's own enable/disable, the same boundary [ADR-0007](ADR-0007-ble-connection-lifecycle.md)'s and [ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md)'s Notes draw for the Dial's Connections page: connection lifecycle is transport-level plumbing a control interface may reach directly, not a Volcano-domain concept the abstraction layer mediates.

**Phase 3 is complete only when the integration is verified against a real Home Assistant instance.** Entities appear in Home Assistant and are controllable from it; changes made at the device's panel, the Dial, the `web_server` page, and Home Assistant all reflect across the others; and — the gating check — the device continues to control the Volcano with Home Assistant stopped, with the wrong API key configured, and with WiFi absent entirely.

## Consequences

**Benefits**

- The three control paths ADR-0001 defines now converge on one shared state model, so a command behaves identically regardless of which path issues it — the end state ADR-0001 describes.
- Home Assistant support is configuration plus the entities that already exist, not a custom integration to build and maintain — the outcome ADR-0003 anticipated.
- The standalone guarantee becomes explicit at the one layer that could silently break it: the reboot-on-no-client behaviour disabled is a stated, reviewable decision rather than a default left unexamined.
- Home Assistant users get Fahrenheit where their locale asks for it at no cost, because the temperature entities already carry a temperature device class and Home Assistant converts them ([ADR-0008](ADR-0008-temperature-units-handling.md)).

**Trade-offs**

- Both configurations carry the `api` stack in addition to everything else. The Dial is already RAM-constrained — LVGL's buffer was reduced to fit BLE and `web_server` alongside it — so adding `api` needs a compile and a free-memory check on real Dial hardware, even though the API protocol itself is lean.
- Anyone flashing the firmware now needs one more secret, the API encryption key, on top of the BLE address and WiFi credentials.
- The dev-board example is no longer BLE-and-`web_server` only; its README section and its configuration header comment change to describe the API.

## Alternatives considered

**1. `api` on the Dial configuration only**

Enable the API only on the configuration that ships as firmware, leaving the dev board BLE-and-`web_server` only. Rejected because the dev board is the project's hardware-independent way to exercise component behaviour — the reasoning ADR-0004 uses for keeping it available for BLE work applies equally to validating the Home Assistant entity surface without needing Dial hardware in hand.

**2. An unencrypted API**

Run `api` with no encryption, matching the `web_server` page's no-authentication trust model. Rejected on two grounds: Home Assistant's ESPHome onboarding expects an encrypted API, and the API carries full write access to the device. The `web_server` page's lower bar is a deliberate choice for a local manual-testing surface, not the standard for the integration the project ships.

**3. Leave the reboot-on-no-client timeout at its default**

Accept the ESPHome default and rely on the device reconnecting quickly after a reboot. Rejected because it makes Home Assistant's availability load-bearing for the device staying up — the exact inversion of ADR-0001's requirement that the device keep working when Home Assistant is unreachable.

**4. A dedicated Home Assistant page on the Dial**

Give Home Assistant its own page in the Dial's navigation rather than a single row on the Connections page. Rejected because nothing about the API needs on-device interaction — there is no pairing step or setting to adjust from the Dial — so a status row beside the existing BLE and WiFi rows is the proportionate treatment.

## Notes

- Reference [ADR-0001](ADR-0001-project-vision.md) for the Phase 3 scope and the standalone requirement this decision is bound by, and [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) for the API-as-optional-consumer boundary it builds on.
- Reference [ADR-0007](ADR-0007-ble-connection-lifecycle.md) and [ADR-0009](ADR-0009-volcano-abstraction-layer-interface.md) for why releasing the BLE connection stays outside the Volcano abstraction layer, which this decision follows for the Home Assistant path as well.
- [ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md)'s Notes already record that its "`api` retained on the Dial" text was never accurate. This ADR does not amend ADR-0010; it is where `api` is actually introduced.
- [ADR-0008](ADR-0008-temperature-units-handling.md)'s expectation that Home Assistant performs its own Celsius/Fahrenheit conversion is already satisfied by the component's existing entity metadata; Phase 3 adds nothing for it to handle.
- The `firmware/` directory and the first versioned release, v1.0.0, still happen when Phase 3 completes, not at this ADR — unchanged from the existing project plan.
- The concrete `api` configuration — the keys beyond the encryption key and the reboot-timeout override, the cleanup of any named entity that should not reach Home Assistant, and the Connections-page indicator's wiring — is implementation detail for the increments that follow, not fixed here, the same way [ADR-0010](ADR-0010-dial-hardware-and-ui-framework.md) left its component and pin choices to implementation.
- The Decision's "the device never reboots because Home Assistant is unreachable" names the `api` component's own reboot-on-no-client timeout, but the same standalone requirement reaches the `wifi` component: its default is to reboot the device after a period with no WiFi association, and Home Assistant becomes unreachable precisely when WiFi is down. Both timeouts are therefore disabled where `api` is enabled — the same standalone decision applied consistently, not a new one.
- **Resolved: the verification the "Phase 3 is complete only when…" decision requires has been carried out.** On 2026-08-31, with the M5Stack Dial firmware against a real Home Assistant instance: the Volcano entities appeared in Home Assistant and were controllable from it; changes made at the Volcano's panel, on the Dial, and on the `web_server` page each reflected in Home Assistant, and Home Assistant's changes reflected back; the Dial's raw rotary-encoder and button inputs were confirmed absent from the entity list; and the gating check passed — with Home Assistant stopped, with the wrong encryption key configured, and with WiFi absent, the Dial kept full local control of the Volcano and did not reboot. The same session settled the Trade-offs section's free-memory question: with `api` running alongside BLE, LVGL and `web_server` on the RAM-constrained Dial, BLE stayed connected and every control kept working for the duration, with no allocation failure or instability. The `firmware/` directory and the v1.0.0 release named above remain before Phase 3 formally closes.
- **The verification above was performed on the Dial configuration only.** The dev-board configuration's `api` block is checked by `esphome config`/`esphome compile` but has not been exercised against a running Home Assistant instance. It is treated as covered by the Dial result rather than gated on its own: the two configurations declare an identical `api` block and the dev board's `volcano` entity set is a strict subset of the Dial's (it has no Dial-firmware-version entity), and the dev board is the less resource-constrained of the two, so the API path holds nothing there that the Dial run did not already exercise. A dev-board-plus-Home-Assistant check is still worth doing as a belt-and-braces confirmation, but it is not a Phase 3 closure condition.
