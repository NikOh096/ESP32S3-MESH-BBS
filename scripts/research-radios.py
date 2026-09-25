"""Download the public evidence pinned in docs/upstream-lock.json."""
import concurrent.futures
import json
import pathlib
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
CACHE = ROOT / '.reference'
MC = CACHE / 'meshcore'
LOCK = json.loads((ROOT / 'docs/upstream-lock.json').read_text(encoding='utf-8'))


def get(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'MESHBBS-radio-catalog'})
    with urllib.request.urlopen(request, timeout=45) as response:
        return response.read()


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def fetch_variant(path):
    destination = MC / path
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Refresh each file so a changed lock cannot reuse evidence from an older commit.
    destination.write_bytes(get(
        f'https://raw.githubusercontent.com/meshcore-dev/MeshCore/{LOCK["meshcore"]["commit"]}/{path}'))


if __name__ == '__main__':
    reference = LOCK['meshcore']
    save_json(MC / 'reference.json', reference)
    tree = json.loads(get(f'https://api.github.com/repos/meshcore-dev/MeshCore/git/trees/{reference["commit"]}?recursive=1'))
    if tree.get('truncated'):
        raise RuntimeError('Upstream source tree is incomplete; do not publish a partial catalog.')
    save_json(MC / 'tree.json', tree)
    paths = [row['path'] for row in tree['tree'] if row['path'].endswith('platformio.ini')]
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        list(pool.map(fetch_variant, paths))
    release = json.loads(get(f'https://api.github.com/repos/meshcore-dev/MeshCore/releases/tags/{reference["tag"]}'))
    save_json(MC / 'release.json', release)
    mt = LOCK['meshtastic']
    (CACHE / 'meshtastic-hardware-list.json').write_bytes(get(
        f'https://raw.githubusercontent.com/{mt["repo"]}/{mt["commit"]}/{mt["path"]}'))
    save_json(CACHE / 'meshtastic-hardware-list.json.source.json', mt)
    print(f'Fetched {len(paths)} pinned build definitions and {len(release["assets"])} release assets.')
