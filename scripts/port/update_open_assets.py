"""Vendor a release of the open assets (pRizz/iron-sights-assets), or verify it.

    python scripts/port/update_open_assets.py --tag v0.2.0   fetch, verify and vendor
    python scripts/port/update_open_assets.py --verify       check the vendored files

Fetching downloads the release's SHA256SUMS, LICENSE and one archive per font,
checks every download against SHA256SUMS, and extracts only the files the
game needs into port/fast3d/open_assets/. manifest.json records the
repository, tag, archive checksums and each vendored file's SHA-256; --verify
rechecks those without the network (the release tests and CI run it).
"""
import argparse
import hashlib
import io
import json
import sys
import tarfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VENDOR = ROOT / 'port' / 'fast3d' / 'open_assets'
MANIFEST = VENDOR / 'manifest.json'
REPOSITORY = 'pRizz/iron-sights-assets'
FONTS = ('title', 'body')
FONT_FILES = ('atlas.png', 'metrics.json')  # what the game uses from each font


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def fetch(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def parse_sums(text):
    sums = {}
    for line in text.splitlines():
        if line.strip():
            digest, name = line.split(maxsplit=1)
            sums[name.strip().lstrip('*')] = digest
    return sums


def checked(name, data, sums):
    """data, after confirming it matches its SHA256SUMS entry."""
    if name not in sums:
        raise SystemExit(f'{name} is not listed in SHA256SUMS')
    if sha256(data) != sums[name]:
        raise SystemExit(f'{name} does not match its SHA256SUMS entry')
    return data


def extract(archive, font):
    """{file name: bytes} for the font's FONT_FILES, refusing anything else."""
    wanted = {f'{font}/{name}': name for name in FONT_FILES}
    out = {}
    with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
        for member in tar.getmembers():
            if member.name in wanted and member.isfile():
                out[wanted[member.name]] = tar.extractfile(member).read()
    missing = sorted(set(FONT_FILES) - set(out))
    if missing:
        raise SystemExit(f'{font} archive lacks {", ".join(missing)}')
    return out


def vendor(tag, repository):
    base = f'https://github.com/{repository}/releases/download/{tag}'
    sums_text = fetch(f'{base}/SHA256SUMS').decode('utf-8')
    sums = parse_sums(sums_text)
    files = {'LICENSE': checked('LICENSE', fetch(f'{base}/LICENSE'), sums)}
    archives = {}
    for font in FONTS:
        name = f'{font}-{tag}.tar.gz'
        archive = checked(name, fetch(f'{base}/{name}'), sums)
        archives[name] = sums[name]
        for file_name, data in extract(archive, font).items():
            files[f'{font}/{file_name}'] = data
    for path, data in files.items():
        target = VENDOR / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    manifest = {
        'repository': f'https://github.com/{repository}',
        'tag': tag,
        'license': 'MIT',
        'release': f'https://github.com/{repository}/releases/tag/{tag}',
        'archives': archives,
        'files': {path: sha256(data) for path, data in sorted(files.items())},
    }
    MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(f'vendored {repository} {tag} into {VENDOR.relative_to(ROOT)}')


def verify():
    """Problems with the vendored files, as messages (empty when all is well)."""
    if not MANIFEST.exists():
        return ['port/fast3d/open_assets/manifest.json is missing']
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    problems = []
    for path, digest in manifest['files'].items():
        target = VENDOR / path
        if not target.exists():
            problems.append(f'{path} is missing')
        elif sha256(target.read_bytes()) != digest:
            problems.append(f'{path} differs from the manifest')
    listed = set(manifest['files']) | {'manifest.json'}
    present = {p.relative_to(VENDOR).as_posix() for p in VENDOR.rglob('*') if p.is_file()}
    problems += [f'{path} is not in the manifest' for path in sorted(present - listed)]
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--tag', help='release tag to vendor, for example v0.2.0')
    group.add_argument('--verify', action='store_true', help='check the vendored files against the manifest')
    parser.add_argument('--repository', default=REPOSITORY)
    args = parser.parse_args()
    if args.tag:
        vendor(args.tag, args.repository)
    problems = verify()
    for problem in problems:
        print(problem)
    if problems:
        return 1
    print('open assets match their manifest')
    return 0


if __name__ == '__main__':
    sys.exit(main())
