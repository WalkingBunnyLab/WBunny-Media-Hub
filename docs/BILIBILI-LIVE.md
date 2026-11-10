# Bilibili Live

Live is a separate left-navigation entry directly below Bilibili, at the same
level. Bilibili keeps its video recommendations/popular/search UI; Live opens
the live lobby and retains the existing layout/player. Live can independently
be hidden or selected as the startup page in personalization settings.

- Live lobby: paginated room cards, covers, UP names and viewing counts.
- Live areas: choose a parent category, then all rooms or a sub-category.
- Saved rooms: local, per-profile bookmarks, separate from video favorites.
  Press X on a room card to save/remove it. Only room metadata is persisted.
- Followed live: shown when signed in, using the existing QR account session.
  The account endpoint falls back to the client's followed-room feed section
  when that endpoint is unavailable; that fallback is a single page.
- Room entry: a positive room number or live.bilibili.com URL, including short
  room IDs. Invalid IDs, offline rooms and replay-only rooms show an error.
- Quality preference cycles Smooth (80), High definition (150), Ultra clear (250),
  Original (400). The service may return a different quality depending on the
  room/account. The player does not claim these numbers are fixed pixel heights.

The existing live player handles H.264/AAC HLS/FLV; HEVC/AV1 are excluded.
Playback resolves a fresh stream URL on open/reconnect and probes alternate CDN
URLs. Account cookies go only to the HTTPS API, never to media-CDN requests.
Live playback has no saved playback position or seek timeline. Quality changes
apply to the next open; there is no new in-player quality menu.

This does not include chat, danmaku, gifts, paid-stream support, live search or
live history. An API verification challenge shows a retry/error state; a network
error does not log the user out. Confirmed API login expiry does.

## Client protocol configuration

The feed/area API signing follows wiliwili's public client protocol. Before a
build, generate ignored configuration and headers from the local reference:

```sh
python3 tools/configure-bilibili-live.py --reference /path/to/wiliwili/wiliwili/include/api/bilibili/util/http.hpp
```

This stores `.local/bilibili-live-client.json` and generates
`build/bilibili_live_client.hpp` plus its desktop equivalent. None are tracked
or included in source archives. On the verified remote build directory, transfer
the configuration separately, then run the same script without `--reference`.
After cleaning either build directory, rerun the script. A clean source archive
requires this local configuration before compiling the production live service.

Reference: wiliwili (yoga branch), `wiliwili/source/api/home_api.cpp`,
`video_detail_api.cpp`, `include/api/bilibili/result/home_live_result.h` and
`include/api/bilibili/util/http.hpp`. Parsing/UI code here is implemented for
CoffeeFlix's service/widget/task architecture rather than importing Borealis UI.

## Verification

`make -f tools/tests/bilibili_protocol.mk live-test` is offline and uses fixture
signing parameters. It tests query signing, feeds/areas, malformed responses,
room states, codec filtering, cancellation, CDN fallback and cookie isolation.
`live-probe` is a read-only production HTTP check of anonymous lobby, pagination,
area filtering and readable live media; it prints no stream URLs.

For repeatable desktop interaction checks (Linux with offscreen SDL and CJK font):

```sh
make -f desktop.mk -f tools/tests/bilibili_live_ui.mk live-ui-preview
python3 tools/tests/bilibili_live_ui.py
```

This separately linked preview mocks only HTTP and tests bookmark add/remove,
parent/sub-area selection, refresh retaining the filter, followed-live navigation,
confirmed login expiry and retaining login on network failure. Its fixture
transport is never part of the Wii U or normal desktop executable.

Desktop/build evidence is stored with the version's package. The user reported
normal playback on Wii U for v0.3.0-alpha.1; see HARDWARE-VALIDATION.md for the
exact package. Real-account followed-live behavior, performance, long sessions, disconnect
recovery, and HOME-close behavior still require device testing. The previously
reported HOME-close hang is not addressed by this feature.
