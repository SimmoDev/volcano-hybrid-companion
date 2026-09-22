# Install page

The browser-flashable install page [ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md)
calls for: a static page using [ESP Web Tools](https://esphome.github.io/esp-web-tools/)'
install button, which flashes `firmware/m5stack-dial.yaml`'s factory image over
WebSerial with no ESPHome toolchain needed. Deployed to GitHub Pages by
[`.github/workflows/pages.yml`](../.github/workflows/pages.yml).

**Live** at https://simmodev.github.io/volcano-hybrid-companion/. The install
button itself has nothing to flash yet, though — no release has been cut for
`manifest.json` to point at, so it will fail until one exists — see the
checklist in ADR-0013's Notes.

## Files

- **`index.html`** — the page itself. Matches the Dial's own on-screen colour
  palette (`firmware/m5stack-dial.yaml`'s `cyan`/`orange`/`dark_grey`
  substitutions). Loads ESP Web Tools from its CDN, pinned to an exact
  version — that library ships as several interdependent chunks rather than
  one file, so vendoring a copy (the way `firmware/fonts/` vendors the DSEG7
  font) would mean keeping that whole internal layout in sync by hand; its
  publisher's own CDN distribution is the supported way to load it.
- **`manifest.json`** — what the install button reads: the target chip
  (`ESP32-S3`) and where to fetch the factory image. Points at
  `.../releases/latest/download/volcano-hybrid-dial.factory.bin` — GitHub's
  own stable alias for "whichever release is newest" — rather than one
  specific release, so this file does not need touching again as later
  releases replace each other. That depends on
  [`release.yml`](../.github/workflows/release.yml) always naming that asset
  identically across releases; see its own comment.

## What needs updating by hand, at release time

`manifest.json`'s `version` field is display text only — it does not affect
what gets flashed, only what the install dialog says while doing it — but
left unattended it would drift from whatever `.../latest/` actually resolves
to after the next release. Bump it alongside `dial/hardware.yaml`'s own
`version:` and the git tag that names a release, the same three-way sync
[ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md) already
asks for between the tag and the firmware's own version string.
