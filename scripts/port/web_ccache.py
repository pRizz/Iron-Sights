#!/usr/bin/env python3
"""The browser build's compiler cache: ccache's official static Linux binary at
the version and SHA-256 pinned in web_toolchain.json, mounted into the pinned
SDK container and used through Emscripten's EM_COMPILER_WRAPPER."""
import hashlib
from pathlib import Path
import re
import tarfile
import urllib.request

VERSION_PATTERN = r'\d+\.\d+(\.\d+)?'


def pin(config):
    """The validated ccache pin from the web toolchain configuration."""
    entry = config['ccache']
    version = entry['version']
    if not re.fullmatch(VERSION_PATTERN, version) or not re.fullmatch(r'[0-9a-f]{64}', entry['sha256']):
        raise ValueError('Invalid ccache pin')
    name = f'ccache-{version}-linux-x86_64-musl-static'
    url = f'https://github.com/ccache/ccache/releases/download/v{version}/{name}.tar.gz'
    if entry['url'] != url:
        raise ValueError('The ccache URL must be the pinned version\'s official static release')
    return dict(version=version, sha256=entry['sha256'], url=url, archive=name + '.tar.gz', member=name + '/ccache')


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ensure(directory, entry, download=urllib.request.urlretrieve):
    """Returns the pinned ccache binary under directory, downloading its release
    archive once and checking the archive against the pin every time."""
    directory.mkdir(parents=True, exist_ok=True)
    archive = directory / entry['archive']
    if not archive.is_file() or sha256(archive) != entry['sha256']:
        partial = archive.with_name(archive.name + '.part')
        download(entry['url'], partial)
        if sha256(partial) != entry['sha256']:
            partial.unlink()
            raise ValueError('The downloaded ccache does not match its pinned SHA-256')
        partial.replace(archive)
    binary = directory / 'bin' / ('ccache-' + entry['version'])
    with tarfile.open(archive) as bundle:
        member = bundle.getmember(entry['member'])
        if not member.isfile():
            raise ValueError('The ccache archive has no binary')
        binary.parent.mkdir(parents=True, exist_ok=True)
        binary.write_bytes(bundle.extractfile(member).read())
    binary.chmod(0o755)
    return binary


def docker_arguments(binary, objects):
    """Mounts and settings that make emcc compile through the cache in objects."""
    objects.mkdir(parents=True, exist_ok=True)
    return ['--mount', f'type=bind,source={binary},target=/usr/local/bin/ccache,readonly',
            '--mount', f'type=bind,source={objects},target=/ccache',
            '-e', 'EM_COMPILER_WRAPPER=ccache', '-e', 'CCACHE_DIR=/ccache', '-e', 'CCACHE_BASEDIR=/src',
            '-e', 'CCACHE_COMPILERCHECK=content', '-e', 'CCACHE_MAXSIZE=1G']
