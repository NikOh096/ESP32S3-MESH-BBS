# Third-party notices

MESHBBS source, app, artwork and documentation are GPL-3.0-only unless a file
carries an upstream notice. Keep those notices. See [LICENSE](LICENSE).

| Component | Version / source | License |
| --- | --- | --- |
| Meshtastic generated protocol sources | Firmware 2.7.26.54e0d8d; protobuf commit 6b1ded439633cd03d4af85b44231b91d1d106278 | [GPL-3.0](components/meshtastic/LICENSE) |
| nanopb runtime | 0.4.9.1 | [zlib](components/nanopb/LICENSE.txt) |
| cJSON | 1.7.19 | [MIT](components/cjson/LICENSE) |
| ESP-IDF and bundled components | 6.1 | Apache-2.0 and component-specific licenses; [upstream](https://github.com/espressif/esp-idf/tree/v6.1) and release notices |
| Meshtastic hardware catalog | Commit in docs/upstream-lock.json | [GPL-3.0](https://github.com/meshtastic/web-flasher/blob/3a8aaa32135ebad32b5d73a9c0f510fda4b7ecb3/LICENSE) |
| MeshCore protocol/build definitions used as references | companion-v1.17.1 | [MIT](https://github.com/meshcore-dev/MeshCore/blob/d92964352441e53b93e8667b802e04f6e072b39e/license.txt) |
| esptool-js | 0.7.0 | Apache-2.0; Espressif Systems |
| js-md5 | 0.8.3 | MIT; Chen, Yi-Cyuan |
| pako | 2.2.0, esptool-js dependency | MIT and zlib |
| atob-lite | 2.0.0, esptool-js dependency | MIT |
| esbuild | 0.25.12, build tool only | MIT |

The installer's [notice file](flasher/public/THIRD-PARTY-NOTICES.txt) includes
complete license texts for bundled browser dependencies. Flash packages carry
applicable SDK license texts. ESP-IDF is a separate dependency; obtain the pinned
release and its submodules to build the firmware.

Meshtastic and MeshCore names identify interoperable protocols. This independent
project makes no claim of certification or endorsement.
