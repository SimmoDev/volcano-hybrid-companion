# Firmware

The device firmware this project ships. One target so far: the M5Stack
Dial. The config under [`examples/`](../examples/README.md) is a compile
check and BLE-only test surface for the `volcano` component; this is the
configuration meant to be flashed to a device and used.

The firmware is feature-complete and versioned `1.0.0`, but the project
is not yet released: packaging it as a browser-flashable download, with
WiFi and device details entered on the device instead of compiled in, is
Phase 4 — see
[ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md). Until
then, flashing needs the ESPHome CLI, as below. See the root
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

Copy [`secrets.yaml.example`](secrets.yaml.example) to `secrets.yaml`
alongside it (not committed — see the repository's `.gitignore`) and set
`volcano_mac_address`, `wifi_ssid`/`wifi_password` and
`api_encryption_key`. A placeholder value is fine for `esphome
config`/`esphome compile`; flashing to hardware needs the real
`volcano_mac_address` and WiFi credentials, and a generated
`api_encryption_key` (the committed placeholder is public — see that
file's own comment). WiFi serves the Home page's WiFi status icon, the
Connections page's WiFi status and toggle, the `web_server` page below,
and the Home Assistant `api` connection; the on-screen UI itself needs
none of them to navigate.

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

The `api` block ([ADR-0012](../docs/decisions/ADR-0012-home-assistant-integration.md)) lets Home Assistant's ESPHome integration discover the Dial over WiFi and expose the same entities the `web_server` page carries — the `volcano` controls and sensors, plus the Dial firmware-version diagnostic. The Dial's own raw rotary-encoder and button inputs are kept off it. Add the device in Home Assistant with the `api_encryption_key` from your `secrets.yaml`. This was verified on real hardware: discovery, control from Home Assistant, and changes reflecting both ways across the Volcano's panel, the Dial, the `web_server` page and Home Assistant.

The connection is not load-bearing. The config sets `reboot_timeout: 0s` on `api` *and* on `wifi`, so the Dial never reboots for want of an API client or a WiFi association. Losing Home Assistant, or the network, or never having either, changes nothing about BLE control, the Dial UI, or the `web_server` page — the standalone guarantee ADR-0001 requires. The trade-off is that neither component will auto-reboot to recover from a wedged network stack. This too was checked on hardware: with Home Assistant stopped, with the wrong key, and with WiFi absent, the Dial kept full control of the Volcano in every case, with no reboot.

The temperature entities are always Celsius, and Home Assistant converts them per its own unit settings (system-wide, or a per-entity override) because they carry a temperature device class — see [ADR-0008](../docs/decisions/ADR-0008-temperature-units-handling.md). The Volcano's "Display in Fahrenheit" switch is the *device's own screen* setting; it does not change what these entities report, so the Volcano and Dial showing °F while Home Assistant shows °C (or the reverse) is expected, not a fault.

## Flashing and watching logs

Requires the [ESPHome CLI](https://esphome.io/) and the Dial connected over USB-C. From the repository root:

```sh
esphome run firmware/m5stack-dial.yaml
```

This compiles, flashes over USB, and opens the log monitor in one step — it prompts for a serial port on first run. To flash and watch logs as separate steps:

```sh
esphome upload firmware/m5stack-dial.yaml
esphome logs firmware/m5stack-dial.yaml
```

`esphome logs` also re-attaches to an already-running device without reflashing it.

This config declares no `ota:` block, so every reflash is over USB — there is no over-the-air update path. That is a deliberate omission while the firmware is CLI-flashed; Phase 4 ([ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md)) adds `ota:` with `safe_mode` for the release, so an installed Dial can be updated without a USB cable.

Watch for the `[volcano]` log tag: it logs heater/pump state, the auto-shutoff countdown, and current/target temperature on connect and whenever they change, including changes made at the device's own panel. Each `on_*` handler across the packages also logs at `DEBUG`, so `esphome logs` shows each peripheral responding to input.

## Troubleshooting

**Nothing from the Volcano — no connection, no decoded state.** The device accepts only one connection at a time and stops advertising while connected ([CONN-003](../docs/protocol/gatt-services.md#conn-003--single-connection-at-a-time)); make sure it isn't already connected to the official app, and that `volcano_mac_address` in `secrets.yaml` is correct. The Connections page's BLE row shows `Disconnected - in use?` in this case.

**Home Assistant doesn't discover the Dial, or the `web_server` page is unreachable.** Check `wifi_ssid`/`wifi_password` in `secrets.yaml`, and that the machine is on the same network the Dial joined — `esphome logs` prints the IP once WiFi connects. `<hostname>.local` (`volcano-hybrid-dial.local` by default) needs mDNS, which not every network/browser combination has; the numeric IP always works. The Connections page's WiFi and Home Assistant rows show the current state on-screen.

**The screen briefly wipes on a page change.** Expected: LVGL's draw buffer is trimmed to fit BLE and `web_server` in RAM on this PSRAM-less board (see `dial/hardware.yaml`'s comment above its `lvgl:` block), so a full-screen redraw takes several flush passes.
