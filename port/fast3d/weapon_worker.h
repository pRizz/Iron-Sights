#ifndef MIDAS_WEAPON_WORKER_H
#define MIDAS_WEAPON_WORKER_H
/* The weapon icon worker process (FORK-019), Windows and Linux only. */
#include <filesystem>
#include <functional>
namespace weapon_worker {
/* Runs fn while holding the per-user cache lock (another launch may be
 * preparing); false when the lock could not be taken. */
bool withLock(const std::filesystem::path &folder, const std::function<void()> &fn);
/* Removes worker-* folders a launch that quit mid-preparation left behind;
 * call under the lock. */
void removeStale(const std::filesystem::path &folder);
/* Runs this executable as the worker; true when it exited cleanly, the cache
 * then written. */
bool generate(const std::filesystem::path &cache, const char *rom, unsigned romFingerprint);
}
#endif
