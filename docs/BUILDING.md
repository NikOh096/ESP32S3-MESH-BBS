# Build MESHBBS

Use the release binaries for ordinary setup. These instructions are for changing
the firmware, app or installer. Timestamps and signing keys can change build hashes.

## Firmware

Install ESP-IDF **6.1** and its ESP32-S3 tools, then open an activated ESP-IDF shell.
The required small libraries and Meshtastic generated protocol sources are vendored.

```sh
idf.py set-target esp32s3
idf.py build
```

For MeshCore, run those commands inside `meshcore/`. The default is N16R8 with
RGB LED GPIO48. In `idf.py menuconfig`, change Serial flasher config → Flash size
for N8R8, and MESHBBS board → Addressable RGB LED GPIO for GPIO38 layouts.
Both profiles require 8 MB octal PSRAM; other PSRAM types are not release targets.

The Windows helper `scripts/idf-env.ps1` describes the original SDK installation.
Adjust its SDK/tool paths for your computer, or use the normal activated IDF shell.
To produce every release profile with that helper:

```powershell
./scripts/build-profiles.ps1 -Protocol meshtastic
./scripts/build-profiles.ps1 -Protocol meshcore
```

Results go to `build-release/<protocol>/<profile>/`, including each actual sdkconfig.
The script restores your working sdkconfig; the working build directory still
contains the last profile until rebuilt. Flash a verified release profile, or run
`idf.py build` before flashing the working project. Keep all three flash files
from the same profile together.

## Android

Install Android SDK platform **36.1**, build tools **36.1.0**, and Java **21**.
On Windows:

```powershell
./android/build.ps1 -SdkPath "$env:LOCALAPPDATA\Android\Sdk" -JavaPath 'C:\Program Files\Android\Android Studio\jbr'
```

The script runs Java checks, builds and verifies `android/build/MESHBBS-0.5.0.apk`.
It creates a local signing key under `.local/` on first use. Keep that key private
and backed up if distributing updates. A fresh key cannot update an APK signed
by somebody else. Published updates retain the existing project key so owners
keep their app data and owner credential. No signing key is in Git.

## Radio catalog

```sh
python scripts/research-radios.py
python scripts/build-radio-catalog.py
```

Downloads are pinned by `docs/upstream-lock.json`. Review upstream changes before
updating it. Outputs include the model guide, JSON catalog and Android model names.
A listed variant is a candidate for testing, not proof of hardware acceptance.

## Installer and packages

Use Node **22** or newer and pnpm **10**. From `flasher/`:

```sh
pnpm install --frozen-lockfile --ignore-scripts
pnpm test
pnpm build
pnpm start
```

Release assets in `flasher/public/` let a fresh clone build the installer. To
replace them, build all firmware profiles and the app, then run
`python scripts/prepare-release.py` from the repository root. This checks image
headers, configuration, hashes and private build paths and creates flash ZIPs
under `artifacts/`. Distribute matching source and notices with binaries.

Open `http://127.0.0.1:4173` to preview. USB needs desktop Chrome/Edge and HTTPS or
localhost. GitHub Pages builds the same static site. An actual USB flash is a
separate hardware acceptance step, not part of `pnpm test`.
