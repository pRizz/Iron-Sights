"""Pinned Windows Sentry build, package and symbol-upload tooling."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = ROOT / "scripts/port/sentry_config.json"
CONFIG = json.loads(CONFIG_PATH.read_text())
RUNTIME_FILES = ("sentry.dll", "sentry-crash.exe", "sentry-wer.dll")


def run(*args, **kwargs):
    subprocess.run([str(arg) for arg in args], check=True, **kwargs)


def header(output, dsn=""):
    dsn = dsn or CONFIG["dsn"]
    if dsn and not (dsn.startswith("https://") or dsn.startswith("http://public@127.0.0.1:")):
        raise ValueError("Expected a public HTTPS ingestion DSN or local test receiver")
    output.parent.mkdir(parents=True, exist_ok=True)
    content = ("/* Generated public ingestion configuration; contains no auth token. */\n"
               "#define MIDAS_SENTRY_DSN " + json.dumps(dsn) + "\n")
    if not output.exists() or output.read_text() != content:
        output.write_text(content, encoding="utf-8")


def checkout_source(work):
    work.mkdir(parents=True, exist_ok=True)
    source, build_dir = work / "source", work / "build"
    if not source.exists():
        run("git", "clone", "--filter=blob:none", "--no-checkout",
            "https://github.com/getsentry/sentry-native.git", source)
    git = ["git", "-c", "safe.directory=" + source.resolve().as_posix(), "-C", str(source)]
    run(*git, "fetch", "--depth=1", "origin", CONFIG["sdk_commit"])
    run(*git, "checkout", "--detach", CONFIG["sdk_commit"])
    if subprocess.check_output([*git, "rev-parse", "HEAD"], text=True).strip() != CONFIG["sdk_commit"]:
        raise ValueError("Sentry source revision mismatch")
    if subprocess.check_output([*git, "status", "--porcelain"], text=True).strip():
        raise ValueError("Refusing to build a modified Sentry SDK")
    return source, build_dir


def licenses(source, output):
    license_text = (source / "LICENSE").read_text()
    mpack = (source / "vendor/mpack.h").read_text().split("*/", 1)[0] + "*/\n"
    jsmn = (source / "vendor/jsmn.h").read_text().split("*/", 1)[0] + "*/\n"
    stb = (source / "vendor/stb_sprintf.h").read_text().split("ALTERNATIVE A - MIT License\n", 1)[1]
    stb = stb.split("------------------------------------------------------------------------------", 1)[0]
    (output / "Sentry-LICENSE.txt").write_text(license_text + "\nMPack\n" + mpack +
        "\njsmn\n" + jsmn + "\nstb_sprintf (MIT)\n" + stb)


def build(work, output):
    source, build_dir = checkout_source(work)
    run("cmake", "-S", source, "-B", build_dir, "-G", "Visual Studio 17 2022", "-A", "Win32",
        "-DCMAKE_SYSTEM_VERSION=10.0", "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
        "-DSENTRY_BACKEND=native", "-DSENTRY_BUILD_SHARED_LIBS=ON",
        "-DSENTRY_BUILD_RUNTIMESTATIC=ON", "-DSENTRY_BUILD_TESTS=OFF", "-DSENTRY_BUILD_EXAMPLES=OFF")
    run("cmake", "--build", build_dir, "--config", "RelWithDebInfo", "--parallel", "4")
    output.mkdir(parents=True, exist_ok=True)
    (output / "include").mkdir(exist_ok=True)
    shutil.copy2(source / "include/sentry.h", output / "include/sentry.h")
    licenses(source, output)
    for name in RUNTIME_FILES:
        shutil.copy2(build_dir / "RelWithDebInfo" / name, output / name)
    # Keep SDK PDBs for symbol upload, outside the player ZIP.
    symbols = output / "symbols"
    symbols.mkdir(exist_ok=True)
    for path in (build_dir / "RelWithDebInfo").glob("*.pdb"):
        shutil.copy2(path, symbols / path.name)
    fingerprints = {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in RUNTIME_FILES}
    compiler_metadata = next((build_dir / "CMakeFiles").glob("*/CMakeCCompiler.cmake")).read_text()
    compiler_version = re.search(r'set\(CMAKE_C_COMPILER_VERSION "([^"]+)"\)', compiler_metadata).group(1)
    (output / "sentry-build.json").write_text(json.dumps(dict(
        sdk_version=CONFIG["sdk_version"], sdk_commit=CONFIG["sdk_commit"], architecture="x86",
        backend="native", compiler="MSVC VS2022", compiler_version=compiler_version,
        runtime="static CRT", sha256=fingerprints), indent=2) + "\n")


def stage(source, destination):
    info = json.loads((source / "sentry-build.json").read_text())
    if info["sdk_commit"] != CONFIG["sdk_commit"] or info["architecture"] != "x86":
        raise ValueError("Sentry artifact does not match the pinned x86 SDK")
    for name in RUNTIME_FILES:
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != info["sha256"][name]:
            raise ValueError("Sentry artifact hash mismatch: " + name)
    destination.mkdir(parents=True, exist_ok=True)
    for name in (*RUNTIME_FILES, "Sentry-LICENSE.txt", "sentry-build.json"):
        shutil.copy2(source / name, destination / name)


def cli(work):
    filename = "sentry-cli-Windows-x86_64.exe" if os.name == "nt" else "sentry-cli-Linux-x86_64"
    digest = CONFIG["cli_sha256"][filename]
    if not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise ValueError("Expected a pinned SHA256 digest for the Sentry CLI asset")
    url = f"https://github.com/getsentry/sentry-cli/releases/download/{CONFIG['cli_version']}/{filename}"
    work.mkdir(parents=True, exist_ok=True)
    target = work / ("sentry-cli.exe" if os.name == "nt" else "sentry-cli")
    if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError("Sentry CLI checksum mismatch")
        target.write_bytes(data)
        target.chmod(0o755)
    return target


def upload(executable, sdk, work):
    tool = cli(work)
    run(tool, "debug-files", "check", executable)
    if not os.environ.get("SENTRY_AUTH_TOKEN"):
        raise ValueError("SENTRY_AUTH_TOKEN is required; it must be a CI secret, never a binary setting")
    run(tool, "debug-files", "upload", "--org", CONFIG["organization"], "--project", CONFIG["project"],
        "--include-sources", "--wait", executable, sdk)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    command = commands.add_parser("header")
    command.add_argument("--output", type=Path, required=True)
    command.add_argument("--dsn", default="")
    command = commands.add_parser("build")
    command.add_argument("--work", type=Path, required=True)
    command.add_argument("--output", type=Path, required=True)
    command = commands.add_parser("stage")
    command.add_argument("--source", type=Path, required=True)
    command.add_argument("--destination", type=Path, required=True)
    command = commands.add_parser("upload")
    command.add_argument("--exe", type=Path, required=True)
    command.add_argument("--sdk", type=Path, required=True)
    command.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "header": header(args.output, args.dsn)
    if args.command == "build": build(args.work, args.output)
    if args.command == "stage": stage(args.source, args.destination)
    if args.command == "upload": upload(args.exe, args.sdk, args.work)


if __name__ == "__main__":
    main()
