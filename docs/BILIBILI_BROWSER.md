# Bilibili browsing build

Local test version: `2.5.6-bilibili-browser`, based on CoffeeFlix commit
`58a9e3c66e1dfde08f86b1619282d29b2cc1f45f` plus the local playback prototype.
The earlier prototype's playback, fast-forward and scrubbing were confirmed by
the user on Wii U. That report does not certify this new browser build.

## What changed

- The former YouTube side-menu entry now opens Bilibili's popular-video grid.
- The home screen's video feed and Explore tile show Bilibili content.
- Global search searches Bilibili videos instead of YouTube videos/channels.
- The Bilibili page supports keyword search, entering a BV ID/full video URL,
  refresh, and explicit pagination (up to 200 cards per page instance).
- Cards include cover art, title, UP name, duration and view counts. Opening a
  card fetches the video's actual part list. Select a part to play using the
  previously tested H.264/AAC player and initial CDN fallback.
- The former YouTube settings page now contains Bilibili browsing and the
  360p/480p/720p quality preference. The maximum also applies when opening a
  video from Home or global search. The server can offer a lower quality.
- Chinese labels were added for the new browsing and detail controls.

This build is anonymous. It does not implement account login, subscriptions,
personalized recommendations, comments, danmaku, subtitles, short-link expansion,
or local Bilibili history/resume. Existing YouTube history is retained on disk
but excluded from the home screen; it is not converted into Bilibili history.
The existing settings navigation IDs (`youtube`, `SEC_YOUTUBE`) are retained for
compatibility with saved startup/visibility preferences. The legacy YouTube
implementation remains in the source, disconnected from normal browsing.

YouTube channel allowlists cannot describe Bilibili channels, so Bilibili
browsing is unavailable in kids profiles in this build. Switch to a normal
profile to use it. The earlier playback diagnostic remains under Settings >
Updates and about > Bilibili playback test.

## Service implementation

Popular lists use `/x/web-interface/popular`. Search uses
`/x/web-interface/wbi/search/type`, an anonymous visitor cookie from
`/x/frontend/finger/spi`, and WBI keys from `/x/web-interface/nav`.
Search uses the desktop-client parameters and compatible request headers from
the referenced wiliwili adapter (including its API User-Agent and client Referer).
Playback and media keep the browser headers used by the tested prototype.
Anonymous visitor state is memory-only; no account credentials are involved.

A search response containing `v_voucher` is reported as requiring verification,
even when its top-level code is zero. It is not displayed as a successful empty
search. Missing/malformed lists, HTTP failures and Bilibili error codes also
produce an error and a manual retry action. Leaving a Bilibili page cancels its
pending requests and suppresses callbacks to the destroyed screen.

Reference: wiliwili `yoga` API implementation:
<https://github.com/xfangfang/wiliwili/tree/yoga/wiliwili/source/api>.

## Build and validation

Wii U compilation reuses the existing Mac mini `uflow-builder:dev` image and
CoffeeFlix's dependencies in a separate temporary build copy. The original
prototype directory and delivered files remain unchanged. The build command is:

```sh
make -j4 APP_VERSION=2.5.6-bilibili-browser \
  OPTFLAGS='-O2 -g -ffunction-sections -fdata-sections -fomit-frame-pointer -Wno-error=array-bounds'
```

The warning override addresses the same GCC 15 diagnostic in unchanged subtitle
code documented in [BILIBILI_PROTOTYPE.md](BILIBILI_PROTOTYPE.md).

Service tests:

```sh
make -f tools/tests/bilibili_protocol.mk test live
./build/bilibili-protocol/live --browse
./build/bilibili-protocol/live --live BV1GJ411x7h7
```

The offline suite covers signing, details, stream selection and CDN fallback,
plus popular/search parsing, title markup, duration strings, duplicate entries,
pagination, malformed data, verification challenges, visitor setup and caching.
The live browse check tests popular pages 1/2, English and Chinese searches,
and opening a search result's part list. The separate live playback probe reads
byte ranges from the selected audio/video streams; it does not decode them.

The desktop preview can open the new screens directly using `COFFEEFLIX_OPEN`:
`bilibili`, `bili-search:Nintendo`, `bili-video:BV1GJ411x7h7`, or `settings:4`.
Its existing `COFFEEFLIX_SCRIPT` commands can capture screenshots and exercise
search text entry, opening a card, and returning to the list. Preview settings
and fonts are isolated from the user's device and normal application data.

## Validation on 2026-10-26

- Wii U and desktop builds completed successfully.
- The offline protocol/resolver suite passed, including browsing and search cases.
- Real C++ requests passed popular pages 1/2, English and Chinese keyword search,
  and opening a search result's part list.
- The original live playback probe still passed metadata, signed stream resolution
  and audio/video byte ranges at 480p.
- The actual desktop application was driven through opening a popular card,
  returning, typing a search and displaying results. Screenshots of popular,
  search, multi-part detail, Home, global search and settings were inspected.
- At delivery, this browser build had not yet been tested on Wii U hardware.
- After delivery on 2026-10-26, the user reported that browsing and playback
  worked on Wii U. This is user-reported hardware evidence. Search, pagination,
  multi-part selection, quality switching and long-session stability were not
  separately confirmed in that feedback. The exact video, firmware and installed
  file checksum were not supplied. Delivered binaries, archives and build-time
  records are preserved unchanged.

## Install and test on Wii U

The Aroma ZIP uses the original `wiiu/apps/coffeeflix/coffeeflix.wuhb` path.
Preserve the installed app before replacing it with this local test build.
No automatic deployment, Git commit, push or public release is performed.

Check the Bilibili side-menu entry, Home feed, covers and Chinese titles; search
with both Chinese and English; load another result page; open a multi-part video;
play a selected part and try pause, seeking, Back, and another video. Confirm
the Bilibili quality preference is used. Test hiding/showing the service and
starting the app directly on Bilibili if those preferences are in use.
