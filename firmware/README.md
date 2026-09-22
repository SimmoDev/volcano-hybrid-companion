# Firmware

The device firmware this project ships. One target so far: the M5Stack
Dial. The config under [`examples/`](../examples/README.md) is a compile
check and BLE-only test surface for the `volcano` component; this is the
configuration meant to be flashed to a device and used.

The firmware is feature-complete and versioned `1.0.0`, but the project
is not yet released. Every value a shared image cannot carry — the
Volcano's address, WiFi credentials, the Home Assistant API key — is now
supplied at runtime rather than compiled in, and the firmware carries an
over-the-air update path (see "Flashing and watching logs" below); what
remains of Phase 4 is packaging the firmware itself as a
browser-flashable download — see
[ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md). Until
that exists, flashing needs the ESPHome CLI, as below. See the root
[README.md](../README.md) for the phase history and
[ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md) for
the Home Assistant integration.

## `m5stack-dial.yaml`

Targets the M5Stack Dial ([ADR-0010](../docs/decisions/ADR-0010-dial-hardware-and-ui-framework.md),
[ADR-0011](../docs/decisions/ADR-0011-dial-ui-navigation-architecture.md)): a
local, standalone control surface — an LVGL touchscreen UI driven by the
Dial's rotary encoder, physical button and touch panel, on top of the
same `volcano` component the dev-board example exercises. Every value the
component tracks — current/target temperature, heater/pump, auto-shutoff
duration and countdown, LED brightness, vibration, display-on-cooling,
display units, and the device-information/diagnostic strings — has a Dial
page, so the Dial controls the Volcano fully on its own with no phone,
browser or Home Assistant involved.

No secrets file is needed to build or flash this configuration —
nothing in it reads `!secret` at all. The Volcano's address, WiFi
credentials and the Home Assistant API key are all supplied on the
device at runtime instead: see "Onboarding" and "Pairing" below. WiFi
serves the Home page's WiFi status icon, the Connections page's WiFi
status and toggle, the `web_server` page below, and the Home Assistant
`api` connection; the on-screen UI itself needs none of them to
navigate.

The config is split into `dial/*.yaml` packages by concern
(hardware/peripherals, connectivity, shared state, the `volcano:`
entities, the two physical inputs, the write-coalescing scripts, and one
file per page) rather than one large file — `esphome
config`/`compile`/`run` still take `m5stack-dial.yaml` itself, which
assembles them via `packages:`.

**Pages** (see ADR-0011 for the full navigation model): **Home** shows current temperature (orange) and target temperature (cyan) in a seven-segment style font, with Heat/Air touch buttons, the auto-shutoff countdown, and small BLE/WiFi status icons (steady = connected, red = disconnected; the BLE icon also flashes while connecting, the WiFi one does not). Turning the rotary encoder on Home adjusts target temperature; a press opens the **Navigation Menu**, which lists every other page — turn to highlight, press or touch to open. **Connections**, second in the Menu, shows the same BLE/WiFi status in full alongside a switch for each — releasing BLE frees the Volcano for the official app without power-cycling the Dial, and toggling WiFi is mainly useful for confirming the Dial keeps working with neither connection present — plus a read-only Home Assistant line showing whether an `api` client is connected (no switch, per [ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md)). **Settings** has touch toggles for vibration, display-on-cooling and display units. **LED Brightness**, **Auto-Shutoff Duration** and **Dial Brightness** share one page layout: turn to adjust, with 1×/10×(/30×) step-size buttons for coarser changes; Dial Brightness is the Dial's own backlight, not a Volcano setting, so it works even with no Volcano connected. **Dial Sound** is an on/off toggle for the click/beep feedback described below. **About** and **Diagnostics** show the device-information strings and the heater-runtime meter. A button press returns to the Menu from every page except Home and the Menu itself.

