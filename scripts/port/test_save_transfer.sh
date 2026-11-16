#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
mkdir -p build/save-transfer-tests
test_bin="$PWD/build/save-transfer-tests/driver"
# The 64-bit half is the build the host compiler makes natively. The -m32 half
# is the one CI turns on with MIDAS_SAVE_TEST_I686=1; a host that cannot build
# 64 bits (the i686 container) runs it without the variable, since nothing else
# would run there.
widths=$(harness_widths)
for width in $widths; do
    if harness_pending test_save_transfer "$width"; then
        echo "test_save_transfer: ${width}-bit half pending (LP64-003 task 11)"
        continue
    fi
    if [[ $width == 64 ]]; then
        "${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 \
            scripts/port/test_save_transfer_driver.cpp port/fast3d/save_backup.cpp \
            port/fast3d/save_restore.cpp -o "$test_bin"
        python3 scripts/port/test_save_transfer.py "$test_bin" 2>&1 | tee build/save-transfer-tests/result.log
    elif [ "${MIDAS_SAVE_TEST_I686:-0}" = 1 ] || [[ $widths != *64* ]]; then
        "${CXX:-g++}" -m32 -std=c++20 -Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -D_TIME_BITS=64 \
            scripts/port/test_save_transfer_driver.cpp port/fast3d/save_backup.cpp \
            port/fast3d/save_restore.cpp -o "${test_bin}32"
        python3 scripts/port/test_save_transfer.py "${test_bin}32" 2>&1 | tee build/save-transfer-tests/result32.log
    else
        echo "test_save_transfer: 32-bit half skipped (set MIDAS_SAVE_TEST_I686=1)"
        continue
    fi
    echo "test_save_transfer: PASS (${width}-bit)"
done
