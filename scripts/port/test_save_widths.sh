#!/usr/bin/env bash
# A campaign save moves between widths (LP64-006 Task 13). test_save_widths.c
# includes file2.c's save code and libultra_shim.c's EEPROM; built at each
# width harness_widths.sh offers, `write DIR` saves the campaign the driver
# defines into DIR/Data/Saves/eeprom.bin and `read DIR` loads it and prints
# every field. Each width writes and reads its own folder, then reads the
# other's: the two files must hold the same bytes, every read must print
# EXPECTED below and no read may change the file it read. At 64 bits `padding
# DIR` checks that a stored record's padding is 0 (check_padding).
# MIDAS_SAVE_WIDTHS_KEEP=DIR keeps each width's folder as DIR/32 and DIR/64
# (Task 13's game runs start from them). A compiler that offers one width
# (aarch64 has no -m32) runs that width and names what it skipped.
set -euo pipefail
cd "$(dirname "$0")/../.."
source scripts/port/harness_widths.sh
work=$(mktemp -d /tmp/midas-save-widths.XXXXXX)
trap 'rm -rf "$work"' EXIT
compiler=${CC:-gcc}

# The campaign as the driver writes it: folder 1 has twelve missions on Agent
# (100 + 37 s a mission), three on 00 Agent (600, 650, 700 s; the game's
# writer marks Secret Agent at the cap, 1023), cheats 3 and 11 and both
# volumes changed (music 0x3000 and effects 0x5800, stored >> 7); folder 3 has
# Dam on Agent in 321 s. Eighteen writes move folder 1 between slots 4 and 0.
read -r -d '' EXPECTED <<'TEXT' || [[ $? -eq 1 ]]
header 0x42
slot 0: flags 0x10 flag_007 0 music 96 sfx 176 options 0x003a cheats 0x08 0x08 0x00 padding 0
slot 1: flags 0x21 flag_007 0 music 255 sfx 255 options 0x003a cheats 0x00 0x00 0x00 padding 0
slot 2: free
slot 3: flags 0x63 flag_007 0 music 255 sfx 255 options 0x003a cheats 0x00 0x00 0x00 padding 0
slot 4: flags 0x4a flag_007 0 music 255 sfx 255 options 0x003a cheats 0x00 0x00 0x00 padding 0
folder 1: slot 0 rotation 2 bond 0 007 0 highest 12 earned 3 11
folder 1 agent: 100 137 174 211 248 285 322 359 396 433 470 507 0 0 0 0 0 0 0 0
folder 1 secret: 1023 1023 1023 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 1 00: 600 650 700 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 2: slot 1 rotation 0 bond 1 007 0 highest 0 earned none
folder 2 agent: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 2 secret: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 2 00: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 3: slot 4 rotation 1 bond 2 007 0 highest 1 earned none
folder 3 agent: 321 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 3 secret: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 3 00: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 4: slot 3 rotation 0 bond 3 007 0 highest 0 earned none
folder 4 agent: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 4 secret: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
folder 4 00: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
TEXT

# Builds the driver at WIDTH as $work/save-WIDTH.
build() {
    local width=$1
    "$compiler" $(harness_flags "$width" -msse2 -mfpmath=sse) \
        -std=gnu11 -fms-extensions -fcommon -funsigned-char -fno-strict-aliasing -w \
        -Werror=implicit-function-declaration \
        -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -DVERSION_US -DLANG_US -DREFRESH_NTSC -DNONMATCHING -DAVOID_UB -D_LANGUAGE_C \
        -I. -Iinclude -Iinclude/ultra64 -Iinclude/PR -Isrc -Isrc/game -Iport/include \
        scripts/port/test_save_widths.c src/game/crc.c port/src/n64_random.c -lm -o "$work/save-$width"
}

# Runs the WIDTH driver's read on FOLDER: it must print EXPECTED and leave the
# folder's eeprom.bin as it was. LABEL names the read in a failure.
check_read() {
    local width=$1 folder=$2 label=$3 save=$2/Data/Saves/eeprom.bin
    cp "$save" "$work/before.bin"
    "$work/save-$width" read "$folder" >"$work/read.txt"
    if ! cmp -s "$work/before.bin" "$save"; then
        echo "test_save_widths: FAIL ($label): the read changed $save" >&2
        cmp -l "$work/before.bin" "$save" >&2 || [[ $? -eq 1 ]]
        return 1
    fi
    if [[ $(<"$work/read.txt") != "$EXPECTED" ]]; then
        echo "test_save_widths: FAIL ($label): the read printed other text" >&2
        diff <(printf '%s\n' "$EXPECTED") "$work/read.txt" >&2 || [[ $? -eq 1 ]]
        return 1
    fi
    return 0
}

# Writes and reads the campaign at WIDTH in $work/folder-WIDTH.
own_width() {
    local width=$1 folder=$work/folder-$width
    build "$width"
    mkdir "$folder"
    "$work/save-$width" write "$folder"
    check_read "$width" "$folder" "${width}-bit"
    check_padding "$width"
    echo "test_save_widths: PASS (${width}-bit)"
}

# At 64 bits a stored folder record's padding is 0 whatever memory held there
# (file2.c, fileWriteSave). The 32-bit program is left as it was: it stores
# what its memory holds, which this campaign leaves at 0 (the file comparison).
check_padding() {
    local width=$1 folder=$work/padding-$1
    if [[ $width != 64 ]]; then
        return 0
    fi
    mkdir "$folder"
    "$work/save-$width" padding "$folder" >/dev/null
}

# Each width's file holds the same bytes, and each width reads the other's.
cross_widths() {
    if ! cmp "$work/folder-32/Data/Saves/eeprom.bin" "$work/folder-64/Data/Saves/eeprom.bin"; then
        echo "test_save_widths: FAIL: the 32-bit and 64-bit eeprom.bin differ" >&2
        return 1
    fi
    check_read 32 "$work/folder-64" "32 reads 64"
    check_read 64 "$work/folder-32" "64 reads 32"
    echo "test_save_widths: PASS (32 reads 64, 64 reads 32)"
}

# Copies each width's folder into MIDAS_SAVE_WIDTHS_KEEP, when it is set.
keep_folders() {
    local keep=${MIDAS_SAVE_WIDTHS_KEEP:-} width
    if [[ -z $keep ]]; then
        return 0
    fi
    for width in $widths; do
        rm -rf "${keep:?}/$width"
        mkdir -p "$keep"
        cp -R "$work/folder-$width" "$keep/$width"
        echo "test_save_widths: kept the ${width}-bit folder as $keep/$width"
    done
}

# On its own line, so a failure of harness_widths stops the harness (set -e).
widths=$(harness_widths)
for width in $widths; do
    own_width "$width"
done
if [[ $widths == *32* && $widths == *64* ]]; then
    cross_widths
else
    echo "test_save_widths: skipped the other width's half, the file comparison and the cross-width reads:" \
        "$compiler offers only the ${widths}-bit build"
fi
keep_folders
