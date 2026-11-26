/* FORK-092: posix_dialog's zenity / kdialog runner, against fake tools on PATH
 * (test_posix_dialog.sh writes them). The stand-in SDL pump only counts. */
#include "posix_dialog.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <vector>

static int pumps;
static void pump(void) { pumps++; }
static int failures;

static void expect(bool ok, const char *what, const std::string &detail = "")
{
    if (ok) return;
    std::printf("FAIL: %s %s\n", what, detail.c_str());
    failures++;
}

static posix_dialog::Result runWith(const char *path, const std::vector<std::string> &zenity,
    const std::vector<std::string> &kdialog)
{
    setenv("PATH", path, 1);
    return posix_dialog::run(zenity, kdialog, pump);
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    const std::string bin = argv[1];

    /* A chosen file: printed with a newline, exit 0; arguments reach the tool
     * as they are (a space and a quote, never a shell). */
    auto chosen = runWith((bin + "/ok").c_str(), {"--file-selection", "--filename=/tmp/a b'c.json"}, {});
    expect(chosen.started && posix_dialog::accepted(chosen), "ok: accepted");
    expect(posix_dialog::chosenLine(chosen) == "/tmp/a b'c.json", "ok: argument passed whole", chosen.output);

    /* The tool prints its answer, then dies of SIGSEGV (GTK 4 on NVIDIA's WSL
     * driver): the file still counts, the dialog is not accepted. */
    pumps = 0;
    auto crashed = runWith((bin + "/crash").c_str(), {"/tmp/x.json"}, {});
    expect(posix_dialog::chosenLine(crashed) == "/tmp/x.json", "crash: line kept", crashed.output);
    expect(!posix_dialog::accepted(crashed) && WIFSIGNALED(crashed.status), "crash: not accepted");
    expect(pumps > 0, "crash: pumped while the tool ran");

    /* Cancel: nothing printed, exit 1. */
    auto cancelled = runWith((bin + "/cancel").c_str(), {}, {});
    expect(cancelled.started && !posix_dialog::accepted(cancelled), "cancel: not accepted");
    expect(posix_dialog::chosenLine(cancelled).empty(), "cancel: no line");

    /* No zenity: kdialog with its own arguments. */
    auto kde = runWith((bin + "/kdeonly").c_str(), {"--zenity-only"}, {"--getopenfilename", "/home"});
    expect(posix_dialog::chosenLine(kde) == "kdialog --getopenfilename /home", "kdialog fallback", kde.output);

    /* Neither tool. */
    auto none = runWith((bin + "/none").c_str(), {}, {});
    expect(!none.started && !posix_dialog::accepted(none), "none: not started");

    /* GTK's GL is off for the tool; an existing GDK_DEBUG keeps its flags. */
    setenv("GDK_DEBUG", "", 1);
    auto env = runWith((bin + "/env").c_str(), {}, {});
    expect(env.output == "gl-disable gl\n", "env: GL off", env.output);
    setenv("GDK_DEBUG", "interactive", 1);
    setenv("GDK_DISABLE", "vulkan", 1);
    env = runWith((bin + "/env").c_str(), {}, {});
    expect(env.output == "interactive,gl-disable gl\n", "env: flags kept", env.output);

    if (failures) return 1;
    std::printf("7 cases\n");
    return 0;
}
