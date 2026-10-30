"""Shared, explicit native artwork allowlist and release provenance."""
import hashlib
import json


def texture_files(directory, required=True):
    if required and not (directory / "hd.istex").is_file():
        raise ValueError("Release requires the prepared campaign HD pack")
    files = {}
    for name in ("hd.istex", "4k.istex"):
        path = directory / name
        if not path.is_file():
            continue
        data = path.read_bytes()
        count = int.from_bytes(data[8:12], "little")
        if (len(data) < 16 or data[:8] != b"ISTEX001" or not 0 < count <= 16384
                or 16 + count * 36 > len(data) or int.from_bytes(data[12:16], "little") != len(data)):
            raise ValueError("Invalid native texture pack: " + name)
        report_path = path.with_suffix(".json")
        report = json.loads(report_path.read_text())
        if report["pack_bytes"] != len(data) or report["keys"] != count:
            raise ValueError("Texture pack provenance does not match: " + name)
        files[name] = path
        files[report_path.name] = report_path
    if files:
        for name in ("HD-Textures-NOTICE.txt", "puff-LICENSE.txt"):
            path = directory / name
            if not path.is_file():
                raise ValueError("Missing texture pack notice: " + name)
            files[name] = path
    return files


def hd_provenance(directory):
    report = json.loads((directory / "hd.json").read_text())
    return dict(source=report["source"], source_sha256=report["source_sha256"],
                pack_sha256=hashlib.sha256((directory / "hd.istex").read_bytes()).hexdigest(),
                scope=report["scope"], keys=report["keys"], unique_images=report["unique_images"],
                rgba_pixels_preserved=report["rgba_pixels_preserved"])
