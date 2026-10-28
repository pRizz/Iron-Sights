"""ROM-free Windows tests against the actual player executable and MSVC SDK."""
import argparse
import http.server
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import threading
import tempfile
import time
import zipfile


class Receiver(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers["Content-Length"]))
        if self.server.reject_uploads:
            # Simulate a broken connection, without an HTTP response.
            self.close_connection = True
            return
        self.server.envelopes.append(body)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b"{}")

    def log_message(self, *args):
        pass


def run(exe, directory, receiver, arguments, overrides=None):
    env = {key: value for key, value in os.environ.items() if not key.startswith("GE_")}
    env.update(GE_SENTRY_DSN=f"http://public@127.0.0.1:{receiver.server_port}/1",
               GE_SENTRY_ENVIRONMENT="windows-ci-smoke", GE_SENTRY_DEBUG="1")
    env.update(overrides or {})
    with (directory / "stdout.log").open("wb") as output, (directory / "stderr.log").open("wb") as errors:
        return subprocess.run([str(exe), *arguments], cwd=directory, env=env,
                              stdout=output, stderr=errors, timeout=30,
                              creationflags=subprocess.CREATE_NO_WINDOW).returncode


def check(package, build, output):
    if os.name != "nt":
        raise RuntimeError("Run this integration suite on Windows")
    output.mkdir(parents=True, exist_ok=True)
    report_root = output
    output = Path(tempfile.mkdtemp(prefix="run-", dir=output)).resolve()
    (report_root / "latest-run.txt").write_text(str(output) + "\n")
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Receiver)
    server.envelopes = []
    server.reject_uploads = False
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    cases = []
    try:
        for name in ("enabled", "optout", "missing-sdk", "override-off", "healthy", "toggle-off", "readonly-toggle-off", "watchdog"):
            directory = output / name
            directory.mkdir(exist_ok=True)
            if package:
                with zipfile.ZipFile(package) as archive:
                    archive.extractall(directory)
            else:
                for filename in ("IronSights.exe", "SDL2.dll", "sentry.dll", "sentry-crash.exe", "sentry-wer.dll"):
                    shutil.copy2(build / filename, directory / filename)
            exe = directory / "IronSights.exe"
            if name == "optout":
                (directory / "midas.ini").write_text("[diagnostics]\ncrash_reporting=0\n")
                queued = directory / "Data/CrashReports/cache"
                queued.mkdir(parents=True, exist_ok=True)
                (queued / "old.envelope").write_bytes(b"must be discarded")
            if name == "missing-sdk":
                (directory / "sentry.dll").unlink()
            if name == "readonly-toggle-off":
                (directory / "midas.ini").write_text("[diagnostics]\ncrash_reporting=1\n")
                (directory / "midas.ini").chmod(stat.S_IREAD)
            before = len(server.envelopes)
            arguments = ["--test-crash-reporting"] if name in ("enabled", "optout", "override-off") else ["--check-crash-reporting"]
            if name in ("toggle-off", "readonly-toggle-off"):
                arguments = ["--test-crash-reporting-optout"]
            if name == "watchdog":
                arguments = ["--test-stall-reporting"]
            overrides = {"GE_CRASH_REPORTING": "0"} if name == "override-off" else {}
            code = run(exe, directory, server, arguments, overrides)
            if name == "enabled":
                deadline = time.monotonic() + 5
                while len(server.envelopes) == before and time.monotonic() < deadline:
                    time.sleep(0.05)
            count = len(server.envelopes) - before
            assert count == (1 if name in ("enabled", "watchdog") else 0), (name, count)
            if name == "watchdog":
                event = json.loads(server.envelopes[-1].split(b"\n")[2])
                assert event["logger"] == "watchdog" and event["level"] == "error"
            if arguments[0].startswith("--test-crash-reporting"):
                assert code != 0, (name, code)
                assert (directory / "Data/Logs/crash.log").is_file(), name
            else:
                assert code == 0, (name, code)
            if name == "optout":
                assert not (directory / "Data/CrashReports/cache/old.envelope").exists()
            if name == "toggle-off":
                assert "crash_reporting=0" in (directory / "midas.ini").read_text()
            cases.append(dict(case=name, exit_code=code, uploads=count))
        # Arrange: an enabled crash while the ingestion service is unavailable.
        directory = output / "offline-retry"
        directory.mkdir(exist_ok=True)
        if package:
            with zipfile.ZipFile(package) as archive:
                archive.extractall(directory)
        else:
            for filename in ("IronSights.exe", "SDL2.dll", "sentry.dll", "sentry-crash.exe", "sentry-wer.dll"):
                shutil.copy2(build / filename, directory / filename)
        server.reject_uploads = True
        before = len(server.envelopes)
        # Act: crash, restore connectivity, then restart normally.
        code = run(directory / "IronSights.exe", directory, server, ["--test-crash-reporting"])
        assert code != 0
        deadline = time.monotonic() + 10
        queued = []
        while not queued and time.monotonic() < deadline:
            queued = list((directory / "Data/CrashReports").rglob("*.envelope"))
            time.sleep(0.1)
        assert queued, "Offline crash was not persisted"
        server.reject_uploads = False
        assert run(directory / "IronSights.exe", directory, server, ["--check-crash-reporting"]) == 0
        # Assert: exactly one queued event arrives on restart.
        assert len(server.envelopes) - before == 1, "Offline retry lost or duplicated the event"
        cases.append(dict(case="offline-retry", exit_code=code, uploads=1))
        envelope = server.envelopes[0]
        lines = envelope.split(b"\n")
        assert json.loads(lines[1])["type"] == "event"
        event = json.loads(lines[2])
        assert event["level"] == "fatal"
        assert event["platform"] == "native"
        assert event["release"].startswith("iron-sights@")
        assert all(isinstance(event["extra"][key], int) for key in ("stage", "menu", "frame"))
        images = event["debug_meta"]["images"]
        image = next(item for item in images if item["code_file"].endswith("IronSights.exe"))
        assert image["arch"] == "x86" and image.get("debug_id", "").strip("0-")
        assert b'"type":"attachment"' not in envelope
        (output / "captured.envelope").write_bytes(envelope)
        result = json.dumps(dict(cases=cases, event_id=event["event_id"],
            debug_id=image["debug_id"], bytes=len(envelope)), indent=2) + "\n"
        (output / "results.json").write_text(result)
        (report_root / "results.json").write_text(result)
        print(json.dumps(cases))
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        # Audit copies are temporary; retain only logs and captured diagnostics.
        for path in output.glob("*/*"):
            if path.suffix.lower() in (".exe", ".dll"):
                path.unlink()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--package", type=Path)
    source.add_argument("--build", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check(args.package, args.build, args.output)
