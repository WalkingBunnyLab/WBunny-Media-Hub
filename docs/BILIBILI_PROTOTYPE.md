# Local Bilibili playback prototype

This experiment adds **Settings > Updates and about > Bilibili playback test**.
Enter a BV ID (for example `BV1GJ411x7h7`) or a full
`https://www.bilibili.com/video/BV...` URL, choose a maximum quality, then select
a part to play. Changing the quality affects the next playback. Reload details
to retry a metadata request; re-open the part to obtain fresh media URLs.

The prototype uses CoffeeFlix's existing video player and Wii U H.264 decoder.
It requests anonymous video metadata, obtains WBI signing material from `nav`,
and resolves DASH streams through `/x/player/wbi/playurl`. Only H.264 video and
AAC audio are selected. The available maximums are 360p, 480p and 720p; account
permissions and the server response can reduce the actual quality. The first
tests should use ordinary public landscape videos. The input URL's `p=` is not
used: choose the desired part in the list.

There is no login, recommendation feed, search, danmaku, subtitles, short-link
expansion, or resume/history integration in this version. Before playback, each
track probes up to three supplied CDN addresses with a 1 KB range and a 6-second
timeout per address. This handles unreachable primary addresses; it does not
switch CDN addresses during an already-running playback session.
The last successfully loaded BV ID is saved as `bilibili_test_bvid`; no account
cookies are stored. Signed media URLs are resolved again when opening a part.
API access does not prove Wii U playback, audio synchronization or seeking.

## Build

Use the repository's existing Wii U dependency/build workflow:

```sh
bash tools/build-deps.sh
make -j4 APP_VERSION=2.5.6-bilibili-probe
```

On the tested devkitPPC GCC 15.2.0 image, unchanged `src/player/subtitles.cpp`
triggers an optimized `std::sort` array-bounds diagnostic. The prototype artifact
was completed with the following build-only override; the subtitle code and
Makefile were not changed:

```sh
make -j4 APP_VERSION=2.5.6-bilibili-probe \
  OPTFLAGS='-O2 -g -ffunction-sections -fdata-sections -fomit-frame-pointer -Wno-error=array-bounds'
```

The output remains `coffeeflix.wuhb`, with the original CoffeeFlix application
and settings paths. Preserve the installed app before substituting this test
build. No deployment is performed by these build or test commands.

## Tests

The standalone tests need a C++20 compiler, pkg-config, SDL2 headers, Jansson,
OpenSSL, and (for the live target) libcurl. They use section garbage collection
to link just the service and required core helpers:

```sh
make -f tools/tests/bilibili_protocol.mk test
make -f tools/tests/bilibili_protocol.mk live
./build/bilibili-protocol/live --live BV1GJ411x7h7
```

The offline target replaces HTTP transport with fixtures. It exercises the real
BV parser, signing, metadata/stream parsing, codec/quality selection, source
resolver, signing-key cache and cancellation. The live target uses the production
HTTP client and checks metadata, signed playback resolution, and byte ranges from
both audio and video. It does not decode media. Its only support replacements are
English translation lookup and logging (the app's UI and log writer are not started).

A complete desktop checkout can alternatively use the normal test-link workflow:

```sh
make -f desktop.mk -f tools/tests/bilibili.mk bilibili-tests
./build-desktop/bilibili-test --live BV1GJ411x7h7
```

`COFFEEFLIX_BILIBILI_API` can point metadata and playback API requests at a local
mock server for desktop debugging. Media addresses still come from that response.

On Wii U, verify launch, BV text entry, detail loading, each part, picture and
sound, pause/resume, forward/back seeking, closing playback, and opening another
video. Compare the available quality choices and try a multi-part video. Logs
remain in `sd:/wiiu/apps/coffeeflix/coffeeflix.log` (and `coffeeflix-previous.log`).

## Validation on 2026-10-26

The standalone offline tests passed. The live target, using the production C++
HTTP client, resolved `BV1GJ411x7h7` to 480p H.264/AAC and read byte ranges from
both tracks after CDN fallback was added. The original primary-only probe timed
out, which motivated the fallback. Wii U compilation succeeded using the
build-only diagnostic override above. No hardware test had been performed when
the artifact and its build-info.json were generated.

Following delivery on 2026-10-26, the user reported that playback on Wii U was
normal and that fast-forward and timeline scrubbing worked without problems.
This is user-reported hardware evidence, not an agent-observed device test.
The exact BV ID, selected quality, device/firmware details and installed-file
checksum were not supplied. Pause/resume, multi-part playback and long-session
stability have not been separately confirmed. The original build artifacts and
build-time manifest are retained unchanged.

## Reference

The API flow was checked against wiliwili `yoga` commit
`88e5876bea9502d06f46a8656e3530684d3aaf7d`, especially its `search_api.cpp`,
`video_detail_api.cpp`, `util/wbi.cpp` and DASH result models:
<https://github.com/xfangfang/wiliwili/tree/88e5876bea9502d06f46a8656e3530684d3aaf7d/wiliwili>.
This adapter uses CoffeeFlix's HTTP, Jansson, MD5 and task interfaces; it does not
add wiliwili's CPR, nlohmann/json, Borealis or libmpv dependencies.
