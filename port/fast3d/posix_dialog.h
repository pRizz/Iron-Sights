/* FORK-092: native dialogs on Linux (and other POSIX desktops) through zenity,
 * else kdialog, for save transfer (save_transfer.cpp). */
#ifndef POSIX_DIALOG_H
#define POSIX_DIALOG_H
#include <string>
#include <vector>

namespace posix_dialog {

struct Result {
    bool started = false;  /* zenity or kdialog was found and ran */
    std::string output;    /* everything the tool printed */
    int status = 0;        /* waitpid status */
};

/* Runs zenity with zenityArgs, or kdialog with kdialogArgs when zenity is not on
 * PATH (argv[0] is added). The argument lists are passed as they are, never
 * through a shell. While the tool is open the caller's SDL events are pumped
 * (pump), so requests already made reach the display server and the game window
 * keeps answering. The tool runs with GTK's GL off (GDK_DEBUG=gl-disable,
 * GDK_DISABLE=gl): a GTK 4 dialog on NVIDIA's WSL driver segfaulted at exit and
 * lost its answer. */
Result run(const std::vector<std::string> &zenityArgs, const std::vector<std::string> &kdialogArgs,
    void (*pump)(void));

/* The tool closed normally with exit status 0 (OK, Yes, Save). */
bool accepted(const Result &result);

/* The line the tool printed for a chosen file, or "" when none was chosen.
 * Both tools print the path and a newline only when the player confirms; a
 * tool that then fails on its way out still chose that file. */
std::string chosenLine(const Result &result);

/* The environment a dialog runs with: env plus GTK's GL switches. */
std::vector<std::string> environment(const std::vector<std::string> &env);

}

#endif
