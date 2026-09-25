# Validation: 0.5.0 preview

The release separates software checks from hardware acceptance. A successful
build or an upstream model listing does not certify a radio.

## Software checks

- Meshtastic host suite: protobuf addressing, queue acceptance and ACKs, persistent
  threads and counts, expiry, FIFO promotion, bans, rules and 2,000 malformed inputs.
- MeshCore host suite: pinned wire layouts, 160-byte boundaries, 20,000 malformed
  frames, persistent identity aliases and collision rejection; shared thread,
  queue, rule and ban checks at the smaller payload size.
- Discovery checks: advertisement/scan-response merge, service precedence, both
  protocols, stale entries, connectability and invalid text.
- Android 0.5.0: compile/package/signature verification, owner HMAC, codec/discovery,
  attempt-scoped PIN states, protocol detection and radio guidance checks.
- Installer: wrong chip, wrong flash size, missing PSRAM, invalid offsets, unsafe
  download paths, truncated downloads and SHA-256 mismatch rejection.
- Local browser preview: page rendering, profile selection, MeshCore preview
  feedback and the board-match gate were checked; no browser errors were reported.
- Firmware targets: N16R8 and N8R8, each with GPIO48 or GPIO38, for both protocols.
  Release preparation validates each image header against its saved configuration.

## Earlier physical results

The original N16R8 GPIO48 board ran the Meshtastic BBS. Earlier PC Bluetooth checks
verified protected owner access, rules operations and a real Heltec V3 passkey
request while the owner connection stayed usable. The owner subsequently reported
working pairing and message replies and supplied a screenshot of a retrieved thread
with a comment. The owner also confirmed the RGB status behavior.

These observations concern earlier firmware. No MeshCore radio was available for
physical testing during this release, and additional memory/LED layouts have not
been tested on hardware. The new installer has not yet performed a physical flash.
The new Android UI, permission flows, notifications and background behavior need
phone acceptance testing. Host tests do not establish those results.

Follow [TESTING.md](TESTING.md) and record model, board revision, firmware, pairing
mode and outcome. Publish only observed results as tested support. Do not include
PINs, owner keys, private radio addresses or flash dumps in a public report.