Every value that changes while a page adjusts it locally (target temperature, LED brightness, auto-shutoff duration) updates the display immediately but only writes to the Volcano once turning pauses — the same write-coalescing ADR-0011 requires, so a fast turn doesn't fire a write per detent. Re-opening one of these pages resyncs its shown value to the last one the device confirmed, so a write that never took (a dropped BLE write on a characteristic with no notification to correct it) doesn't leave a stale figure on screen. Every rotary turn plays a short click and every button press or control touch plays a short beep, muted together via the Dial Sound page; the buzzer has no real volume control, so this is on/off rather than graduated (see that page's own note in the config for why).

`web_server` is present too, unchanged from the dev-board example's page (same port, no authentication, no TLS — see [`examples/README.md`](../examples/README.md#sending-commands-and-watching-state-web_server) for that trust model and the full per-entity walkthrough). It carries one control per value for every entity listed above, plus a "Dial firmware version" diagnostic entity for this firmware's own version (distinct from the Volcano's own firmware/BLE firmware version strings). Reaching it is optional — the Dial's own screen is the primary control surface.

## Home Assistant

The `api` block ([ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md)) lets Home Assistant's ESPHome integration discover the Dial over WiFi and expose the same entities the `web_server` page carries — the `volcano` controls and sensors, plus the Dial firmware-version diagnostic. The Dial's own raw rotary-encoder and button inputs are kept off it.

To add it: WiFi must be provisioned first (see "Onboarding" below — Home Assistant needs the Dial on the network before it can find it), then go to **Settings → Devices & Services → Add Integration → ESPHome** and enter the Dial's IP or `volcano-hybrid-dial.local`, or just accept it if Home Assistant's own discovery has already surfaced it. With no key compiled in, this is enough: Home Assistant completes the bootstrap handshake described in "Onboarding" itself and keeps the key it agrees on — nothing to type in, and no key exists yet for you to have gotten wrong. This only works within the same 20-minute provisioning window WiFi shares (see "Onboarding"); if it has closed, power-cycle the Dial and add the device again within the new window. If Home Assistant instead prompts for an encryption key rather than completing this handshake itself, that Home Assistant version does not yet support it; [ESPHome's own dashboard](https://esphome.io/guides/getting_started_hassio.html) speaks the same handshake through its "Adopt" action and can be used to agree a key by hand, which can then be typed into Home Assistant's prompt.

This was verified on real hardware: the bootstrap handshake itself (agreeing a key, the well-known key then being rejected, and a cleared key reopening it — see "Onboarding"), and, from an earlier increment with a compiled-in key, discovery, control from Home Assistant, and changes reflecting both ways across the Volcano's panel, the Dial, the `web_server` page and Home Assistant.

The connection is not load-bearing. The config sets `reboot_timeout: 0s` on `api` *and* on `wifi`, so the Dial never reboots for want of an API client or a WiFi association. Losing Home Assistant, or the network, or never having either, changes nothing about BLE control, the Dial UI, or the `web_server` page — the standalone guarantee ADR-0001 requires. The trade-off is that neither component will auto-reboot to recover from a wedged network stack. This too was checked on hardware: with Home Assistant stopped, with the wrong key, and with WiFi absent, the Dial kept full control of the Volcano in every case, with no reboot.

The temperature entities are always Celsius, and Home Assistant converts them per its own unit settings (system-wide, or a per-entity override) because they carry a temperature device class — see [ADR-0008](../docs/decisions/ADR-0008-temperature-units-handling.md). The Volcano's "Display in Fahrenheit" switch is the *device's own screen* setting; it does not change what these entities report, so the Volcano and Dial showing °F while Home Assistant shows °C (or the reverse) is expected, not a fault.

## Flashing and watching logs

Requires the [ESPHome CLI](https://esphome.io/). The first flash needs the Dial connected over USB-C — from the repository root:

```sh
esphome run firmware/m5stack-dial.yaml
```

This compiles, flashes, and opens the log monitor in one step — it prompts for how to reach the device on first run (a serial port, or, once the Dial has been flashed with WiFi already provisioned, its IP/hostname over the network). To flash and watch logs as separate steps:

```sh
esphome upload firmware/m5stack-dial.yaml
esphome logs firmware/m5stack-dial.yaml
```

`esphome logs` also re-attaches to an already-running device without reflashing it; over the network it needs [ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md)'s `api` connection, so it doesn't work with WiFi provisioned but Home Assistant/API access otherwise unreachable.

Every later update can go out over the network — `ota:` ([ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md)) is enabled, on the same trust model as `web_server` above (no password; a trusted home network is assumed). Recovery from a bad update needs no attention: if the new firmware never reports itself healthy — a minute of uptime — the bootloader reverts to the one it replaced on the very next boot, and a device stuck in a genuine boot loop drops into a minimal recovery mode after ten failed boots that accepts one more update without running anything else. Verified on real hardware: an update pushed over the network while running, landing and taking over cleanly with the Volcano still connected and paired throughout.

Watch for the `[volcano]` log tag: it logs heater/pump state, the auto-shutoff countdown, and current/target temperature on connect and whenever they change, including changes made at the device's own panel. Each `on_*` handler across the packages also logs at `DEBUG`, so `esphome logs` shows each peripheral responding to input.

## Onboarding

WiFi credentials and the Home Assistant API's per-device encryption key are both supplied at runtime, not compiled in ([ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md)). Neither blocks the rest of the Dial — an unprovisioned device still boots and runs its local UI and Volcano control fully; only WiFi and Home Assistant wait.

**WiFi** is provisioned over the same USB connection used to flash the Dial, via the [Improv Wi-Fi](https://www.improv-wifi.com/serial/) protocol (`improv_serial`): a tool that speaks Improv over serial scans for networks, connects to the one chosen, and the result is saved to flash — every later boot reads it back from there, with no further setup. The browser install page ([`install/`](../install/README.md), live but with nothing to flash yet) offers this as a step right after flashing; meanwhile [ESPHome's own dashboard](https://esphome.io/guides/getting_started_hassio.html) has a "Configure Wi-Fi" button that speaks the same protocol over the same serial port `esphome logs` uses.

**The API encryption key** is generated on first connection rather than baked in: `api:` boots with no key at all, accepting one bootstrap connection — Home Assistant's own "Add device" flow, or ESPHome dashboard's "Adopt" — which agrees a real, per-device key that is then saved to flash and used exclusively from then on. No released image ever carries a key that would unlock a second device.

Both close after a fixed window (20 minutes by default) if neither is set up, rather than answering indefinitely — a power cycle reopens it. Watch for the `[provisioning]` log tag: it logs the window closing, if it does.

## Pairing

The Dial is not told which Volcano to control at build time. On first boot, with none paired, it scans for one for about ten seconds ([ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md)) and opens the Pairing page so the search is on screen, returning to Home once a Volcano is paired: if exactly one Volcano Hybrid is in range it pairs with it and stores its address, so every later boot connects directly with no scan. Two or more are offered as a list, strongest signal first and identified by serial number — turn the dial and press it, or touch one, to choose. None leaves the page at `Not found`. Nothing about this blocks the rest of the Dial — an unpaired Dial boots and runs its local UI, and only Volcano control waits.

The Pairing page ([ADR-0011](../docs/decisions/ADR-0011-dial-ui-navigation-architecture.md)), next to Connections in the Menu, is also where to search again — `Search again` on the page, or simply opening it while the last search found nothing — and where to replace a unit: `Hold to forget` on the paired page clears it and searches again. It takes a long press because forgetting drops the connection to a Volcano that may be heating. The same is reachable from the `web_server` page and Home Assistant: `Pairing status` reports where it has got to, and `Volcano address` shows the paired address, accepts one typed in, and forgets the unit when set to an empty string. The component's own documentation, [`components/volcano/README.md`](../components/volcano/README.md#pairing), covers the mechanism.

## Troubleshooting

**Nothing from the Volcano — no connection, no decoded state.** The device accepts only one connection at a time and stops advertising while connected ([CONN-003](../docs/protocol/gatt-services.md#conn-003--single-connection-at-a-time)); make sure it isn't already connected to the official app. If the Pairing page (or the `Volcano address` entity on the `web_server` page) shows no address, the Dial has not paired yet — the page says why, and `Search again` on it, or opening it, searches again. The Connections page's BLE row shows `Disconnected - in use?` when the Dial has an address but cannot connect.

**Home Assistant doesn't discover the Dial, or the `web_server` page is unreachable.** Check whether WiFi has actually been provisioned yet — see "Onboarding" above; a device that has never been given a network, or whose provisioning window closed before it was, needs a fresh Improv session (power-cycle it first if the window already closed). Once connected, check that the machine is on the same network the Dial joined — `esphome logs` prints the IP once WiFi connects. `<hostname>.local` (`volcano-hybrid-dial.local` by default) needs mDNS, which not every network/browser combination has; the numeric IP always works. The Connections page's WiFi and Home Assistant rows show the current state on-screen.

**The screen briefly wipes on a page change.** Expected: LVGL's draw buffer is trimmed to fit BLE and `web_server` in RAM on this PSRAM-less board (see `dial/hardware.yaml`'s comment above its `lvgl:` block), so a full-screen redraw takes several flush passes.
