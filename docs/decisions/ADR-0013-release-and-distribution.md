# ADR-0013: Release and Distribution

## Status

Accepted

## Context

[ADR-0001](ADR-0001-project-vision.md) scoped the project as three sequential phases — BLE foundation, local standalone remote, Home Assistant integration — and named no release step of its own. "The `firmware/` directory and the first versioned release, `v1.0.0`" were treated as a mechanical finalisation folded into Phase 3's completion; [ADR-0012](ADR-0012-home-assistant-integration.md)'s Notes and exit checklist carried that assumption forward.

Phase 3 is now complete and hardware-verified. The M5Stack Dial configuration has moved from `examples/` to [`firmware/`](../../firmware/README.md) and carries `version: "1.0.0"`. What remains is turning that configuration into something a Volcano owner can install without being a developer.

That is not one mechanical step. Today both the dev-board scaffold and the Dial firmware are built and flashed with the ESPHome CLI, reading `volcano_mac_address`, `wifi_ssid`/`wifi_password` and `api_encryption_key` from an uncommitted `secrets.yaml` at compile time. Installing the firmware therefore requires Python, the ESPHome toolchain, a checkout of this repository, and hand-edited secrets. ADR-0001's goal of a standalone remote for Volcano owners is not met while that is the only path.

Closing the gap has its own decisions. A prebuilt binary cannot contain a user's WiFi credentials or their Volcano's BLE address, and must not contain a shared API encryption key. It needs a runtime onboarding path, a hosting and update story, and a defined browser/OS support matrix. One of those pieces — moving the Volcano address out of a compile-time substitution — changes the `volcano` component, which has otherwise been stable since Phase 1. This is a milestone in its own right, not a finalisation of Phase 3.

## Decision

**A fourth phase is added: Phase 4 — Release and Distribution.** ADR-0001's three-phase plan, its rule that phases are sequential gates, and its standalone-operation requirement are unchanged. Phase 4 does not begin until Phase 3's Home Assistant integration is verified against real hardware — which it is. Its goal: a Volcano owner with no development environment can install and run the Dial firmware.

**The firmware is distributed as a prebuilt binary flashed from a web browser.** A released factory image is attached to a versioned GitHub Release; a project install page hosts a browser-based flasher (WebSerial) and its manifest. The ESPHome CLI workflow documented in [`firmware/README.md`](../../firmware/README.md) remains fully supported for anyone building from source, and stays the only path for the dev-board scaffold under `examples/`.

**Device-specific configuration moves from compile time to runtime.** The three values a prebuilt binary cannot carry — the Volcano BLE address, WiFi credentials, and the API encryption key — are supplied on the device after flashing rather than baked into the image. WiFi is provisioned on-device; the Volcano address is entered on-device or discovered; the API encryption key is generated or entered per device, so no two installs share one. Which mechanism serves each is left to the implementing increments. Moving the Volcano address off its compile-time substitution is component-affecting work and lands as its own increment.

**The standalone guarantee holds during and after onboarding.** A device that has been flashed but not yet given WiFi or a Volcano address still boots, still drives the Volcano once given its address, and never blocks local control on a provisioning step being completed. This is ADR-0001's requirement — the same one [ADR-0012](ADR-0012-home-assistant-integration.md) applied to the `api` component — applied to the release image.

**The Dial firmware gains an over-the-air update path.** Both ESPHome configurations currently omit `ota:` deliberately, noted in their READMEs. A released binary that can only be updated over USB is not a reasonable ask of a non-developer, so the Dial firmware gains `ota:` with `safe_mode`. The dev-board scaffold does not need it.

**`v1.0.0` is cut as Phase 4's exit artefact, not before.** The Dial firmware's `version:` string is already `1.0.0`: that is the firmware's own version, not a release. The git tag `v1.0.0`, the GitHub Release, and the published install page are created together when Phase 4's exit criteria are met. [ADR-0012](ADR-0012-home-assistant-integration.md)'s exit-checklist item that anticipated the tag being cut at Phase 3's close is superseded by this decision; see that ADR's Notes.

## Consequences

**Benefits**

- The project reaches the users ADR-0001 is for. A Volcano owner installs the firmware from a web page instead of a toolchain.
- A single documented, browser-based install path replaces "clone the repository, install ESPHome, write a `secrets.yaml`".
- Per-device API keys close the shared-secret hole a prebuilt image would otherwise open: no released binary carries a key that unlocks write access to every install.
- An OTA path means a protocol or security fix reaches installed devices without a USB cable.

