/* ROM-free adapter for native codec/storage integration tests. */
#include "../../port/fast3d/save_backup.h"
#include <cstdio>
#include <stdexcept>

int main(int argc, char **argv)
{
    try {
        if (argc < 3) throw std::runtime_error("Expected command and path.");
        const std::string command = argv[1];
        if (command == "export") {
            const auto text = savebackup::encode(savebackup::collect(argv[2]));
            std::fwrite(text.data(), 1, text.size(), stdout);
        } else if (command == "validate") {
            savebackup::decode(savebackup::read(argv[2]));
        } else if (command == "stage" && argc == 4) {
            savebackup::stage(argv[2], savebackup::read(argv[3]));
        } else if (command == "apply") {
            savebackup::applyPending(argv[2]);
        } else throw std::runtime_error("Unknown command.");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
