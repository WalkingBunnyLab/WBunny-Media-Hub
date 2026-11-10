# Local version storage

This is an independent local Git repository on `main`, with no push remote.
The author `WBunny Media Hub local snapshots <local-snapshot@localhost>` labels
local checkpoints, not the upstream author. Existing copyright notices remain.

- `baseline/coffeeflix-bilibili-20261026`: imported local CoffeeFlix browser source,
  before this fork's identity and slim-mode changes. Source provenance and hashes
  are recorded in `IMPORT_MANIFEST.json`.
- `v0.1.0-alpha.1`: first independent Bilibili-only build. A tag identifies source;
  hardware evidence is recorded separately for the exact package.
- `VERSION`: application version; increment it for a new deliverable.
- `v0.4.0-alpha.1`: user-supplied icon/launch art and native integration of the
  revised whole-app Figma style, browsing layout and player control overlays.
- `v0.3.0-alpha.2`: move Live to its own left-navigation entry beside Bilibili,
  including independent visibility and startup-page preferences.
- `v0.3.0-alpha.1`: Bilibili Live lobby, area filtering, saved rooms, followed-live
  entry and fresh H.264 live playback; device validation recorded separately.
- `v0.2.0-alpha.1`: QR accounts, local/account favorites and history, local resume.
- `checkpoint/bilibili-recommendations-20261026`: source checkpoint saved while
  the build server was unavailable.
- `v0.2.0-alpha.2`: signed-in Recommendations-first navigation and Popular fallback
  after logout or confirmed login expiry. Build evidence and hardware-test status
  are stored with the exact package under `artifacts/0.2.0-alpha.2/`.
- `artifacts/<version>/`: WUHB, installation ZIP, source archive, build/test logs,
  build record, manifest and checksums. These generated files are ignored by Git.
- `backups/`: verified Git bundles with history/tags and separate SHA-256 files.

Before a new version:

```sh
git status
# Review changes, including any newly added files. Never add credentials or device settings.
git add <reviewed-paths>
git commit -m "Describe the change"
# Build/test this commit, then label it after checks pass:
git tag -a v0.1.0-alpha.1 -m "Local Bilibili alpha; see exact-package validation record"
python tools/backup-version.py --tag v0.1.0-alpha.1
```

The tag above is the first-version example; use the next version for later work.
Build records must bind the clean source commit to the WUHB SHA-256. Package with:

```sh
python tools/package.py --wuhb artifacts/0.1.0-alpha.1/wbunny-media-hub.wuhb \
  --record artifacts/0.1.0-alpha.1/build-record.json
```

Both scripts refuse to overwrite their existing version output. To check an
offline history backup or restore it into a *new* directory:

```sh
git bundle verify backups/<version>.bundle
git clone backups/<version>.bundle restored-wbunny-media-hub
```

A same-disk bundle protects against accidental repository edits, not disk loss.
Keep the app ZIP, manifest/checksums and bundle together if making an external
backup. No network storage, remote repository or public release is configured.

The original `CoffeeFlix` directory and its working-tree changes are preserved.
Its earlier tested app remains a separate fallback; the new app installs to a
different SD directory and does not automatically import upstream credentials.
