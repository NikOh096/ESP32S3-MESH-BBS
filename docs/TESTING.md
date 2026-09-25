# Verification checklist: 0.5.0

## Automated on the development PC

- scripts/test.ps1: thread creation/comment/read/delete and expiry; immutable IDs;
  A999999 to B000000 rollover; counts after deletion/reboot; 9-active/32-pending
  FIFO; no duplicate promotion after power interruption between record and queue
  commits; persistent write failures; UTF-8 boundaries and full-thread splitting;
  generation guards; unread arrival during an open view; bans and malformed IDs.
- Transport tests: exact target protobuf vectors, PKI reply addressing, node queue
  and routing ACKs/errors, bounded malformed packets and 2,000 fuzz-like inputs.
- android/build.ps1: Java compile, Android packaging, existing-key signature verify,
  legacy codec/discovery checks and owner HMAC vector/key/nonce/board binding.
- scripts/check-owner.py: encrypted INFO, unauthenticated denial, physical enrollment,
  HMAC/replay/wrong-key tests, owner create/read/expiry/delete, ban add/list/remove,
  and reconnect outside the physical window. --enroll replaces the owner ONLY
  when the BOOT window is open. Its test key stays in .local.

## Physical phone/radio acceptance

1. Install the APK as an update (keep app data). An existing owner should reconnect
   without a BOOT sequence. Use physical enrollment only for first setup or replacement.
2. Disconnect any other Bluetooth client from the chosen Heltec V3 / Meshtastic radio.
   Find nearby radios and tap its result. Verify that this immediately requests pairing,
   enter its displayed PIN and verify both owner and radio API ready simultaneously.
3. From a separate radio DM PING/HELP to the attached radio, then CREATE and UPDATE.
   Read the returned seven-character ID; reply with !ID text. Verify replies and ACKs.
4. Fill nine active slots. Queue two more; check QUEUE position and next expiry.
   Give the oldest thread a short owner expiry. Verify deletion, positions moving,
   unchanged IDs, FIFO publication and a full 24-hour lifetime for the promoted item.
5. Open a thread while another comment arrives; only displayed comments become read.
   Verify red every five seconds until viewed, plus green heartbeat and dim blue states.
6. Test all four notification modes, Android notification permission denial, background
   monitoring on/off, screen off, out-of-range and return-to-range. Android may apply
   battery/background restrictions; actual device behavior must be observed.
7. Ban the visitor ID. Verify its next DM gets the ban response, its queued items are
   removed and its comments/creates are rejected. Unban and verify access returns.
8. Reboot without erasing. Restore the clock through the app. Verify posts, queue,
   bans, IDs and totals persist; expired threads retire before serving commands.
9. Confirm another phone cannot administer the board outside physical enrollment.
   Replacing the owner must revoke the old app while preserving board contents.

No actual Android phone is attached through ADB. Host tests and PC BLE regressions
are not substitutes for these radio/UI/notification acceptance tests.

## Discovery and rules

- Native scan tests: service/name filtering, ADV + SCAN_RSP merge, connectability,
  public/random address types, expired results, invalid UTF-8, bounded cache.
- RULES tests: defaults, edited text, empty-rule policy, no LoRa configuration,
  UTF-8/length/packet bounds. On hardware, edit a rule, reboot and read it again;
  from a second radio DM RULES and verify the saved text appears in numbered replies.
- Android PIN states: prompt only during enter_pin, uint32 attempt ID bounds,
  connection-stage descriptions, distinction between radio service and API ready.
- Cancel PIN then retry; verify the previous dialog/code cannot affect a new attempt.
- Try an out-of-range/busy radio, then rescan/retry. Errors should leave the owner
  link connected and tell the user to retry. Test a bonded reconnect (no fresh PIN)
  and an explicitly No PIN radio if available.

## MeshCore and installer acceptance

Run scripts/test-meshcore.ps1 for wire vectors, key aliases/collisions, malformed
frames and shared BBS tests at 160 bytes. Run pnpm test in flasher for manifest,
chip/memory and integrity rejection checks.

With a Companion BLE radio, test fixed and displayed PIN settings when available,
bonded reconnect, startup contact synchronization, a full-length 160-byte message,
contact changes and recipient ACKs. Test from a second saved contact. Check unknown
and ambiguous identities are rejected; no visitor should inherit another's ban or
thread attribution. Radio and phone links must remain usable together.

For each BBS board profile, verify memory detection, LED wiring, USB recovery,
verified installation and preservation of saved posts after an update. Interrupt
a flash only on a spare board, then recover through BOOT/RST. Wrong chip/memory
choices must stop before writing. Test the page on a narrow viewport, by keyboard
and with browser zoom. New layouts, phone UI and MeshCore radios need physical
acceptance before marking them tested.
