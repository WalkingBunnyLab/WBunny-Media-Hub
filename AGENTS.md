# WBunny Media Hub

- This independent Wii U application derives from the locally validated CoffeeFlix
  Bilibili browser. Preserve the upstream license, required notices and credits.
- Retain CoffeeFlix's overall layout, navigation behavior, player controls and
  TV/GamePad interaction. The first milestone exposes Bilibili only.
- Keep the existing player, Wii U platform and dependency patches stable.
  Jellyfin/local/LAN implementations are retained as future integration references;
  Plex has not been implemented. Do not silently enable deferred providers.
- Read the global machine instructions and `~/.codex/machines.md` before remote
  operations. Use the verified Mac mini Docker workflow; verify a new remote
  build directory before transferring source. Never alter the original CoffeeFlix
  checkout or its delivered artifacts as part of this project's builds.
- Source versioning is local Git on `main`, with annotated baseline/release tags.
  No upstream update feed, supporter feed, log upload, publishing workflow or remote
  push is configured. Do not publish or add a push remote without user instruction.
- `VERSION` is the application version. Installation and settings live under
  `wiiu/apps/wbunny-media-hub`, independently from CoffeeFlix.
- Store generated bundles/packages/logs under ignored `artifacts/` and local Git
  backups under ignored `backups/`. See `docs/VERSIONING.md`.
- A successful build or desktop preview does not imply Wii U hardware success.
  Preserve exact tested package bytes and record user-reported feedback separately.
