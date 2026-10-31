#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build/save-transfer-tests
test_bin="$PWD/build/save-transfer-tests/driver"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 \
    scripts/port/test_save_transfer_driver.cpp port/fast3d/save_backup.cpp \
    port/fast3d/save_restore.cpp -o "$test_bin"
python3 scripts/port/test_save_transfer.py "$test_bin" 2>&1 | tee build/save-transfer-tests/result.log
if [ "${MIDAS_SAVE_TEST_I686:-0}" = 1 ]; then
    "${CXX:-g++}" -m32 -std=c++20 -Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 \
        scripts/port/test_save_transfer_driver.cpp port/fast3d/save_backup.cpp \
        port/fast3d/save_restore.cpp -o "${test_bin}32"
    python3 scripts/port/test_save_transfer.py "${test_bin}32" 2>&1 | tee build/save-transfer-tests/result32.log
fi
