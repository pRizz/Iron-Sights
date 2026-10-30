"""Fetch and verify the pinned environment artwork missing from the HD ZIP."""
import hashlib
import json
from pathlib import Path
from urllib.parse import quote
import urllib.request

MANIFEST = Path(__file__).with_name("hd_level_supplement.json")


def verify(data, entry):
    header = f"blob {len(data)}\0".encode()
    if len(data) != entry["size"] or hashlib.sha1(header+data).hexdigest() != entry["git_blob_sha1"]:
        raise ValueError("Supplement artwork does not match pinned Git blob: "+entry["path"])


def artwork(directory, download=False):
    manifest = json.loads(MANIFEST.read_text())
    directory = Path(directory)
    for entry in manifest["textures"]:
        path = directory/Path(entry["path"]).name
        if not path.is_file():
            if not download:
                raise FileNotFoundError("Missing pinned supplement: "+str(path))
            url = "https://raw.githubusercontent.com/GhostlyDark/GoldenEye-007-HD/"+manifest["revision"]+"/"+quote(entry["path"])
            print("Downloading pinned environment artwork: "+entry["path"], flush=True)
            with urllib.request.urlopen(url, timeout=60) as response:
                data = response.read(entry["size"]+1)
            verify(data, entry)
            directory.mkdir(parents=True, exist_ok=True)
            temporary = path.with_suffix(".tmp")
            temporary.write_bytes(data)
            temporary.replace(path)
        data = path.read_bytes()
        verify(data, entry)
        yield entry["path"], data
