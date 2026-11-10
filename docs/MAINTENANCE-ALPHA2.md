# 0.4.0-alpha.2 maintenance

- Internal intro now displays the supplied launch artwork and fades into the UI.
  The clock screensaver uses the supplied bunny icon. Upstream legal credits remain.
- Settings > Bilibili > Account opens the existing QR/account screen. Signed-in
  rows show the current profile account name and UID; account logout remains there.
- Language choices and packaged catalogs are limited to English, Japanese and
  Simplified Chinese. Existing unsupported saved languages migrate to the supported
  system language, or English. An unset preference still follows the console.
- A remote live EOF now enters the existing bounded reconnection path instead of
  marking playback finished. Each restart refreshes playback URLs and rotates
  secure candidates, keeping HTTP fallbacks behind HTTPS candidates. Normal video
  EOF still finishes. Room-offline responses remain errors rather than retry loops.

The supplied device logs show TLS resets, playlist reload failures and CDN DNS
errors preceding unexpected Finished events. They do not establish insufficient
cache capacity. This change addresses the missing live EOF recovery path; it
cannot guarantee recovery while the network or all Bilibili CDNs are unavailable.

Validation targets: service protocol/cookie isolation and restart rotation; three
language selection and legacy migration; native screenshots of intro/screensaver,
account rows and QR navigation; real HTTP finite-response live versus VOD EOF;
existing Live UI/player previews; Wii U cross compilation. Exact results and logs
are recorded in artifacts/0.4.0-alpha.2/build-record.json after execution.

Wii U device validation is pending. The previously reported HOME-close hang is
outside this maintenance change and remains unverified.
