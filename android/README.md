# MESHBBS for Android

Version 0.5.0, Android 8+. The app uses Android framework Bluetooth APIs, with no
third-party runtime libraries. See [building](../docs/BUILDING.md) for the SDK and
Java versions. Install an update without uninstalling to keep the owner credential.

Home contains threads and recent activity. Devices connects MESHBBS and its radio.
Settings holds rules, bans and notification choices. Activity contains the console.
The radio list is scanned by MESHBBS; tapping a result begins its pairing request.
The phone enters the radio's displayed or fixed PIN only when the radio asks for it.

The same app supports Meshtastic and MeshCore BBS images. Protocol and model data
come from the connected firmware; a radio's display name alone is not authoritative.
The included compatibility guide distinguishes documented candidates from tests.
Old 0.4.1 firmware retains a Meshtastic fallback. See the owner protocol document
for optional status fields and backward compatibility.

First enrollment requires the physical BOOT sequence. Later connections use an
owner HMAC over encrypted BLE. Android Keystore protects the saved secret. Clearing
app data or reinstalling can require physical re-enrollment. Signing keys stay in
.local and are never distributed.

Grant Nearby devices on Android 12+, or Location and enabled location services for
scanning on Android 8–11. Notifications require permission on Android 13+. Optional
background monitoring uses an ongoing connection notification. Sound, vibration,
silent and muted choices remain subject to Android's system settings and Bluetooth
range. No Internet push service or account is used.

Java checks cover protocol behavior; actual phone UI, permission, pairing and
notification acceptance remains a separate step. See [testing](../docs/TESTING.md).
