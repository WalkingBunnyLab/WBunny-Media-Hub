# Project direction

Current project: **WBunny Media Hub**, an independent local repository with
Bilibili-only navigation in `0.1.0-alpha.1`. The source/prototype discussion below
is retained as design history; naming and data-path separation have now been
implemented. See the root README and VERSIONING.md for the current workflow.

## User-defined product scope

Build a Wii U media application similar in scope to CoffeeFlix, beginning with
Bilibili as the only supported content source. Add Jellyfin, Plex, LAN media
and local playback in later phases. The initial use is local/private. Product
name and branding have not been selected.

The user subsequently clarified that the overall layout and playback interaction
should remain those of CoffeeFlix. Retain its navigation structure, page layout
conventions, card/list behavior, focus/scroll/back behavior, playback controls,
and TV/GamePad interaction. The current scope is content-source integration and
service-specific pages, not a UI redesign. Adapt labels, branding and controls
only where required by the selected service or the eventual application identity.

The current CoffeeFlix-based Bilibili prototype is the starting point. The user
has reported working Wii U browsing and playback, and working fast-forward and
scrubbing in the preceding playback prototype. Preserve these working builds
as reference points; do not infer that untested features or future builds pass.

## Architecture direction

Retain and progressively adapt the working CoffeeFlix platform, rendering/input,
task, networking, storage and playback foundations. Use the existing
`player::Source` resolution boundary for content sources. Keep browsing,
authentication and service-specific behavior in separate service/page modules;
the shared player should not acquire assumptions about Bilibili accounts,
BV IDs or API response formats.

Common application behavior includes navigation, settings, playback controls,
resume/history presentation and GamePad/TV behavior. Service-specific behavior
includes Bilibili parts, UP pages, account actions and danmaku, and later server
libraries, connections and metadata. Share card/list widgets and playback
contracts where they already fit. Introduce common provider interfaces only
when concrete features need them; do not force unlike services into a single
large API or add a plugin-loading system for the first version.

Use wiliwili as a reference for Bilibili API behavior and service-specific
features. CoffeeFlix remains the reference for overall layout and playback
interaction; wiliwili's UI is not the target. This direction does not require a full port of its
Borealis/libmpv/platform stack to Wii U. Keep the current hard-decoding and
platform integrations that have already worked on the device.

Preserve the existing Jellyfin, local-media and LAN implementations as future
integration references. Hide services outside the first milestone from the
normal product UI rather than deleting useful implementations now. Evaluate
Plex separately when its phase starts; its integration has not been implemented
or verified by this prototype.

## Implementation sequence

1. **Establish the application structure.** Give Bilibili its own service and
   section identity; migrate legacy `youtube` navigation preferences without
   erasing existing data. Make the initial navigation, home, global search and
   settings consistently Bilibili-focused. Separate app identity/data paths
   from upstream CoffeeFlix when product naming is decided. Preserve the tested
   playback behavior and existing future-provider code during this work.
2. **Finish the Bilibili milestone.** Make browsing/search/details/parts and
   error/retry behavior reliable, add local history and resume, then account
   login and the required account features. Add further interaction features
   such as UP pages, favorites, comments and danmaku incrementally, with device
   validation for each meaningful playback or performance change.
3. **Add the later media sources.** Restore/adapt Jellyfin, add Plex, and adapt
   LAN and local-media playback as independent integrations. The user has named
   these future sources but has not fixed their detailed implementation order
   or requirements.

The first milestone is a usable Bilibili application, not simultaneous delivery
of all future services. The current prototype remains an experimental build,
not evidence that account features, subtitles, all codecs or all media sources
already work.

## Current decision record

Recorded from the user's clarification on 2026-10-26. This is a scope and
architecture record; it does not itself rename the application, migrate saved
settings, change code, create a release, or authorize publication. The next
concrete development step is the application-structure work above, followed by
the Bilibili feature milestone.
