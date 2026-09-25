# MESHBBS for MeshCore

Separate firmware for the **BBS ESP32-S3**, connected to a MeshCore **Companion BLE**
radio. Target: companion-v1.17.1, commit `d92964352441e53b93e8667b802e04f6e072b39e`.
This is a preview pending physical radio testing. Do not flash it onto the radio.

Threads, rules, moderation, expiry, notifications, owner authentication and LED
behavior share the Meshtastic BBS engine. The same owner BLE service and JSON API
allow app 0.4.1 to work. That older app retains Meshtastic wording and must leave
hops at 3. **App 0.5.0 is recommended** for protocol-aware labels, radio metadata
and fixed/displayed PIN guidance.

## Transport

Nordic UART service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`; RX `...0002` writes,
TX `...0003` notifications. Each ATT value is one companion frame. After security,
MTU negotiation and subscription, the adapter requests self/device/time/contacts,
then pulls queued messages. One command is outstanding at a time; asynchronous
message and ACK events are handled independently.

Protocol version 2 keeps a full 160-byte DM within 173 ATT bytes on ESP32 companions
with MTU 176. The parser also supports v3 DMs. Only plain private text becomes a
BBS command; channels, CLI commands and room relays are ignored. Malformed/oversize
frames are rejected. Command deadlines recover stalled connections.

Replies use at most 160 bytes and five-second spacing. Radio acceptance and
recipient acknowledgment are logged separately. Unconfirmed sends are not silently
repeated. The radio handles paths and floods; BBS does not change LoRa settings.

## Identities and storage

MeshCore public keys have 32 bytes. To keep older owner-app fields compatible, BBS
persists an eight-hex-digit local alias backed by the full key. Alias collisions
do not overwrite identities; ambiguous six-byte contact prefixes are refused.
The registry holds 512 lifetime identities including the local radio. Full or
unreadable storage refuses new identities.

Aliases are **local BBS identifiers, not native MeshCore addresses**. Ban people
from their comments; do not paste a public key into the older eight-digit ban field.
Configure visitor contacts in MeshCore before unattended use. The BBS reads contacts
but does not add every unknown advertisement automatically.

Radio selection, threads, moderation and diagnostics use MeshCore-specific NVS
namespaces. Owner enrollment is shared across variants. Switching keeps both
stores but does not merge them. The app sets UTC; the companion clock can initialize
it at startup. Visitor timestamps never set the BBS clock. Timed operations wait
when no valid clock is available.

Build with `scripts/build-meshcore.ps1`; test with `scripts/test-meshcore.ps1`.
Upstream code was inspected as protocol evidence; MeshCore radio firmware is not
bundled. See [dependencies](../docs/DEPENDENCIES.md) and [validation](../docs/VALIDATION.md).
