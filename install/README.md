# Install page

The browser-flashable install page [ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md)
calls for: a static page using [ESP Web Tools](https://esphome.github.io/esp-web-tools/)'
install button, which flashes `firmware/m5stack-dial.yaml`'s factory image over
WebSerial with no ESPHome toolchain needed. Deployed to GitHub Pages by
[`.github/workflows/pages.yml`](../.github/workflows/pages.yml).

**Live** at https://simmodev.github.io/volcano-hybrid-companion/, flashing
whatever release is currently newest.

## Files

- **`index.html`** — the page itself. Matches the Dial's own on-screen colour
  palette (`firmware/m5stack-dial.yaml`'s `cyan`/`orange`/`dark_grey`
  substitutions). Loads ESP Web Tools from its CDN, pinned to an exact
  version — that library ships as several interdependent chunks rather than
  one file, so vendoring a copy (the way `firmware/fonts/` vendors the DSEG7
  font) would mean keeping that whole internal layout in sync by hand; its
  publisher's own CDN distribution is the supported way to load it.
- **`manifest.json`** — what the install button reads: the target chip
  (`ESP32-S3`) and where to fetch the factory image, `firmware/volcano-hybrid-dial.factory.bin`
  — a path on this same site, not a link to GitHub. See "Why the factory
  image isn't just linked from GitHub Releases" below for why.

## Why the factory image isn't just linked from GitHub Releases

A URL like `.../releases/latest/download/volcano-hybrid-dial.factory.bin`
— GitHub's own stable alias for "whichever release is newest" — looks like
it should work here: a plain download, or pasting it into a browser tab,
both fetch it successfully. It doesn't work from the install button,
though. ESP Web Tools reads the image's bytes via `fetch()`, which —
unlike a plain download or a top-level navigation — enforces CORS, and
GitHub's release asset storage redirects to a signed Azure Blob Storage
URL that carries no `Access-Control-Allow-Origin` header at all (confirmed
by reading its response headers directly), so a browser refuses to let
cross-origin JavaScript read it, even though *fetching* it in every other
sense works fine.

[`.github/workflows/pages.yml`](../.github/workflows/pages.yml) works
around this by downloading the latest release's factory image itself (a
plain server-side download, not subject to CORS at all) and publishing it
alongside `index.html`/`manifest.json` on the same origin as the page — a
same-origin request needs no CORS header. It re-runs whenever a release is
published, not just when `install/` itself changes, so the hosted copy
never lags behind what `manifest.json`'s path actually names.

## What needs updating by hand, at release time

`manifest.json`'s `version` field is display text only — it does not affect
what gets flashed, only what the install dialog says while doing it — but
left unattended it would drift from whatever release the page is actually
serving. Bump it alongside `dial/hardware.yaml`'s own `version:` and the
git tag that names a release, the same three-way sync
[ADR-0013](../docs/decisions/ADR-0013-release-and-distribution.md) already
asks for between the tag and the firmware's own version string.
