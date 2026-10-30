"""ROM-free crash/opt-out tests for the packaged Linux ELF and AppImage."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import threading
import time
import zipfile
from http.server import ThreadingHTTPServer

from test_sentry_windows import Receiver


def verify_artwork(read):
    prefix = "Data/TexturePacks/"
    pack = read(prefix + "hd.istex")
    report = json.loads(read(prefix + "hd.json"))
    provenance = json.loads(read("build-inputs.json"))["hd_textures"]
    assert pack[:8] == b"ISTEX001" and len(pack) == report["pack_bytes"]
    assert int.from_bytes(pack[8:12], "little") == report["keys"]
    assert hashlib.sha256(pack).hexdigest() == provenance["pack_sha256"]
    for name in ("HD-Textures-NOTICE.txt", "puff-LICENSE.txt"):
        assert read(prefix + name)
    sums = dict(line.split("  ", 1)[::-1] for line in read("SHA256SUMS.txt").decode().splitlines())
    assert sums[prefix + "hd.istex"] == provenance["pack_sha256"]
    return provenance["pack_sha256"]


def wait_upload(server, count, seconds=6):
    deadline = time.monotonic() + seconds
    while len(server.envelopes) < count and time.monotonic() < deadline:
        time.sleep(0.05)


def run(exe, directory, server, args, overrides=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GE_")}
    env.pop("LD_LIBRARY_PATH", None)  # Exercise the shipped ELF's relative RUNPATH.
    env.update(GE_SENTRY_DSN=f"http://public@127.0.0.1:{server.server_port}/1",
               GE_SENTRY_ENVIRONMENT="linux-ci-smoke", GE_SENTRY_DEBUG="1")
    env.update(overrides or {})
    with (directory / "stdout.log").open("wb") as out, (directory / "stderr.log").open("wb") as err:
        return subprocess.run([str(exe), *args], env=env, cwd=directory,
                              stdout=out, stderr=err, timeout=30).returncode


def check(package, appimage, output):
    output.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="run-", dir=output)).resolve()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Receiver)
    server.envelopes, server.reject_uploads = [], False
    threading.Thread(target=server.serve_forever, daemon=True).start()
    cases = []
    try:
        with tarfile.open(package) as stream:
            names = stream.getnames()
            assert not any(Path(n).name == "midas.ini" or n.endswith((".z64", ".n64", ".v64", ".debug", ".envelope")) for n in names)
            assert stream.getmember("Iron-Sights-linux-i686/IronSights").mode & 0o111
            elf = stream.extractfile("Iron-Sights-linux-i686/IronSights").read(20)
            assert elf[:5] == b"\x7fELF\x01" and elf[18:20] == b"\x03\x00"
            artwork_hash = verify_artwork(lambda name: stream.extractfile("Iron-Sights-linux-i686/" + name).read())
        # Verify the actual squashfs contents, not just the tarball input tree.
        extracted = work / "appimage-contents"
        extracted.mkdir()
        subprocess.run([str(appimage.resolve()), "--appimage-extract"], cwd=extracted,
                       stdout=subprocess.DEVNULL, check=True, timeout=60)
        assert verify_artwork(lambda name: (extracted / "squashfs-root" / name).read_bytes()) == artwork_hash
        for name in ("enabled", "healthy", "optout", "toggle-off", "override-off", "missing-sdk", "symlink-cache", "offline-retry", "appimage"):
            directory = work / name
            directory.mkdir()
            with tarfile.open(package) as stream:
                stream.extractall(directory)
            directory = directory / "Iron-Sights-linux-i686"
            exe = directory / "IronSights"
            args = ["--test-crash-reporting"]
            overrides = {}
            if name == "healthy": args = ["--check-crash-reporting"]
            if name == "optout":
                (directory / "midas.ini").write_text("[diagnostics]\ncrash_reporting=0\n[display]\nwidth=800\n")
                pending = directory / "Data/CrashReports/queued"
                pending.mkdir(parents=True)
                (pending / "old.envelope").write_text("discard on opt-out")
            if name == "toggle-off": args = ["--test-crash-reporting-optout"]
            if name == "override-off": overrides = {"GE_CRASH_REPORTING": "0"}
            if name == "missing-sdk":
                (directory / "lib/libsentry.so").unlink()
                args = ["--check-crash-reporting"]
            if name == "symlink-cache":
                outside = work / "outside"
                outside.mkdir()
                (outside / "keep").write_text("never delete")
                (directory / "Data/CrashReports").symlink_to(outside, target_is_directory=True)
                (directory / "midas.ini").write_text("[diagnostics]\ncrash_reporting=0\n")
                args = ["--check-crash-reporting"]
            if name == "offline-retry": server.reject_uploads = True
            if name == "appimage":
                exe = appimage.resolve()
                args = ["--appimage-extract-and-run", "--test-crash-reporting"]
                overrides = dict(GE_DATA_ROOT=str(directory / "appimage-data"), XDG_DATA_HOME=str(directory / "unused-xdg"))
            before = len(server.envelopes)
            code = run(exe, directory, server, args, overrides)
            if name == "offline-retry":
                time.sleep(2)
                assert len(server.envelopes) == before
                server.reject_uploads = False
                assert run(exe, directory, server, ["--check-crash-reporting"]) == 0
            expected = int(name in ("enabled", "offline-retry", "appimage"))
            wait_upload(server, before + expected)
            assert len(server.envelopes) - before == expected, (name, code, (directory / "stderr.log").read_text()[-2000:])
            assert (code == 0) == (name in ("healthy", "missing-sdk", "symlink-cache")), (name, code)
            if name == "toggle-off": assert "crash_reporting=0" in (directory / "midas.ini").read_text().replace(" ", "")
            if name == "optout": assert not list((directory / "Data/CrashReports").rglob("*.envelope"))
            if name == "symlink-cache": assert (work / "outside/keep").read_text() == "never delete"
            if name == "appimage":
                assert (directory / "appimage-data/Data/shaders_base.txt").is_file()
                assert not (directory / "appimage-data/Data/TexturePacks").exists()
                assert not (directory / "unused-xdg").exists()
            cases.append(dict(case=name, exit_code=code, uploads=expected))
        body = server.envelopes[0]
        if body[:2] == b"\x1f\x8b": body = gzip.decompress(body)
        lines = body.splitlines()
        event = next(json.loads(lines[i + 1]) for i in range(1, len(lines)-1) if json.loads(lines[i]).get("type") == "event")
        (output / "captured-event.json").write_text(json.dumps(event, indent=2) + "\n")
        assert b'"type":"attachment"' not in body.replace(b" ", b"")
        assert event["level"] == "fatal" and event["release"].startswith("iron-sights@")
        assert event["extra"] == dict(stage=-1, menu=-1, frame=0)
        image = next(image for image in event["debug_meta"]["images"] if image["code_file"].endswith("/IronSights"))
        assert image["debug_id"] and event["tags"]["architecture"] == "i686"
        result = dict(cases=cases, hd_pack_sha256=artwork_hash, event_id=event["event_id"], debug_id=image["debug_id"], bytes=len(body))
        (output / "results.json").write_text(json.dumps(result, indent=2) + "\n")
        print(json.dumps(result, indent=2))
    finally:
        server.shutdown()
        # mkdtemp/SDK queues are private (0700). Preserve useful diagnostics in
        # one readable artifact; do not leave root-owned runtime copies for the
        # host Actions uploader to traverse after a Docker test.
        with zipfile.ZipFile(output / ("logs-" + work.name + ".zip"), "w", zipfile.ZIP_DEFLATED) as archive:
            for path in work.rglob("*"):
                if path.is_file() and (path.suffix in (".log", ".json", ".envelope") or path.name == "__sentry-event"):
                    archive.write(path, path.relative_to(work))
        if work.parent != output.resolve():
            raise RuntimeError("Unexpected smoke directory; refusing cleanup")
        shutil.rmtree(work)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--appimage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check(args.package, args.appimage, args.output)
