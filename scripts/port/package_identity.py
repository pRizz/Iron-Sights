"""Package identity for the in-game updater (UPD-001, scripts/port/self_update_plan.md).

Every player package carries `package.txt` beside its executable, naming the package's flavor,
and its own `SHA256SUMS.txt`, listing every other file in the package with its SHA-256. The
updater reads the first to pick the matching release asset, and the second as the list of
program files it may replace; nothing else in the install is ever touched.
"""
import hashlib

NAME = "package.txt"
SUMS = "SHA256SUMS.txt"
SCHEMA = 1
# Each flavor and the release assets it updates from: the package, the checksum file that names
# it, and the build-info file the updater compares build numbers with.
FLAVORS = {
    "windows-x86-zip": ("Iron-Sights-windows-x86.zip", "SHA256SUMS.txt", "build-info.json"),
    "windows-x64-zip": ("Iron-Sights-windows-x64.zip", "SHA256SUMS-windows-x64.txt", "windows-x64-build-info.json"),
    "linux-i686-tar": ("Iron-Sights-linux-i686.tar.gz", "SHA256SUMS-linux.txt", "linux-build-info.json"),
    "linux-i686-appimage": ("Iron-Sights-linux-i686.AppImage", "SHA256SUMS-linux.txt", "linux-build-info.json"),
    "steamdeck-i686": ("Iron-Sights-steamdeck-i686.zip", "SHA256SUMS-linux.txt", "linux-build-info.json"),
    "linux-x86_64-tar": ("Iron-Sights-linux-x86_64.tar.gz", "SHA256SUMS-linux-x86_64.txt",
                         "linux-x86_64-build-info.json"),
    "linux-x86_64-appimage": ("Iron-Sights-linux-x86_64.AppImage", "SHA256SUMS-linux-x86_64.txt",
                              "linux-x86_64-build-info.json"),
    "steamdeck-x86_64": ("Iron-Sights-steamdeck-x86_64.zip", "SHA256SUMS-linux-x86_64.txt",
                         "linux-x86_64-build-info.json"),
}


def text(flavor):
    """Return package.txt's bytes for FLAVOR; raise on a flavor the updater does not know."""
    if flavor not in FLAVORS:
        raise ValueError(f"unknown package flavor {flavor!r}")
    return (f"# Iron Sights package identity, read by the in-game updater. Do not edit.\n"
            f"schema={SCHEMA}\nflavor={flavor}\n").encode("ascii")


def read_flavor(data):
    """Return the flavor that package.txt's bytes DATA name; raise when they are not a known one."""
    fields = dict(line.split("=", 1) for line in data.decode("ascii").splitlines()
                  if line and not line.startswith("#"))
    if fields.get("schema") != str(SCHEMA):
        raise ValueError(f"{NAME} has schema {fields.get('schema')!r}, not {SCHEMA}")
    flavor = fields.get("flavor", "")
    if flavor not in FLAVORS:
        raise ValueError(f"{NAME} names unknown flavor {flavor!r}")
    return flavor


def sums(files):
    """Return SHA256SUMS.txt's text for FILES, a mapping of package-relative path to bytes."""
    return "".join(f"{hashlib.sha256(data).hexdigest()}  {name}\n" for name, data in sorted(files.items())
                   if name != SUMS)


def check(files, flavor):
    """Raise AssertionError unless FILES (package-relative path -> bytes, the package's top folder
    removed) hold package.txt naming FLAVOR and a SHA256SUMS.txt that lists every other file with
    its hash, and nothing else."""
    assert NAME in files, f"the package has no {NAME}"
    named = read_flavor(files[NAME])
    assert named == flavor, f"{NAME} names {named}, not {flavor}"
    assert SUMS in files, f"the package has no {SUMS}"
    listed = {}
    for line in files[SUMS].decode("utf-8").splitlines():
        digest, separator, name = line.partition("  ")
        assert separator and len(digest) == 64, f"{SUMS} has a malformed line: {line!r}"
        listed[name] = digest
    actual = {name: hashlib.sha256(data).hexdigest() for name, data in files.items() if name != SUMS}
    missing, extra = sorted(set(actual) - set(listed)), sorted(set(listed) - set(actual))
    assert not missing and not extra, f"{SUMS} omits {missing} and lists absent {extra}"
    wrong = sorted(name for name in actual if actual[name] != listed[name])
    assert not wrong, f"{SUMS} has the wrong hash for {wrong}"


def strip_folder(files, folder):
    """Return FILES with each path's leading FOLDER/ removed; raise if one lies outside it."""
    prefix = folder + "/"
    outside = sorted(name for name in files if not name.startswith(prefix))
    assert not outside, f"entries outside {prefix}: {outside}"
    return {name[len(prefix):]: data for name, data in files.items()}
