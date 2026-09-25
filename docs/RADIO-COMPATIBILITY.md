# Choosing and connecting a radio

The ESP32-S3 stores bulletins. The nearby radio handles LoRa, regional settings,
encryption and routing. Radios use their firmware's common Bluetooth API; a new
BBS protocol is not needed for each manufacturer.

## Model coverage

The [catalog](RADIO-MODELS.md) includes **87 MeshCore Companion BLE build targets**
and **118 Meshtastic hardware records**, with a source for each. Its
[machine-readable version](radio-catalog.json) is included in the app. It covers
the cited upstream manifests, not every custom board, fork or future model.

| Family | Connection requirements and distinctions |
| --- | --- |
| Heltec LoRa32 V2, V3/V3.1, V4 and V4-R8 | Match the upstream radio image. V4 and V4-R8 differ. LoRa32 V2 is not MeshTower V2. Keep V3 close during pairing; its Bluetooth antenna can limit range. |
| Heltec MeshTower V2, MeshSolar, T114, T096, T1, MeshPocket | nRF-based products have distinct builds and display options. Screenless setups need their configured PIN. The catalog preserves upstream active/inactive status. |
| Heltec Wireless Tracker/Paper, CT62, RC32, E213 and other variants | Revisions and display-less builds matter. A shared BLE API does not make radio firmware images interchangeable. |
| LILYGO T-Beam / Supreme / 1W, T3-S3, T-LoRa | Match SX1262/SX1276 and board revision when flashing the radio. These differences are handled inside the radio. |
| LILYGO T-Echo / Lite / Card, T-Impulse Plus | nRF variants share the companion API but use different board firmware. Verify display and PIN settings. |
| LILYGO T-Deck, Pager and standalone UIs | A built-in UI may occupy the client API. Meshtastic can require Bluetooth programming mode or BaseUI. MeshCore needs a compatible Companion BLE build. MeshOS is not automatically certified. |
| RAK4631 / WisBlock, RAK3401, RAK3112, WisMesh | Product variants may share a hardware ID. The app reports the family rather than inventing an exact enclosure. RAK11310/RP2040 and STM32-only devices are outside this BLE connection. |
| Seeed T1000-E, XIAO kits, Wio Tracker L1, SenseCAP Solar | Use a BLE-capable CPU and exact radio image. XIAO RP2040 and Wio-E5 are not interchangeable with nRF/ESP32 versions. |
| Elecrow, Ebyte, GAT, Station, Nano, muzi, Ikoka, community boards | Refer to the exact catalog entry. Source-defined targets and downloadable stable BLE assets are recorded separately. |

Discovery uses expected BLE services and recognizable names. Names are editable.
MeshCore uses Nordic UART, which other products also use; a scan result is a
candidate until the companion handshake succeeds. Model details come from
Meshtastic `DeviceMetadata.hw_model` or MeshCore's device-info string after pairing.
Some devices report a family or share an ID. Bluetooth addresses cannot reliably
identify the exact model.

## Displayed or fixed PIN

Meshtastic's `RANDOM_PIN`, `FIXED_PIN` and `NO_PIN` are configuration choices.
Display detection influences defaults, but removing a display does not necessarily
change an existing setting. An unchanged screenless setup commonly uses `123456`;
a custom PIN takes priority.
[Meshtastic Bluetooth settings](https://meshtastic.org/docs/configuration/radio/bluetooth/)

MeshCore v1.17.1 generates a PIN during startup when no PIN is saved, a display is
detected and the build uses the normal default. Without that display it uses the
build default. A saved PIN overrides both. That active code is passed to Bluetooth;
it is not necessarily regenerated for every connection.
[Pinned initialization](https://github.com/meshcore-dev/MeshCore/blob/d92964352441e53b93e8667b802e04f6e072b39e/examples/companion_radio/MyMesh.cpp)

Choosing a radio starts Bluetooth security. MESHBBS asks for a PIN only when the
radio requests passkey input. It does not silently try default PINs. Fixed PINs
follow the same flow. **No PIN** is a separate Meshtastic option, not a synonym
for a radio without a display.

## Quirks that affect connections

- Disconnect the radio's other client so it can advertise and accept MESHBBS.
- LoRa range does not extend Bluetooth. Keep the BBS physically close to its radio.
- On ESP32 Meshtastic radios, enabled Wi-Fi can disable Bluetooth. This is not the
  same issue on nRF52 products, which do not have Wi-Fi.
- Power-saving settings may disable Bluetooth. An unattended solar setup must
  leave its client interface available for continuous BBS service.
- After clearing radio bonds or changing security settings, use Connect again.
- For Meshtastic MUI, use the documented Bluetooth programming mode or BaseUI if
  the standalone UI occupies the connection.
  [MUI connection modes](https://meshtastic.org/docs/configuration/device-uis/meshtasticui/)
- MeshCore repeater, room-server and USB-only builds cannot provide this Companion
  BLE link. Configure the radio and contacts in the native app, then disconnect it.
  [MeshCore roles and troubleshooting](https://docs.meshcore.io/faq/)
- MeshCore handles paths/flooding internally. The new app hides Meshtastic's
  per-message hop selector for MeshCore. The older app must leave it at 3.

MeshCore protocol v2 keeps a 160-byte direct message inside a 173-byte ATT value
when an ESP32 companion negotiates MTU 176. Standard security, service discovery,
notification subscription and serialized commands handle different CPU families.
These are source-backed design choices, not physical certification.

Earlier owner-reported Meshtastic results include a Heltec V3 and real BBS replies.
This release has host tests and compiled images. MeshCore radios and additional
board layouts still need physical testing. Report exact model/revision, radio
firmware, BBS profile and connection stage when contributing results; omit PINs.
