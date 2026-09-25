"""Package explicit build outputs; never copy live flash, owner state or signing keys."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PUBLIC = ROOT / 'flasher/public'
VERSION = '0.5.0'
SOURCE_DIRS = ('main', 'components', 'meshcore', 'android', 'assets', 'docs', 'scripts', 'tests', 'flasher', 'licenses', '.github')
SOURCE_FILES = ('.gitignore', 'CMakeLists.txt', 'partitions.csv', 'sdkconfig.defaults', 'LICENSE')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def notices(idf):
    PUBLIC.mkdir(parents=True, exist_ok=True)
    browser = ['MESHBBS browser installer — third-party licenses\n']
    packages = {'esptool-js': '0.7.0', 'js-md5': '0.8.3', 'pako': '2.2.0', 'atob-lite': '2.0.0'}
    for name, version in packages.items():
        directory = ROOT / f'flasher/node_modules/.pnpm/{name}@{version}/node_modules/{name}'
        files = [p for p in directory.iterdir() if p.is_file() and p.name.upper().startswith(('LICENSE', 'NOTICE'))]
        if not files:
            raise ValueError(f'Install the pinned flasher dependencies first; license missing for {name}.')
        browser.append(f'\n{name} {version}\n' + '=' * 72)
        for path in sorted(files):
            browser.append(path.read_text(encoding='utf-8'))
    (PUBLIC / 'THIRD-PARTY-NOTICES.txt').write_text('\n'.join(browser), encoding='utf-8')
    # Include the SDK's license/notice files, with paths relative to the SDK only.
    # This is deliberately broader than the final link: conditional SDK objects
    # may differ between profiles and supported toolchains.
    sdk = ['ESP-IDF 6.1 and bundled components\nhttps://github.com/espressif/esp-idf/tree/v6.1\n']
    files = [idf / 'LICENSE']
    for base, _, names in os.walk(idf / 'components'):
        for name in names:
            if name.upper().split('.')[0] in ('LICENSE', 'LICENCE', 'COPYING', 'NOTICE'):
                files.append(Path(base) / name)
    for path in sorted(set(files)):
        sdk.append(f'\n{path.relative_to(idf).as_posix()}\n' + '=' * 72)
        sdk.append(path.read_text(encoding='utf-8', errors='replace'))
    (PUBLIC / 'SDK-NOTICES.txt').write_text('\n'.join(sdk), encoding='utf-8')


def check_image(data, size):
    if len(data) < 24 or data[0] != 0xe9 or struct.unpack_from('<H', data, 12)[0] != 9:
        raise ValueError('Expected an ESP32-S3 image.')
    if data[2] != 2 or data[3] >> 4 != {8: 3, 16: 4}[size] or data[3] & 15 != 15:
        raise ValueError('Image header does not match DIO, 80 MHz and selected flash size.')
    if re.search(rb'[A-Za-z]:[/\\]Users[/\\]|/home/|/Users/', data):
        raise ValueError('A private build path remains in the image.')


def check_partitions(data):
    expected = [(1, 2, 0x9000, 0x6000), (1, 1, 0xf000, 0x1000),
                (0, 0, 0x10000, 0x300000), (1, 2, 0x310000, 0x100000)]
    rows = []
    for at in range(0, len(data), 32):
        if data[at:at+2] != b'\xaaP':
            break
        _, kind, subtype, offset, length = struct.unpack_from('<HBBII', data, at)
        rows.append((kind, subtype, offset, length))
    if rows != expected:
        raise ValueError('Partition table changed; review storage migration before packaging.')


def collect_profiles():
    catalog = {'version': VERSION, 'license': 'GPL-3.0-only', 'profiles': []}
    for protocol in ('meshtastic', 'meshcore'):
        for size in (16, 8):
            for gpio in (48, 38):
                profile = f'n{size}r8-gpio{gpio}'
                source = ROOT / f'build-release/{protocol}/{profile}'
                config = (source / 'sdkconfig').read_text(encoding='utf-8-sig')
                for required in (f'CONFIG_ESPTOOLPY_FLASHSIZE="{size}MB"', f'CONFIG_MESHBBS_RGB_GPIO={gpio}', 'CONFIG_SPIRAM_MODE_OCT=y'):
                    if required not in config.splitlines():
                        raise ValueError(f'{protocol}/{profile}: configuration missing {required}')
                row = dict(id=f'{protocol}/{profile}', protocol=protocol, chip='ESP32-S3', flashMB=size, ledGPIO=gpio,
                           label=f'{protocol.title()} BBS · N{size}R8 · LED GPIO{gpio}', parts=[])
                for address, name, maximum in ((0, 'bootloader.bin', 0x8000), (0x8000, 'partition-table.bin', 0x1000), (0x10000, 'firmware.bin', 0x300000)):
                    data = (source / name).read_bytes()
                    if not data or len(data) > maximum:
                        raise ValueError(f'{name}: invalid file size')
                    check_partitions(data) if address == 0x8000 else check_image(data, size)
                    relative = f'firmware/{protocol}/{profile}/{name}'
                    destination = PUBLIC / relative
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(data)
                    row['parts'].append(dict(address=address, path=relative, size=len(data), sha256=digest(data)))
                catalog['profiles'].append(row)
    (PUBLIC / 'firmware-manifest.json').write_text(json.dumps(catalog, indent=2) + '\n', encoding='utf-8')
    return catalog


def source_files():
    selected = [ROOT / name for name in SOURCE_FILES] + list(ROOT.glob('*.md'))
    for directory in SOURCE_DIRS:
        for path in (ROOT / directory).rglob('*'):
            relative = path.relative_to(ROOT)
            if not path.is_file() or any(p in ('node_modules', 'dist', '__pycache__', '.local', '.git') or p.startswith('build') for p in relative.parts[:-1]):
                continue
            if path.name in ('sdkconfig', 'sdkconfig.old') or path.suffix in ('.log', '.p12', '.jks', '.pyc'):
                continue
            selected.append(path)
    return sorted(set(selected))


def packages(catalog):
    artifacts = ROOT / 'artifacts'
    artifacts.mkdir(exist_ok=True)
    source = artifacts / f'MESHBBS-{VERSION}-source.zip'
    with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in source_files():
            archive.write(path, f'MESHBBS-{VERSION}/' + path.relative_to(ROOT).as_posix())
    for protocol in ('meshtastic', 'meshcore'):
        path = artifacts / f'MESHBBS-{protocol}-{VERSION}.zip'
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as archive:
            sums = []
            for row in catalog['profiles']:
                if row['protocol'] != protocol:
                    continue
                for part in row['parts']:
                    name = part['path'].removeprefix('firmware/' + protocol + '/')
                    archive.write(PUBLIC / part['path'], name)
                    sums.append(f'{part["sha256"]}  {name}')
            for source_path, name in ((PUBLIC / f'MESHBBS-{VERSION}.apk', f'MESHBBS-{VERSION}.apk'),
                                     (source, source.name), (ROOT / 'docs/GETTING-STARTED.md', 'START-HERE.md'),
                                     (ROOT / 'docs/VALIDATION.md', 'VALIDATION.md'), (ROOT / 'docs/BBS-USER-GUIDE.md', 'BBS-USER-GUIDE.md'),
                                     (ROOT / 'LICENSE', 'LICENSE'), (ROOT / 'THIRD-PARTY-NOTICES.md', 'THIRD-PARTY-NOTICES.md'),
                                     (PUBLIC / 'SDK-NOTICES.txt', 'SDK-NOTICES.txt'), (PUBLIC / 'THIRD-PARTY-NOTICES.txt', 'BROWSER-NOTICES.txt')):
                archive.write(source_path, name)
                sums.append(f'{digest(source_path.read_bytes())}  {name}')
            for license_path in (ROOT / 'components').rglob('LICENSE*'):
                archive.write(license_path, license_path.relative_to(ROOT).as_posix())
            for license_path in (ROOT / 'licenses').glob('*.txt'):
                archive.write(license_path, license_path.relative_to(ROOT).as_posix())
            archive.writestr('SHA256SUMS.txt', '\n'.join(sums) + '\n')
    release_files = [source, artifacts / f'MESHBBS-meshtastic-{VERSION}.zip', artifacts / f'MESHBBS-meshcore-{VERSION}.zip']
    apk = artifacts / f'MESHBBS-{VERSION}.apk'
    shutil.copyfile(PUBLIC / apk.name, apk)
    release_files.append(apk)
    (artifacts / f'MESHBBS-{VERSION}-SHA256SUMS.txt').write_text(''.join(f'{digest(p.read_bytes())}  {p.name}\n' for p in release_files), encoding='ascii')
    for path in release_files:
        print(f'{path.name}: {path.stat().st_size:,} bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--idf', type=Path, default=Path(os.environ.get('IDF_PATH', 'C:/esp/v6.1/esp-idf')))
    parser.add_argument('--notices-only', action='store_true')
    args = parser.parse_args()
    notices(args.idf)
    if not args.notices_only:
        catalog = collect_profiles()
        shutil.copyfile(ROOT / f'android/build/MESHBBS-{VERSION}.apk', PUBLIC / f'MESHBBS-{VERSION}.apk')
        shutil.copyfile(ROOT / 'assets/meshbbs-icon.png', PUBLIC / 'icon.png')
        packages(catalog)
