#include "save_backup.h"
#include "third_party/picojson/picojson.h"
#include "third_party/picosha2/picosha2.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace savebackup {
static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

bool allowedPath(const std::string &path)
{
    if (path == "Data/Saves/eeprom.bin" || path == "midas.ini" || path == "gamecontrolleruser.txt") return true;
    const std::string devices = "Data/Devices/", profiles = "Data/Profiles/";
    if (path.starts_with(devices) && path.ends_with(".gedevice")) {
        const auto name = path.substr(devices.size(), path.size() - devices.size() - 9);
        return !name.empty() && name.size() <= 128 && name.find_first_not_of(
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos;
    }
    if (!path.starts_with(profiles) || !path.ends_with(".geprofile")) return false;
    const auto name = path.substr(profiles.size(), path.size() - profiles.size() - 10);
    if (name.empty() || name.size() > 23 || name == "none" || name.front() == ' '
        || name.back() == ' ' || name.back() == '.') return false;
    for (unsigned char c : name) if (c < 32 || std::string("/\\:*?\"<>|").find(c) != std::string::npos) return false;
    auto base = name.substr(0, name.find('.'));
    while (!base.empty() && base.back() == ' ') base.pop_back();
    std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) { return std::tolower(c); });
    return base != "con" && base != "prn" && base != "aux" && base != "nul"
        && !(base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt"))
             && base[3] >= '0' && base[3] <= '9');
}

void validate(const Files &files)
{
    require(files.size() <= 256, "The backup contains too many files.");
    std::set<std::string> paths;
#ifdef _WIN32
    std::vector<std::wstring> windowsPaths;
#endif
    size_t total = 0;
    for (const auto &file : files) {
        require(allowedPath(file.path) && paths.insert(file.path).second,
                "The backup contains an unsupported or duplicate path.");
        // Windows is case insensitive. Refuse aliases even on Linux so this
        // backup cannot overwrite the same file twice when moved to Windows.
        std::string folded = file.path;
        std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) { return std::tolower(c); });
        require(paths.insert("/" + folded).second, "The backup contains conflicting file names.");
#ifdef _WIN32
        // Windows also aliases non-ASCII case variants. Convert before any I/O
        // so invalid UTF-8 and colliding Unicode names cannot cause data loss.
        const auto native = std::filesystem::path(std::u8string(file.path.begin(), file.path.end())).native();
        for (const auto &other : windowsPaths)
            require(CompareStringOrdinal(native.data(), native.size(), other.data(), other.size(), TRUE) != CSTR_EQUAL,
                    "The backup contains conflicting Windows file names.");
        windowsPaths.push_back(native);
#endif
        total += file.bytes.size();
        require(file.bytes.size() <= 256 * 1024 && total <= 4 * 1024 * 1024, "The backup is too large.");
        require(file.path != "Data/Saves/eeprom.bin" || file.bytes.size() == 512,
                "The campaign save must contain exactly 512 bytes.");
    }
}

static const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64(const std::vector<unsigned char> &bytes)
{
    std::string result;
    for (size_t i = 0; i < bytes.size(); i += 3) {
        unsigned value = unsigned(bytes[i]) << 16;
        if (i + 1 < bytes.size()) value |= unsigned(bytes[i + 1]) << 8;
        if (i + 2 < bytes.size()) value |= bytes[i + 2];
        result += alphabet[(value >> 18) & 63]; result += alphabet[(value >> 12) & 63];
        result += i + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '=';
        result += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
    }
    return result;
}

static std::vector<unsigned char> unbase64(const std::string &text)
{
    require(text.size() <= 4 * ((256 * 1024 + 2) / 3) && text.size() % 4 == 0, "Invalid file data.");
    std::vector<unsigned char> bytes;
    for (size_t i = 0; i < text.size(); i += 4) {
        unsigned value = 0;
        for (size_t j = 0; j < 4; j++) {
            auto digit = alphabet.find(text[i + j]);
            require(digit != std::string::npos || text[i + j] == '=', "Invalid base64 data.");
            value = (value << 6) | (digit == std::string::npos ? 0 : unsigned(digit));
        }
        bytes.push_back(value >> 16);
        if (text[i + 2] != '=') bytes.push_back(value >> 8);
        if (text[i + 3] != '=') bytes.push_back(value);
    }
    require(base64(bytes) == text, "Invalid base64 padding.");
    return bytes;
}

std::string encode(const Files &files)
{
    validate(files);
    picojson::array records;
    for (const auto &file : files) {
        picojson::object record;
        record["path"] = picojson::value(file.path);
        record["data"] = picojson::value(base64(file.bytes));
        record["sha256"] = picojson::value(picosha2::hash256_hex_string(file.bytes));
        records.emplace_back(record);
    }
    std::time_t now = std::time(nullptr);
    char date[32];
    std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
    picojson::object backup;
    backup["format"] = picojson::value("project-midas-saves");
    backup["version"] = picojson::value(1.0);
    backup["createdAt"] = picojson::value(date);
    backup["files"] = picojson::value(records);
    return picojson::value(backup).serialize(true) + "\n";
}

Files decode(const std::string &text)
{
    require(text.size() <= maxText, "The backup is too large.");
    // Bound parser nesting before handing untrusted JSON to a recursive parser.
    int depth = 0; bool quoted = false, escaped = false;
    for (char c : text) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') require(++depth <= 16, "The backup is nested too deeply.");
        else if (c == '}' || c == ']') --depth;
    }
    picojson::value backup;
    auto end = text.begin();
    std::string error;
    end = picojson::parse(backup, end, text.end(), &error);
    require(error.empty() && std::all_of(end, text.end(), [](char c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
    }) && backup.is<picojson::object>(), "The backup is not valid JSON.");
    require(backup.get("format").is<std::string>() && backup.get("format").get<std::string>() == "project-midas-saves"
        && backup.get("version").is<double>() && backup.get("version").get<double>() == 1
        && backup.get("files").is<picojson::array>(), "Choose an Iron Sights or Project Midas version 1 save backup.");
    const auto &records = backup.get("files").get<picojson::array>();
    require(records.size() <= 256, "The backup contains too many files.");
    Files files;
    for (const auto &record : records) {
        require(record.is<picojson::object>(), "Invalid file record.");
        for (const auto *key : {"path", "data", "sha256"})
            require(record.get(key).is<std::string>(), "Invalid file record.");
        File file{record.get("path").get<std::string>(), unbase64(record.get("data").get<std::string>())};
        require(picosha2::hash256_hex_string(file.bytes) == record.get("sha256").get<std::string>(),
                "The backup is damaged: a file checksum does not match.");
        files.push_back(std::move(file));
    }
    validate(files);
    return files;
}
}
