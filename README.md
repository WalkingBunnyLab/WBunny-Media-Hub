# WBunny Media Hub

A Bilibili-focused media player for Nintendo Wii U.

Developed and independently maintained by Walking Bunny Lab.

## Development status

- Version: **0.4.0-alpha.3**
- Status: **Alpha / Experimental**
- Free of charge, source-available, and non-commercial.
- Not an official Nintendo or Bilibili product; no endorsement is implied.

## Source-sharing scope

This repository is for source sharing and technical discussion, particularly
review of the Bilibili integration by CoffeeFlix's author. Precompiled packages
are kept out of Git history. The [0.4.0-alpha.3 pre-release](https://github.com/WalkingBunnyLab/WBunny-Media-Hub/releases/tag/v0.4.0-alpha.3)
separately provides the original WBunny UI installation package for experimental
hardware testing; it is not a formally accepted stable release.

The source-sharing export omits WBunny logos, launch artwork, Figma exports,
screenshots, and decorative UI images. All C++ sources, Bilibili adapters and
technical documentation needed to understand the integration are retained.
Existing code falls back to the bundled upstream Material Icons font when the
optional Figma icon PNGs are absent. Browsing, playback and input code are unchanged;
the export does not reproduce the private development build's visual styling.

Unmodified CoffeeFlix icon/launch images from the imported baseline are retained
only as required build/runtime fallback resources, together with their original
license and notices. This derivative is not an official CoffeeFlix release.

Earlier builds have user-reported Wii U browsing and playback validation. The
0.4.0-alpha.3 candidate passed desktop UI/regression checks and Wii U cross
compilation; its hardware acceptance remains pending. A successful build does
not establish successful hardware testing.

## Project background

WBunny Media Hub is built upon [CoffeeFlix](https://github.com/Roastedd/CoffeeFlix)
by Roastedd. The Bilibili API integration references implementation patterns from
[wiliwili](https://github.com/xfangfang/wiliwili); wiliwili is not an official
Bilibili SDK or a complete player dependency of this project.

The current milestone keeps CoffeeFlix's layout, navigation, player controls and
TV/GamePad interaction. Jellyfin, Plex, LAN and local playback are later phases.
The local development build has additional WBunny artwork; that optional artwork
is not part of this source-sharing export.

## Features

- Bilibili video browsing, search, and playback.
- Native Wii U interface with TV and GamePad interaction.

These features are implemented and covered by the validation described above;
the current alpha still requires device testing. Additional implemented scope:

- Home, Search, Bilibili, Live and Settings only.
- Bilibili popular videos, pagination, Chinese/English search, covers and UP names.
- Signed-in Bilibili opens Recommendations first; signed-out Bilibili opens
  Popular and hides Recommendations. Both feeds keep the existing card/player UI.
- Video details/parts and H.264/AAC playback, with initial CDN fallback.
- Live entry beside Bilibili in the left navigation: public lobby, pagination, parent/sub-area filters, saved
  rooms, room number/URL entry and signed-in followed-live rooms. Live H.264
  HLS/FLV uses the existing player and fresh CDN URLs; see
  [live integration](docs/BILIBILI-LIVE.md).
- Bilibili QR account sign-in, existing account favorite folders, favorite
  add/remove, and read-only account history with pagination.
- Local favorites, watched history and per-part resume / Continue watching.
- 360p/480p/720p preferences, subject to the streams offered by Bilibili.
- Local settings/logs and manually installed versions. No upstream automatic
  updates, supporter service, public release workflow or automatic log upload.

Subscriptions and danmaku are not implemented. Local playback is not automatically
reported to account history. See [Bilibili library](docs/BILIBILI-LIBRARY.md) for
account use, local data handling and remaining device-side tests.
Other provider implementations are retained in source as future references; they
are not exposed by the first milestone. Plex is not implemented.

## Local development builds

For the original WBunny interface, download the installation ZIP from the
[alpha pre-release](https://github.com/WalkingBunnyLab/WBunny-Media-Hub/releases/tag/v0.4.0-alpha.3)
and extract its `wiiu/` directory to the SD card root. That binary includes WBunny
artwork intentionally omitted from the source-sharing snapshot. The public
snapshot uses upstream fallback artwork, so it does not reproduce the binary's
visual assets byte for byte. Corresponding linked-library sources and rebuild
instructions are included as a separate pre-release asset.

When building locally for Aroma, the application uses:

    sd:/wiiu/apps/wbunny-media-hub/wbunny-media-hub.wuhb

Settings are `wbunny-media-hub.json` in that directory. Logs are
`wbunny-media-hub.log` and `wbunny-media-hub-previous.log`. CoffeeFlix's original
installation/settings are not overwritten or automatically imported.

Account session credentials are stored locally in the settings file on the SD
card. Do not share that file. Review and redact logs before attaching them to
issues; they can contain viewing activity, account identifiers, and network data.

## Build

`VERSION` contains the application version. Reuse the existing devkitPro/wut
Docker environment and patched dependencies:

Live API builds require separately configured client protocol parameters. The
source export includes neither private configuration nor generated client headers;
prepare an authorized configuration before running the existing build helper.
The optional artwork-regeneration scripts need the omitted design originals and
are not required by the default Makefile build.

```sh
python3 tools/configure-bilibili-live.py --reference /path/to/wiliwili/wiliwili/include/api/bilibili/util/http.hpp
bash tools/docker-build.sh
# Inside an established devkitPro environment:
bash tools/build-deps.sh
make -j4
```

`DEVKITPPC_IMAGE` selects an existing image. The verified Mac mini image is
`uflow-builder:dev` (linux/amd64); do not replace the machine's build environment.
The Makefile retains the known GCC 15 array-bounds diagnostic in unchanged
upstream subtitle code as a warning via `COMPAT_WARNINGS`; other errors remain
fatal. `desktop.mk` builds `build-desktop/wbunny-media-hub` for preview/testing.

```sh
make -f tools/tests/bilibili_protocol.mk test account-test live account-live
make -f tools/tests/bilibili_protocol.mk live-test live-probe
./build/bilibili-protocol/live --browse
./build/bilibili-protocol/live --live BV1GJ411x7h7
make -f desktop.mk -f tools/tests/project_identity.mk project-identity-tests
./build-desktop/project-identity-test
```

The original `COFFEEFLIX_*` desktop/service-test environment overrides are retained
for compatibility with existing test scripts; they do not select the SD install
path. Built packages, source snapshots, manifests, screenshots and logs live in
`artifacts/<version>/`. See [version storage](docs/VERSIONING.md).

## Credits

- [CoffeeFlix](https://github.com/Roastedd/CoffeeFlix) — Roastedd; upstream application.
- [wiliwili](https://github.com/xfangfang/wiliwili) — Bilibili API reference;
  the reference project uses GPL-3.0. This identifies wiliwili's license, not a
  relicensing of CoffeeFlix or WBunny Media Hub.
- FFmpeg, libsmb2, Mbed TLS, FreeType, and other libraries, fonts, and data retain
  their own applicable terms; see [third-party notices](content/licenses/NOTICES.txt).

## License and provenance

This project incorporates CoffeeFlix code licensed under PolyForm Noncommercial
1.0.0. Refer to the [original license](LICENSE.md) and
[third-party notices](content/licenses/NOTICES.txt) for applicable terms.
This project is free and non-commercial. CoffeeFlix code has not been relicensed.

Derived from Roastedd/CoffeeFlix upstream commit
`58a9e3c66e1dfde08f86b1619282d29b2cc1f45f` plus the local Bilibili browser work.
The user reported that the earlier CoffeeFlix-based prototype browsed and played
on Wii U. The independently named app and its new data path require their own
hardware verification; compilation or desktop playback does not establish that.

The upstream [PolyForm Noncommercial license](LICENSE.md), required copyright
notice and bundled third-party notices are preserved. The original README is
archived in [UPSTREAM_README.md](docs/UPSTREAM_README.md). File-level import hashes
are in [IMPORT_MANIFEST.json](docs/IMPORT_MANIFEST.json).

No remote repository, push or public publication has been configured.
