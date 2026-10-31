#include "save_backup.h"
#include <fstream>
#include <set>
#include <stdexcept>

namespace savebackup {
namespace fs = std::filesystem;
static const char *pending = ".pending-save-restore.json";
static const char *previous = ".previous-save-restore.json";
static const char *journal = ".save-restore-in-progress";

static fs::path utf8Path(const std::string &text)
{
    return fs::path(std::u8string(text.begin(), text.end()));
}

std::string read(const fs::path &path, size_t limit)
{
    if (!fs::is_regular_file(fs::symlink_status(path)) || fs::file_size(path) > limit)
        throw std::runtime_error("The file is unsupported or too large.");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open the file.");
    // Also bound the read if another process grows the file after file_size.
    std::string result(limit + 1, '\0');
    input.read(result.data(), result.size());
    result.resize(static_cast<size_t>(input.gcount()));
    if (input.bad() || result.size() > limit) throw std::runtime_error("Could not read the complete file.");
    return result;
}

void write(const fs::path &path, const std::string &text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), text.size());
    output.close();
    if (!output) throw std::runtime_error("Could not write the file. Check free space and folder permissions.");
}

static void safePath(const fs::path &root, const fs::path &relative)
{
    auto path = root;
    for (const auto &part : relative) {
        path /= part;
        const auto status = fs::symlink_status(path);
        if (fs::is_symlink(status) || (fs::exists(status) && !fs::is_directory(status) && path != root / relative))
            throw std::runtime_error("Save folders must be regular directories, without links.");
        if (fs::exists(status) && path == root / relative && !fs::is_regular_file(status))
            throw std::runtime_error("A save path is not a regular file.");
    }
}

static std::vector<std::string> paths(const fs::path &root)
{
    std::vector<std::string> files;
    auto append = [&](const std::string &path) {
        safePath(root, utf8Path(path));
        if (!fs::exists(root / utf8Path(path))) return;
        files.push_back(path);
    };
    for (const char *path : {"Data/Saves/eeprom.bin", "midas.ini", "gamecontrolleruser.txt"}) append(path);
    for (const char *dir : {"Data/Profiles", "Data/Devices"}) {
        // Check the directory itself, too: do not traverse junctions/symlinks.
        safePath(root, fs::path(dir) / ".transfer-check");
        if (!fs::exists(root / dir)) continue;
        for (const auto &entry : fs::directory_iterator(root / dir)) {
            const auto name = entry.path().filename().u8string();
            auto path = std::string(dir) + "/" + std::string(name.begin(), name.end());
            if (allowedPath(path)) append(path);
            if (files.size() > 256) throw std::runtime_error("Too many save files.");
        }
    }
    return files;
}

Files collect(const fs::path &root)
{
    Files files;
    for (const auto &path : paths(root)) {
        auto text = read(root / utf8Path(path), 256 * 1024);
        files.push_back({path, {text.begin(), text.end()}});
    }
    validate(files);
    return files;
}

static void replace(const fs::path &root, const Files &files)
{
    // Remove before writing to handle case-only filename changes on Windows.
    // Enumerate names without decoding bytes so a partial write can roll back.
    for (const auto &path : paths(root)) fs::remove(root / utf8Path(path));
    for (const auto &file : files) {
        const auto path = utf8Path(file.path);
        safePath(root, path);
        fs::create_directories((root / path).parent_path());
        write(root / path, {file.bytes.begin(), file.bytes.end()});
    }
}

static void atomicWrite(const fs::path &path, const std::string &text)
{
    fs::path temp = path;
    temp += ".tmp";
    if (fs::exists(fs::symlink_status(path)) || fs::exists(fs::symlink_status(temp)))
        throw std::runtime_error("A save transfer is already pending. Restart Iron Sights to finish it.");
    try {
        write(temp, text);
        fs::rename(temp, path);
    } catch (...) {
        fs::remove(temp);
        throw;
    }
}

void stage(const fs::path &root, const std::string &text)
{
    decode(text); // Validate every record before touching the running user's files.
    atomicWrite(root / pending, text);
}

void applyPending(const fs::path &root)
{
    // A crash before a marker rename leaves an uncommitted temporary file.
    // No live saves have changed yet; discard it so the next attempt can run.
    for (const char *name : {pending, previous, journal}) {
        fs::path temp = root / name;
        temp += ".tmp";
        if (!fs::exists(fs::symlink_status(temp))) continue;
        if (!fs::is_regular_file(fs::symlink_status(temp)))
            throw std::runtime_error("A save transfer temporary path is not a regular file.");
        fs::remove(temp);
    }
    // A durable pre-image lets the next launch recover an interrupted restore.
    if (fs::exists(root / journal)) {
        replace(root, decode(read(root / previous)));
        fs::remove(root / journal);
    }
    if (!fs::exists(root / pending)) return;
    const auto files = decode(read(root / pending));
    fs::remove(root / previous);
    atomicWrite(root / previous, encode(collect(root)));
    atomicWrite(root / journal, "restore\n");
    try {
        replace(root, files);
    } catch (...) {
        replace(root, decode(read(root / previous)));
        fs::remove(root / journal);
        throw;
    }
    fs::remove(root / journal); // commit; replaying pending after a crash is safe
    fs::remove(root / pending);
    fs::remove(root / previous);
}
}
