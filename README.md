# MESHBBS

An always-on bulletin board for your local mesh. Post a notice, start a conversation,
and catch up later—even when the person who wrote it is offline.

MESHBBS runs on a separate **ESP32-S3 dev board** and connects by Bluetooth to a
nearby radio. Choose the Meshtastic or MeshCore firmware for the BBS board. One
Android app manages either version. No Wi-Fi or Internet is needed while it runs.

**[Install in your browser](https://NikOh096.github.io/ESP32S3-MESH-BBS/)** ·
**[Downloads](https://github.com/NikOh096/ESP32S3-MESH-BBS/releases)** ·
**[Getting started](docs/GETTING-STARTED.md)** · **[Radio guide](docs/RADIO-COMPATIBILITY.md)**

## What you need

- ESP32-S3-WROOM dev board with **N16R8 or N8R8** memory: 16 or 8 MB flash,
  and 8 MB octal PSRAM. Images cover RGB LED GPIO48 and GPIO38 layouts.
- A powered mesh radio within Bluetooth range. It stays on Meshtastic or MeshCore
  **Companion BLE** firmware. Do not put the BBS image on the radio.
- Android 8+ for owner setup. Visitors use their normal mesh app and radio.
- USB data cable and desktop Chrome or Edge for browser installation.

The Meshtastic adapter targets **2.7.26.54e0d8d**. The MeshCore adapter targets
**companion-v1.17.1**, the stable companion release checked September 24, 2026.
The [model catalog](docs/RADIO-MODELS.md) includes 205 upstream model/variant
records. A listing is upstream evidence, not a claim that every radio was tested.
MeshCore and additional board layouts need hardware acceptance testing;
see [validation](docs/VALIDATION.md).

## Start a conversation

Direct-message the radio attached to MESHBBS from another radio on the same
network. A relay does not become the BBS address just because it forwards packets.

| Send | What happens |
| --- | --- |
| `HELP` | Short guide |
| `RULES` | Owner's community rules |
| `UPDATE` | Nine active threads, oldest first |
| `CREATE Trail news \| North trail is muddy.` | Start a 24-hour thread |
| `A000042` | Read that thread and its comments |
| `!A000042 South trail is clear.` | Add a comment |
| `QUEUE` | Waiting positions and next scheduled opening |
| `PING` | Check whether the BBS is responding |

IDs stay fixed. List positions move when threads expire. There are nine active
threads, 32 comments per thread and 32 waiting bulletins. A queued thread's
24 hours starts when published. Owners can choose custom expiry or no expiry.
Ordinary updates preserve posts, IDs, counts and pairing; erasing flash removes them.

Long replies arrive in numbered parts five seconds apart. Let a response finish
before sending another request. MeshCore's complete outgoing command, including
its ID, must fit within 160 UTF-8 bytes.

## Manage it from your phone

**Home** holds bulletins and recent activity. **Devices** connects the board and
radio. **Settings** has rules, bans and notifications. **Activity** shows the
connection and message log when you need it.

For first setup, tap **BOOT three times**, then hold it for **three seconds** and
release. Finish within 12 seconds. Find the board in Devices and choose **Use
this phone**. Find your radio and tap it to connect. Enter its displayed or
configured six-digit PIN when asked.

The phone reconnects automatically while open and nearby. It connects to MESHBBS;
MESHBBS pairs with the radio. Both links can stay open while the BBS runs.
Notifications use nearby Bluetooth, with sound, vibration-only, silent and muted modes.

## Build and contribute

Firmware uses ESP-IDF 6.1. Android uses platform APIs without runtime libraries.
The browser installer uses Espressif's esptool-js.

- [Building](docs/BUILDING.md) and [testing](docs/TESTING.md)
- [Owner Bluetooth protocol](docs/ANDROID-SETUP-PROTOCOL.md)
- [MeshCore adapter](meshcore/README.md)
- [Contributing](CONTRIBUTING.md), [security](SECURITY.md), [privacy](PRIVACY.md)

Licensed under **GPL-3.0**. Third-party components retain their own notices;
see [LICENSE](LICENSE) and [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
This is an independent community project, not an official Meshtastic or MeshCore
product. It does not bridge the two radio networks.
