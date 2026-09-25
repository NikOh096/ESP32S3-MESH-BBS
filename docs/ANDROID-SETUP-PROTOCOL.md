# Owner Bluetooth protocol (0.5.0; compatible with 0.4.1)

One NimBLE host supports two simultaneous connections: a peripheral link from the
owner phone and a central link to the selected mesh radio. Opening physical
owner enrollment pauses the radio connection; normal owner access does not.

Service: 8f3a0001-6ca2-4d15-9d49-75b974ad2716.
INFO: 8f3a0002-6ca2-4d15-9d49-75b974ad2716, encrypted read.
RPC: 8f3a0006-6ca2-4d15-9d49-75b974ad2716, encrypted read/write.
JSON UTF-8 requests/responses are at most 512 bytes; the preferred ATT MTU is 517.
Old binary setup characteristics are no longer administrative endpoints.

INFO returns v=2, id (12 uppercase hex digits), name, enroll, owned, and a fresh
32-byte nonce as 64 hexadecimal digits. Board IDs derive from the hardware BLE
address. Each RPC supplies a positive integer id and an op. Write with response,
then read until that id is returned without busy=true. Serialize operations.

## Enrollment and authentication

Three short BOOT presses (50-700 ms each) followed by a three-second hold/release
within twelve seconds opens enrollment for three minutes. RST is not part of it.
Initial BLE bonding uses encrypted Just Works in this physical window. The
app generates a random 32-byte owner secret and sends enroll with proof=secret hex.
The board stores the secret and bonded peer identity and closes the window.
Replacing the owner requires the physical sequence and revokes the previous
owner's key and bond, preserving node settings, bulletins and other bonds.

Each later connection requires auth with proof equal to:

    hex(HMAC-SHA256(owner_key, nonce_bytes || ASCII(board_id)))

The nonce rotates after every authentication/enrollment attempt. Owner identity,
BLE encryption and valid proof are all required before administrative RPCs.
Android stores the secret encrypted with an Android Keystore AES-GCM key; backup
is disabled. No PIN/key is logged. A wrong proof or replay revokes session access.

Outside enrollment the name/service UUID are omitted from advertisements and
scan/connect requests are filtered to the saved peer identity. The app remembers
the BLE address for reconnection. Bluetooth emissions remain observable; it is
not possible to make a radio physically invisible to all other applications.
Initial Just Works enrollment is not authenticated against a nearby active MITM;
the physical enrollment window must be controlled by the owner. Physical flash
extraction is outside this application's protection; flash encryption is not enabled.
USB accepts read-only STATUS and ACTIVITY, never enrollment or configuration.

## RPC operations

status, clock(time Unix seconds), configure(target,hops,no_pin), pair, pin(pin six
digits, attempt from node_status); list(slot 1-9), read(slot,generation,index=-1 for root or >=0 for comment);
create(slot,title,text,expires), delete(slot,generation), expiry(slot,generation,
expires), seen(slot,generation,count viewed); activity; queue_list(index),
queue_delete(node,packet); ban_list(index), ban_add(node hex), ban_remove(node hex),
ban_message(text up to 120 UTF-8 bytes); rules_get(index 0-5), rules_set(index,text
up to 160 UTF-8 bytes). Empty rule text omits it; at least one rule must remain.

Generation is the immutable numeric backing value of a public ID, e.g. 1=A000001
and 1000000=B000000. Mutations resolve by this ID even if list position has moved.
Create uses the next free storage slot; list slots are display positions only.
Read acknowledgments include the number of comments viewed, preserving later unread
comments. Queue removal uses sender+packet identity rather than a changing position.
Only public creation and comments are available over LoRa.

## Node discovery and pairing

node_scan starts a 12-second active scan on MESHBBS, preserving an established radio
link. node_scan_results(index 0-31) returns count/scanning/code plus address, name,
rssi, connectable for that entry. Advertisement and scan-response data are merged.
Nodes are identified by Meshtastic or MeshCore service UUIDs, with a name-prefix
fallback. Results add protocol and service fields; a service match takes priority.
A candidate for the other protocol is shown but not selectable in this image.
Only connectable entries seen within two minutes can be selected with node_select.

node_select(target observed address,hops,no_pin) saves selection and starts one
pairing attempt. configure accepts an exact name/address for manual fallback and
also starts pairing in 0.4.1. pair retries the saved selection. pair_cancel cancels
without removing any saved bonds, owner key or bulletin data. An optional attempt
on cancellation rejects a stale dialog cancelling a later attempt.

node_status returns stage, attempt (nonzero uint32), code, target and api. Stages:
idle, searching, connecting, pairing, enter_pin, securing, discovering, ready,
failed. A ready BLE service is not yet BBS-ready until api=true. The Android client
polls at 500ms during explicit pairing and prioritizes PIN submission over other RPCs.

After connection, MESHBBS initiates standard SMP security with keyboard input
capability, before MTU/GATT discovery. A display radio may show a generated six-digit PIN; other configurations
use a saved fixed PIN. The owner must supply the active PIN for that radio. Only when node_status.stage=enter_pin may the owner send pin with
six ASCII digits and that attempt token. Tokens are checked again on the BLE host
queue to reject stale PINs. A security timeout requires retrying and using the new
code. No PIN or owner secret is logged. Saved bonds support unattended reconnect.
Explicit No PIN skips bonding only for radios configured for that mode.

This follows the official app's select -> request bond -> await bond -> open radio
service sequence, with MESHBBS as the radio's bonded client. Bonding the phone to
the radio would store keys on the phone, not on MESHBBS. The owner-phone connection
is retained throughout node pairing. The reference was reviewed, not copied wholesale:

- [AndroidBluetoothRepository.bond](https://github.com/meshtastic/Meshtastic-Android/blob/d003a216a6f6a84e5a7a4a26fc1e9bcb41ae1395/core/ble/src/androidMain/kotlin/org/meshtastic/core/ble/AndroidBluetoothRepository.kt)
- [AndroidScannerViewModel.requestBonding](https://github.com/meshtastic/Meshtastic-Android/blob/d003a216a6f6a84e5a7a4a26fc1e9bcb41ae1395/feature/connections/src/androidMain/kotlin/org/meshtastic/feature/connections/AndroidScannerViewModel.kt)

See scripts/check-owner.py for the PC regression. Its temporary owner key lives
only in .local/pc-owner.json and is excluded from packages. Physical enrollment is
required to replace that test owner with the actual Android phone.

## Additive fields in 0.5.0

node_status adds protocol (Meshtastic/MeshCore), hw (Meshtastic hardware enum,
zero when unknown), hardware (reported model text, possibly empty) and pin_mode
(display/fixed/none/unknown). These are hints, never instructions to prefill a PIN.
Unknown metadata stays unknown. The app falls back to the firmware string or
Meshtastic for older images. node_scan_results adds protocol and service (whether
the protocol was observed in service data rather than inferred from a name).

MeshCore retains the existing owner RPC contract and uses separate settings,
bulletin, diagnostic and identity namespaces. Its old hops field accepts 3 as
a compatibility placeholder; the radio manages MeshCore paths. Its 32-bit node
fields are persistent local aliases for full MeshCore public keys, not network
addresses. App 0.4.1 can use the existing contract, with older Meshtastic labels;
0.5.0 identifies the protocol and hides inapplicable options.
