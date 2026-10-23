"""Cache Ubuntu package downloads, verifying every .deb against signed APT indexes.

Only archives are cached. APT's package database, installed tools and signed
indexes are recreated on each runner. The plan stays outside the cache.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
from urllib.parse import unquote


def run(args, capture=False):
    result = subprocess.run(args, check=True, text=True,
                            stdout=subprocess.PIPE if capture else None)
    return result.stdout if capture else ""


def packages(profile):
    if profile == "tests":
        return ["gcc-multilib"]
    result = ["gcc-mingw-w64-i686-win32", "g++-mingw-w64-i686-win32",
              "binutils-mingw-w64-i686", "ccache"]
    # Hosted runners provide these outside dpkg; don't download another CMake.
    for tool in ("cmake", "make"):
        if not shutil.which(tool):
            result.append(tool)
    return result


def options(root, archives=None):
    return ["-o", "Dir::Etc::sourcelist="+str(root/"ubuntu.sources"),
            "-o", "Dir::Etc::sourceparts=-",
            "-o", "Dir::State::lists="+str(root/"lists"),
            "-o", "Dir::Cache::archives="+str(archives or root/"archives"),
            "-o", "Acquire::Retries=2", "-o", "Acquire::http::Timeout=30",
            "-o", "Acquire::https::Timeout=30", "-o", "APT::Keep-Downloaded-Packages=true"]


def apt(root, args, capture=False, archives=None, program="apt-get"):
    prefix = [] if os.geteuid() == 0 else ["sudo"]
    return run([*prefix, program, *options(root, archives), *args], capture)


def paragraphs(text):
    for paragraph in text.strip().split("\n\n"):
        yield dict(line.split(": ", 1) for line in paragraph.splitlines()
                   if ": " in line and not line.startswith(" "))


def archive_record(line, metadata):
    uri, name, size, _legacy_hash = shlex.split(line)
    if Path(name).name != name or not re.fullmatch(r"[A-Za-z0-9.+:%~_-]+\.deb", name):
        raise ValueError("Unsafe APT archive filename")
    matches = [p for p in paragraphs(metadata)
               if Path(p.get("Filename", "")).name == unquote(Path(uri).name)]
    if len(matches) != 1 or not re.fullmatch(r"[0-9a-f]{64}", matches[0].get("SHA256", "")):
        raise ValueError("Expected one signed-index SHA256 for "+name)
    record = matches[0]
    if int(record["Size"]) != int(size):
        raise ValueError("APT plan/index size disagreement")
    return dict(name=name, size=int(size), sha256=record["SHA256"], url=uri,
                package=record["Package"], version=record["Version"], architecture=record["Architecture"])


def verify_archives(folder, records, require=False):
    expected = {r["name"]: r for r in records}
    removed = []
    for path in folder.glob("*.deb"):
        record = expected.get(path.name)
        if (path.is_symlink() or not record or path.stat().st_size != record["size"]
                or hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]):
            path.unlink()
            removed.append(path.name)
    if require:
        missing = sorted(name for name in expected if not (folder/name).is_file())
        if missing:
            raise ValueError("Missing verified archives: "+", ".join(missing))
    return removed


def compiler_identity(plan):
    # Hosted image rollouts can mix revisions between consecutive runs. Key the
    # actual tools, not that unrelated image revision; ccache checks headers/flags.
    identity = dict(requested=plan["requested"], ubuntu=plan["ubuntu"],
                    architecture=plan["architecture"],
                    ccache=run(["ccache", "--version"], True), binaries={})
    for tool in ("gcc", "g++"):
        compiler = "i686-w64-mingw32-"+tool
        for path in [shutil.which(compiler),
                     run([compiler, "-print-prog-name="+("cc1" if tool == "gcc" else "cc1plus")], True).strip()]:
            identity["binaries"][Path(path).name] = hashlib.sha256(Path(path).read_bytes()).hexdigest()
    return identity


def prepare(root, profile):
    root.mkdir(parents=True, exist_ok=True)
    for directory in ("lists/partial", "archives/partial", "planning-empty/partial"):
        (root/directory).mkdir(parents=True, exist_ok=True)
    release = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
    if release.get("ID") != "ubuntu" or release.get("VERSION_CODENAME") != "noble":
        raise ValueError("This dependency profile targets Ubuntu 24.04 (noble)")
    architecture = run(["dpkg", "--print-architecture"], True).strip()
    sources = ""
    # Avoid the Azure mirror that delivered a 128 MB toolchain at 36.6 kB/s.
    # Explicit Ubuntu keyring + fresh private indexes retain APT authentication.
    for uri, suites in (("https://archive.ubuntu.com/ubuntu", "noble noble-updates"),
                        ("https://security.ubuntu.com/ubuntu", "noble-security")):
        sources += (f"Types: deb\nURIs: {uri}\nSuites: {suites}\n"
                    f"Components: main universe\nArchitectures: {architecture}\n"
                    "Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\n\n")
    (root/"ubuntu.sources").write_text(sources)
    apt(root, ["update", "--error-on=any"])
    requested = packages(profile)
    candidates = {}
    for name in requested:
        policy = apt(root, ["policy", name], True, program="apt-cache")
        match = re.search(r"Candidate: (\S+)", policy)
        if not match or match[1] == "(none)":
            raise ValueError("No authenticated Ubuntu candidate for "+name)
        candidates[name] = match[1]
    install = [f"{name}={version}" for name, version in candidates.items()]
    output = apt(root, ["--print-uris", "--assume-yes", "--download-only",
                       "--no-install-recommends", "install", *install], True,
                 archives=root/"planning-empty")
    records = []
    for line in output.splitlines():
        if not line.startswith("'"):
            continue
        name = shlex.split(line)[1]
        package, version, _arch = name[:-4].split("_")
        metadata = apt(root, ["show", package+"="+unquote(version)], True, program="apt-cache")
        records.append(archive_record(line, metadata))
    plan = dict(schema=1, profile=profile, ubuntu="noble", architecture=architecture,
                requested=candidates, install=install, archives=records)
    (root/"plan.json").write_text(json.dumps(plan, indent=2)+"\n")
    script = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()[:16]
    prefix = f"apt-v1-noble-{architecture}-{profile}-{script}-"
    digest = hashlib.sha256(json.dumps(plan, sort_keys=True).encode()).hexdigest()
    values = dict(cache_key=prefix+digest, cache_prefix=prefix, count=len(records))
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as stream:
            stream.write("".join(f"{k}={v}\n" for k, v in values.items()))
    print(f"Resolved {len(records)} authenticated package archives ({sum(r['size'] for r in records):,} bytes)")


def install(root):
    plan = json.loads((root/"plan.json").read_text())
    removed = verify_archives(root/"archives", plan["archives"])
    print(f"Discarded {len(removed)} stale/corrupt cached archives", flush=True)
    reused = sum(r["size"] for r in plan["archives"] if (root/"archives"/r["name"]).is_file())
    print(f"Reusing {reused:,} bytes of verified package downloads", flush=True)
    args = ["--assume-yes", "--no-install-recommends", "install", *plan["install"]]
    apt(root, ["--download-only", *args])
    verify_archives(root/"archives", plan["archives"], require=True)
    # Every needed archive was SHA256-verified against this run's signed indexes.
    apt(root, ["--no-download", *args])
    if plan["profile"] == "windows":
        prefix = [] if os.geteuid() == 0 else ["sudo"]
        for compiler in ("gcc", "g++"):
            run([*prefix, "update-alternatives", "--set", "i686-w64-mingw32-"+compiler,
                 "/usr/bin/i686-w64-mingw32-"+compiler+"-win32"])
        identity = compiler_identity(plan)
        plan["compiler_identity"] = identity
        key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a") as stream:
                stream.write("compiler_key="+key+"\n")
    plan["discarded_cache_archives"] = removed
    plan["cache_exact_hit"] = os.environ.get("MIDAS_DEPENDENCY_CACHE_HIT") == "true"
    plan["reused_archive_bytes"] = reused
    (root/"verified-inputs.json").write_text(json.dumps(plan, indent=2)+"\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("prepare", "install"))
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--profile", choices=("tests", "windows"), default="windows")
    args = parser.parse_args()
    task_root = args.root.resolve()
    prepare(task_root, args.profile) if args.operation == "prepare" else install(task_root)
