# Dependency provenance

| Item | Pinned source |
| --- | --- |
| Meshtastic firmware reference | https://github.com/meshtastic/firmware/tree/v2.7.26.54e0d8d |
| Firmware commit | 54e0d8d0ab2ff56b3a9ce967e53f79e49af560fb |
| Protobuf submodule commit | 6b1ded439633cd03d4af85b44231b91d1d106278 |
| Protobuf repository | https://github.com/meshtastic/protobufs |
| nanopb | https://github.com/nanopb/nanopb/tree/0.4.9.1 |
| nanopb commit | cad3c18ef15a663e30e3e43e3a752b66378adec1 |
| ESP-IDF | Locally installed v6.1 |

components/meshtastic contains the needed dependency closure of generated nanopb
files copied from that exact firmware commit. Original .pb.cpp files contain C;
their extensions were changed to .pb.c for this C project. Their contents are
otherwise unchanged. The corresponding GPL-3.0 license is included.

components/nanopb contains unmodified runtime files and LICENSE.txt from 0.4.9.1.
The generator is not needed during ordinary builds.

The private .reference directory contains downloaded inspection copies and is
excluded from Git. No user credentials or Bluetooth PIN are vendored.

Android setup app uses Android framework APIs only, with no third-party runtime
dependencies. Built with installed Android platform android-36.1, build tools
36.1.0 and Android Studio's Java 21 runtime. Source, manifest, resources and a
command-line build script are in android/.

Reference documentation used for the setup app and friend instructions:

- https://developer.android.com/develop/connectivity/bluetooth/bt-permissions
- https://developer.android.com/reference/android/bluetooth/BluetoothGatt
- https://developer.android.com/tools
- https://espressif.github.io/esptool-js/
- https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html


## Owner protocol and JSON

cJSON runtime sources and MIT license are vendored from official v1.7.19:
https://github.com/DaveGamble/cJSON/tree/v1.7.19
The component sets CJSON_NESTING_LIMIT=16. ESP-IDF's PSA crypto provides HMAC-SHA256.

- https://developer.android.com/privacy-and-security/keystore
- https://developer.android.com/develop/background-work/services/fgs/service-types#connected-device
- https://developer.android.com/develop/ui/views/notifications/channels
- https://mynewt.apache.org/latest/network/ble_hs/ble_gap.html

## MeshCore and browser installer

The MeshCore adapter is an independent implementation of the companion protocol
from companion-v1.17.1, commit d92964352441e53b93e8667b802e04f6e072b39e. Its model
catalog comes from the same pinned build definitions and release asset list.
Meshtastic hardware metadata is pinned separately in upstream-lock.json. Original
licenses are retained under licenses/. The ESP-IDF dependency is v6.1 with its
required submodules; all SDK license/notice files are retained in the release.

Browser runtime dependencies are esptool-js 0.7.0, js-md5 0.8.3, pako 2.2.0 and
atob-lite 2.0.0. esbuild 0.25.12 is a build tool only. flasher/pnpm-lock.yaml records
resolved versions and package integrity. Runtime scripts are bundled locally;
the installer does not load remote CDN code. See THIRD-PARTY-NOTICES.md at the
repository root and flasher/public/THIRD-PARTY-NOTICES.txt for license texts.
