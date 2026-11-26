/* FORK-092: zenity / kdialog runner for save transfer; see posix_dialog.h. */
#include "posix_dialog.h"
#ifndef _WIN32
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace posix_dialog {

static std::string findOnPath(const char *name)
{
    const char *path = std::getenv("PATH");
    std::string dirs = path && *path ? path : "/usr/bin:/bin";
    size_t start = 0;
    while (start <= dirs.size()) {
        const size_t end = dirs.find(':', start);
        std::string dir = dirs.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (dir.empty()) dir = ".";
        const std::string candidate = dir + "/" + name;
        if (access(candidate.c_str(), X_OK) == 0) return candidate;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

std::vector<std::string> environment(const std::vector<std::string> &env)
{
    std::vector<std::string> out;
    std::string debug = "gl-disable";
    for (const auto &entry : env) {
        if (entry.rfind("GDK_DEBUG=", 0) == 0) {
            const std::string value = entry.substr(10);
            if (!value.empty()) debug = value + ",gl-disable";
            continue;
        }
        if (entry.rfind("GDK_DISABLE=", 0) == 0) continue;
        out.push_back(entry);
    }
    out.push_back("GDK_DEBUG=" + debug);  /* GTK 4.14 and earlier */
    out.push_back("GDK_DISABLE=gl");      /* GTK 4.16 and later */
    return out;
}

Result run(const std::vector<std::string> &zenityArgs, const std::vector<std::string> &kdialogArgs,
    void (*pump)(void))
{
    Result result;
    std::string tool = findOnPath("zenity");
    const std::vector<std::string> *args = &zenityArgs;
    const char *name = "zenity";
    if (tool.empty()) { tool = findOnPath("kdialog"); args = &kdialogArgs; name = "kdialog"; }
    if (tool.empty()) return result;

    /* Everything the child needs is built before fork; it only execs. */
    std::vector<std::string> envCopy;
    for (char **entry = environ; entry && *entry; entry++) envCopy.emplace_back(*entry);
    const std::vector<std::string> env = environment(envCopy);
    std::vector<char *> envp, argv;
    for (const auto &entry : env) envp.push_back(const_cast<char *>(entry.c_str()));
    envp.push_back(nullptr);
    argv.push_back(const_cast<char *>(name));
    for (const auto &arg : *args) argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);

    int pipes[2];
    if (pipe(pipes)) return result;
    const pid_t child = fork();
    if (child == 0) {
        close(pipes[0]); dup2(pipes[1], STDOUT_FILENO); close(pipes[1]);
        execve(tool.c_str(), argv.data(), envp.data());
        _exit(127);
    }
    close(pipes[1]);
    if (child < 0) { close(pipes[0]); return result; }
    result.started = true;
    if (pump) pump();
    char buffer[1024];
    for (;;) {
        pollfd ready = {pipes[0], POLLIN, 0};
        const int n = poll(&ready, 1, 50);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) break;
        if (n == 0) { if (pump) pump(); continue; }
        const ssize_t count = ::read(pipes[0], buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        result.output.append(buffer, (size_t) count);
    }
    close(pipes[0]);
    int waited;
    while ((waited = waitpid(child, &result.status, 0)) < 0 && errno == EINTR) {}
    if (waited < 0) result.status = -1;  /* unknown: never read as OK or Yes */
    if (WIFEXITED(result.status) && WEXITSTATUS(result.status) == 127) result.started = false;
    return result;
}

bool accepted(const Result &result)
{
    return result.started && WIFEXITED(result.status) && WEXITSTATUS(result.status) == 0;
}

std::string chosenLine(const Result &result)
{
    if (!result.started || result.output.empty() || result.output.back() != '\n') return {};
    return result.output.substr(0, result.output.size() - 1);
}

}
#else
typedef int posixDialogUnused;
#endif
