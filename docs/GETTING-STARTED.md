# Set up your bulletin board

## Install

1. Check your ESP32-S3 module: **N16R8** or **N8R8**. Check the RGB LED GPIO too:
   GPIO48 on the tested Hosyond/YD layout and original DevKitC-1; GPIO38 on
   Espressif DevKitC-1 v1.1. Other revisions can vary.
2. Open the [installer](https://NikOh096.github.io/ESP32S3-MESH-BBS/) in desktop
   Chrome or Edge. Select the matching memory/layout and your radio network.
3. Connect the **BBS dev board** with a USB data cable. Choose its port, then
   Install. Wait for the verification and completion message.
4. Install **MESHBBS 0.5.0.apk** on Android. Allow installation from your browser or
   file manager if Android asks. Use the APK from this project's release.

Updating? Install the APK without uninstalling and use the same board profile.
Normal flashing preserves owner pairing and posts. Meshtastic and MeshCore use
separate bulletin stores; switching images does not migrate or merge them.
Leave full flash erase off.

## Connect

1. With MESHBBS running, tap BOOT three times, then hold it for three seconds and
   release. Finish within 12 seconds. Do not press RST for this step.
2. In the app, open **Devices → Find my board**. Tap it, then **Use this phone**.
   The button sequence is for first enrollment or owner replacement.
3. Disconnect the radio's other phone app. Select **Find nearby radios**, then
   tap the radio you want MESHBBS to use.
4. Enter its displayed or configured six-digit PIN when prompted. A screenless
   radio still needs its fixed PIN unless explicitly configured for No PIN.
5. Wait for **Board connected · Radio ready**. Create a welcome bulletin on Home
   and edit your community rules in Settings.

The phone talks to MESHBBS; MESHBBS pairs with the radio. Both links can stay
open. Your phone reconnects while open and nearby; background monitoring is optional.

## Try it

From a second radio, send `PING`, then `HELP`, to the radio attached to MESHBBS.
`UPDATE` lists threads; send a permanent ID to read one. Leave ten seconds between
requests and wait for every numbered reply. For MeshCore, add the visitor and
BBS contacts in the native app before testing.

A delivered radio message does not prove the BBS processed it. Check Activity for
the received command, outgoing reply and acknowledgment. Address the attached
radio, not a relay.

## Share the flash ZIP

Send the release ZIP and this guide, or share the installer link. Each board gets
its own hardware-derived ID. The images contain no owner key, saved radio PIN,
posts or chosen radio address. Your friend enrolls their phone after installation.

For manual flashing, take all three files from the **same profile folder**:

| Address | File |
| --- | --- |
| `0x0` | `bootloader.bin` |
| `0x8000` | `partition-table.bin` |
| `0x10000` | `firmware.bin` |

Use ESP32-S3, DIO, 80 MHz and matching 8 MB or 16 MB flash. Keep full erase off.
SHA256SUMS records file hashes. If the port does not appear, hold BOOT, tap and
release RST, then release BOOT. Try another data cable or socket. After an
interrupted install, repeat this sequence and install again. The bootloader and
owner-enrollment sequences are different.

If you prefer the command line, install Python and esptool, open a terminal in the
chosen profile folder, and replace COM7 below with your board's port:

```sh
python -m pip install esptool==5.3.1
python -m esptool --chip esp32s3 --port COM7 flash-id
python -m esptool --chip esp32s3 --port COM7 --baud 460800 write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 firmware.bin
```

That example is for N16R8. Use the N8R8 folder and `--flash-size 8MB` for N8R8.
Confirm the reported flash size before writing. On Linux/macOS, use the observed
serial device path in place of COM7. Tap RST once if the board does not restart.
