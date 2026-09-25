# Share MESHBBS

Share the [browser installer](https://NikOh096.github.io/ESP32S3-MESH-BBS/) or a
[release ZIP](https://github.com/NikOh096/ESP32S3-MESH-BBS/releases), together with
[Getting started](GETTING-STARTED.md). Choose Meshtastic or MeshCore to match the
radio, then the exact N16R8/N8R8 and GPIO48/GPIO38 BBS board profile.

The ZIP contains three firmware files per profile, the Android app, checksums,
licenses, instructions and matching source. Do not send a backup of your own board:
a flash dump can contain owner keys, bonds, saved settings and messages. Release
images contain application code only. Your friend's board gets its own ID and
requires their phone to enroll through the physical BOOT sequence.

Normal updates retain data. Keep full flash erase off and keep app data when
updating the APK. Firmware belongs on the separate ESP32-S3 BBS board; the radio
keeps its normal mesh firmware.
