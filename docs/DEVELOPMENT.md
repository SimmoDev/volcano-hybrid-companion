# Development guide

This document orients a new contributor: what this repository is, what stage it's at, how it's laid out, and how to validate the component locally. It doesn't repeat architectural reasoning that already lives in an ADR — it links to those instead.

## What this repository is

Volcano Hybrid Companion is a standalone remote controller for the Storz & Bickel Volcano Hybrid vaporizer, built on ESPHome and ESP32 hardware. See the root [README.md](../README.md) for the full project vision, phased roadmap, and design principles.

## Current phase

Phase 1 is complete: the BLE foundation and the Volcano component are implemented and hardware-verified on an ESP32-S3-WROOM-1-N16R8 development board, per [ADR-0004](decisions/ADR-0004-development-hardware-strategy.md). Phase 2 is also complete: the M5Stack Dial's local UI, built per [ADR-0010](decisions/ADR-0010-dial-hardware-and-ui-framework.md)/[ADR-0011](decisions/ADR-0011-dial-ui-navigation-architecture.md), gives the Dial full control of the Volcano with no external dependency — see [`examples/README.md`](../examples/README.md#m5stack-dial-minimalyaml) for what it does. The project is now in **Phase 3**: Home Assistant integration through the ESPHome `api` component, following [ADR-0012](decisions/ADR-0012-home-assistant-integration.md). The implementation is complete and hardware-verified: both example configurations carry an encrypted `api:` block, the Dial's Connections page shows Home Assistant connection status, and the standalone guarantee — the Dial keeps full control of the Volcano with Home Assistant stopped, the API key wrong, or WiFi absent, and does not reboot — has been checked on real hardware. The milestone exit review and the `firmware/` + v1.0.0 finalisation are still to come before Phase 3 formally closes.

What Phase 1 delivered, and what Phase 2 builds on:

- **The Volcano BLE protocol is implemented.** The `volcano` component connects, resolves nineteen characteristics by UUID — the status/flags register, the auto-shutoff countdown and duration, current/target temperature, the heater/pump on/off triggers, the heater-runtime meter, LED brightness, the vibration setting and the display/units register, and five device-information strings — subscribes to the notify-capable ones, reads the auto-shutoff duration, the LED brightness and the device-information strings once per connection, and decodes all of those into `VolcanoDevice`'s state model. The four heater/pump trigger characteristics are resolved for writing only: they carry no readable state, so they are neither subscribed nor read. It also writes the auto-shutoff duration, the target temperature, the LED brightness, the heater/pump on/off triggers, the vibration setting, display on cooling and the display units, refusing values outside the ranges confirmed accepted where one exists, and tracks each write as requested-but-unconfirmed until the device's own report resolves it. Each value can optionally be exposed as an ESPHome entity — a sensor for the read-only ones, and for each writable one a single two-way entity that both reports and sets it (a number for the target temperature, the auto-shutoff duration and LED brightness; a switch for the heater, pump, vibration, display-on-cooling and display-units), plus a `connected` binary sensor — so state and commands are reachable from a `web_server` page or Home Assistant. See [`components/volcano/README.md`](../components/volcano/README.md) for the full interface.
- **Protocol discovery is underway and follows [ADR-0005](decisions/ADR-0005-volcano-ble-discovery-methodology.md)**: findings are evidence-driven and classified Confirmed/Probable/Unknown, and only Confirmed findings back default implementation. See [`docs/protocol/`](protocol/README.md) for recorded findings.
- **The Volcano component's internal architecture follows [ADR-0002](decisions/ADR-0002-volcano-component-architecture.md)**, made concrete by [ADR-0009](decisions/ADR-0009-volcano-abstraction-layer-interface.md): a `VolcanoBleClient` (the BLE communication layer), a `VolcanoDevice` (the abstraction layer, owning the single authoritative state model and requested-versus-confirmed write handling), and a `VolcanoComponent` (the ESPHome integration, one consumer of `VolcanoDevice`'s interface among others). `VolcanoDevice` has no BLE dependency, so a control interface can be built and exercised against it without any BLE hardware present.
- **Temperature is Celsius throughout the component**, per [ADR-0008](decisions/ADR-0008-temperature-units-handling.md), matching the wire encoding. The device's own Celsius/Fahrenheit display setting is exposed as an ordinary device setting and changes nothing the component reports; converting for presentation belongs to a control interface.
- **ESPHome is the firmware framework**, per [ADR-0003](decisions/ADR-0003-esphome-as-firmware-framework.md). The `volcano` component is implemented as an ESPHome external component, and the Phase 3 Home Assistant integration ([ADR-0012](decisions/ADR-0012-home-assistant-integration.md)) is an optional consumer of it, not a dependency.

## Repository structure

- **`components/volcano/`** — the Volcano ESPHome external component. See [`components/volcano/README.md`](../components/volcano/README.md) for its current implementation status and component-specific build notes.
- **`examples/`** — example ESPHome YAML configurations that exercise the components in this repository; they are not production device firmware. See [`examples/README.md`](../examples/README.md) for what each one does and how to flash it to real hardware.
- **`docs/decisions/`** — the ADR series. Each ADR records one architectural decision and its reasoning; see the [ADR index](decisions/README.md) for the full set.
- **`docs/protocol/`** — Volcano BLE protocol findings, recorded per [ADR-0006](decisions/ADR-0006-protocol-documentation-structure.md).
- **`docs/CONVENTIONS.md`** — the living reference for terminology, spelling, Markdown/naming conventions, and commit message style used across this repository.

