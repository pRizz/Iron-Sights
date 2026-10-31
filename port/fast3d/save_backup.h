/* Portable v1 backup shared with web/src/backup.ts. No engine/SDK headers. */
#ifndef SAVE_BACKUP_H
#define SAVE_BACKUP_H
#include <filesystem>
#include <string>
#include <vector>
namespace savebackup {
struct File { std::string path; std::vector<unsigned char> bytes; };
using Files = std::vector<File>;
constexpr size_t maxText = 8 * 1024 * 1024;
bool allowedPath(const std::string &path);
void validate(const Files &files);
std::string encode(const Files &files);
Files decode(const std::string &text);
std::string read(const std::filesystem::path &path, size_t limit = maxText);
void write(const std::filesystem::path &path, const std::string &text);
Files collect(const std::filesystem::path &root);
void stage(const std::filesystem::path &root, const std::string &text);
void applyPending(const std::filesystem::path &root);
}
#endif