**Trade-offs**

- WebSerial flashing works only in Chromium-based desktop browsers (Chrome, Edge); it does not work in Firefox, in Safari, or on iOS. Users on those need the from-source CLI path or a separate desktop flasher. This is inherent to the browser API, not a project choice, and the install page must state it plainly.
- Runtime onboarding is new surface to design, build and support — a provisioning step, its failure and re-provisioning cases, and on-device storage of credentials — none of which a compile-time `secrets.yaml` needed.
- Moving the Volcano address to runtime touches the `volcano` component, stable since Phase 1.
- Hosting an install page and building release binaries is ongoing project infrastructure to keep working across ESPHome upgrades.
- Adding `ota:` widens the Dial's attack surface and uses flash. `safe_mode` mitigates a bad flash, not the exposure.

## Alternatives considered

**1. Fold the release into Phase 3's close, as originally planned**

Keep treating `firmware/` plus a `v1.0.0` tag as a mechanical finalisation of Phase 3. Rejected because the work is not mechanical: a prebuilt binary needs runtime onboarding, hosting, an update path and a support matrix, and one piece of it changes the `volcano` component. A tag over a firmware only developers can install would be a release in name only.

**2. Ship source only; no prebuilt binary**

Document the ESPHome CLI flow well and stop there. Rejected because it leaves installation gated on a development environment, contradicting ADR-0001's standalone-remote-for-owners goal. The CLI flow is kept as the from-source path, not the only path.

**3. Bake a shared API key or placeholder WiFi into the released image**

Ship one encryption key, or blank WiFi credentials, in the public binary. Rejected: a shared key gives everyone holding the image write access to every device running it, and a committed key in a public repository is not a secret. Per-device secrets are the point of the runtime-onboarding decision.

**4. Per-user builds — a configurator that compiles a personalised image**

A web service that takes a user's secrets and returns a custom binary. Rejected for now: it is a build service to run and secure, and it puts users' credentials through that service. Runtime onboarding on a generic image avoids both. Not precluded as a later addition.

**5. A desktop flashing application instead of a web flasher**

Rejected as the primary path: a desktop flasher is a per-OS artefact to build, sign and distribute, where the web flasher has no install step of its own. A desktop flasher may still be named as the fallback for the browsers alternative 1's trade-off rules out.

## Notes

- References [ADR-0001](ADR-0001-project-vision.md) for the phase plan this extends and the standalone requirement it inherits; [ADR-0012](ADR-0012-home-assistant-integration.md) for the same requirement applied to the `api` component and for the exit-checklist item this supersedes; and [ADR-0003](ADR-0003-esphome-as-firmware-framework.md) for ESPHome as the framework the release is built with.
- ADR-0001's Decision text still names three phases. Per the [ADR index](README.md)'s convention that accepted ADRs are not rewritten, the fourth phase is recorded in ADR-0001's Notes with a pointer here.
- The concrete mechanisms — which provisioning method is used, how the Volcano address is discovered or entered, where the install page is hosted, how the release build is wired — are implementation detail for the increments that follow, not fixed here, the same way [ADR-0012](ADR-0012-home-assistant-integration.md) left its `api` wiring to its increments.
- The dev-board scaffold under `examples/` is out of scope: it stays a from-source compile and BLE-test surface, keeps its `secrets.yaml` flow, and gains neither runtime onboarding nor `ota:`.
- **Phase 4 exit checklist.** The Decision points above, consolidated as the gate for cutting the release:
  - [ ] The Dial factory image builds reproducibly from a tagged commit.
  - [ ] A published install page flashes that image over WebSerial and completes WiFi provisioning.
  - [ ] The Volcano BLE address is set on-device, with no compile-time substitution required.
  - [ ] Each install uses its own API encryption key; no shared key ships in the image.
  - [ ] The Dial firmware carries `ota:` with `safe_mode`, verified by an over-the-air update on real hardware.
  - [ ] The standalone guarantee re-verified on the release image: a flashed-but-unprovisioned device boots, and local control never blocks on onboarding being finished.
  - [ ] `firmware/README.md` documents the browser install path and the supported-browser matrix, with the from-source CLI path kept alongside it.
  - [ ] The git tag `v1.0.0`, a GitHub Release with the image attached, and the published install page — created together.
