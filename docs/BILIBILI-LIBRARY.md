# Bilibili library — 0.2.0-alpha.2

Recommendations: signed-out entry opens Popular with no Recommendations tab.
After sign-in, Recommendations is the first tab and the default feed. Popular
remains selectable. Returning from a video preserves the selected feed. Signing
out switches the browser back to Popular and cancels old account requests.
The recommendation adapter validates the captured account with authenticated
nav, signs the feed request with WBI, paginates with fresh_idx/fresh_type, and
keeps ordinary video cards only. An explicit expired-login response clears the
local session and returns to Popular. Network, verification and malformed-response
errors preserve the login and offer retry; they do not silently substitute Popular.
Account-dependent completions are discarded after switching account/feed.
Recommendation parameters follow wiliwili's `source/api/home_api.cpp`.

The Bilibili screen retains the existing search, popular feed, BV input and
CoffeeFlix player. Its second action row opens Account, Favorites and History.

- Account: scan the QR code using the Bilibili mobile app and confirm there.
  Waiting, confirmation, expiry and failure have separate states. Request a new
  code after expiry. Logout removes this profile's saved session locally.
- Favorites: starts with this console's favorites. Choose Bilibili account to
  browse your own folders and their videos, with explicit pagination. In video
  details, toggle the local favorite or choose Account favorites to add/remove
  the video in an existing account folder. No favorite is changed just by opening
  a video. Create folders in Bilibili's own app first.
- History: starts with local playback history (up to 200 videos, including finished
  videos), recorded after actual playback progress. Choose Bilibili account to
  read the account's existing video history. Account history is read-only in this
  version: local playback is not automatically reported to Bilibili.
- Resume: playback uses the player's existing per-part resume storage. Select
  the corresponding part to continue. Home's Continue watching opens its video
  details. Account history does not import remote playback positions yet.
- On a local library card, X removes that local favorite/history entry. This does
  not delete account history or remove the saved playback position.

Authentication is saved in the existing settings store under the active profile
and console user's identity. That JSON contains login cookies: keep the SD-card
settings private. It is excluded from source archives, Git and app packages.
Account requests enforce certificate verification and use private URL logging.
Cookie values are not attached to media/CDN requests. No account cookie export,
background account switching or automatic session refresh is provided.

API patterns were checked against wiliwili's `source/api/mine_api.cpp` and
`include/api/bilibili/api.h` (https://github.com/xfangfang/wiliwili). This implements
an adapter around this project's HTTP, tasks, store and existing UI widgets; it
does not import wiliwili's UI or runtime. The web QR passport endpoints, favorite
folders/resources and history cursor API may return verification or expired-login
errors; these are displayed rather than treated as empty successful responses.

Offline checks: `make -f tools/tests/bilibili_protocol.mk test account-test`.
Read-only live checks: build `live account-live`, then run the probes. A live
unclaimed QR proves generation/polling only; successful account access and
favorite writes require a device-side QR confirmation and remain hardware tests.