## Validating the component locally

The current example configuration is [`examples/esp32-s3-devkit-minimal.yaml`](../examples/esp32-s3-devkit-minimal.yaml). It loads the `volcano` component and targets the Phase 1 development board, with two-way number/switch controls and read-only state sensors — see [`examples/README.md`](../examples/README.md) for what they do. It reads a BLE MAC address from `examples/secrets.yaml` (not committed — copy it from [`examples/secrets.yaml.example`](../examples/secrets.yaml.example)), but `esphome config`/`esphome compile` below need only a placeholder value, not a real device.

Requires the [ESPHome CLI](https://esphome.io/) installed locally. From the repository root:

```sh
# Validate the YAML and confirm the component registers correctly
esphome config examples/esp32-s3-devkit-minimal.yaml

# Compile the firmware (no physical device required for this step)
esphome compile examples/esp32-s3-devkit-minimal.yaml
```

Both commands should complete without errors. `esphome compile` is the closest available check that the component's C++ actually builds; it requires no physical ESP32-S3 hardware.

To flash this example to real hardware and watch its logs, see [`examples/README.md`](../examples/README.md).

## Testing `VolcanoDevice`

`VolcanoDevice` (the Volcano abstraction layer) has no BLE or ESP-IDF dependency by design (ADR-0009), so it is tested directly on the host rather than through `esphome compile`/hardware. [`components/volcano/test/`](../components/volcano/test/) builds `volcano_device.cpp` against fake stand-ins for its BLE client and the handful of ESPHome core headers it touches, with no ESPHome installation or physical device required:

```sh
cd components/volcano/test
make test
```

The same command also builds and runs three further host-side tests, each covering a part of `VolcanoBleClient` that has no BLE/ESP-IDF dependency of its own, unlike the rest of that class, so it is exercised directly rather than only through real hardware:

- `DisplayRegisterWriteQueue` (`components/volcano/display_register_write_queue.h`), the FIFO `VolcanoBleClient` uses to attribute a completed write on CHAR-009 to whichever of its two independent settings actually issued it.
- `StaticReadQueue` (`components/volcano/static_read_queue.h`), the bookkeeping for the characteristics read once per connection rather than subscribed — which handle is due next, and whether an incoming read completion is the initial sweep's own rather than a write's read-back.
- `wire_format.h` (`components/volcano/wire_format.h`), the value-level encode and decode: the decidegrees and 2-byte-scalar codecs, the device-information-string trimming, each settings bit's polarity, and the confirmed-accepted write ranges — the logic where a wrong mask or a loosened bound would reach real hardware.

The `esp_ble_gattc_*` reads and writes, and the GATT event sequencing around them, are issued by `VolcanoBleClient` itself. That code, and `VolcanoComponent`, still require a real connection to verify, per [`components/volcano/README.md`](../components/volcano/README.md#building--validating).

## Continuous integration and pre-commit hooks

[`.github/workflows/ci.yml`](../.github/workflows/ci.yml) runs on every pull request and on pushes to `main`: `esphome config`/`esphome compile` against each example configuration, both sharing the same placeholder `secrets.yaml` (per "Validating the component locally" above) — the Phase 1 dev board's, and the M5Stack Dial's, which connects the volcano component over BLE, joins WiFi (for its Home page's status icons, its own `web_server` page, and its Home Assistant `api` connection), and serves the full ADR-0011 local UI — `make test` for the Volcano component's host-side tests, a `clang-format --dry-run --Werror` check (see [`docs/CONVENTIONS.md`](CONVENTIONS.md#code-formatting-expectations) for the style and why it's pinned to an exact version), and `scripts/check_markdown_links.py` over every tracked `.md` file — the same check as the `check-markdown-links` pre-commit hook below, so a broken relative link is caught even without the hook installed. None of the four jobs need real hardware.

Three local pre-commit hooks, defined in [`.pre-commit-config.yaml`](../.pre-commit-config.yaml):

- **`clang-format`** — reformats `components/volcano/`'s C++ in place, matching the CI check exactly (same pinned version). See [`docs/CONVENTIONS.md`](CONVENTIONS.md#code-formatting-expectations).
- **`check-markdown-links`** (implemented in [`scripts/`](../scripts/)) — blocks the commit if a relative Markdown link in a tracked `.md` file doesn't resolve to a real file, or — where the link carries a `#fragment` — to a real heading in that file (heading slugs are regenerated with GitHub's algorithm). Link-like text inside backticks (a syntax example, as in `CONVENTIONS.md`'s Markdown conventions section) is not checked. The CI `markdown-links` job runs the same script over the whole repository, so this is enforced server-side too.
- **`readme-staleness-nudge`** (implemented in [`scripts/`](../scripts/)) — advisory only, never blocks. When a commit touches `components/volcano/`, `docs/decisions/` or `examples/`, it prints a reminder naming each of the root `README.md`, this document, and [`examples/README.md`](../examples/README.md) the commit does not also touch, since such a change can make an implementation-status, phase, capability, or entity-behaviour claim in any of them go stale — and touching one of the three does nothing for a claim that has drifted in another.

Install with:

```sh
pip install pre-commit
pre-commit install
```

`pre-commit run --all-files` runs all three hooks against the whole repository without committing anything.
