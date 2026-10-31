# Desktop and web save transfer

On the four-campaign **Files** screen, choose **Export** to write a JSON
backup. On the other desktop installation or web instance, choose **Import**,
select that file, then confirm **Import and restart**. Export first if
you want to keep the destination's current progress.

The suggested filename includes local date and time, for example
`iron-sights-saves-2026-10-30T19-45-09.json`, on Windows, Linux and web.
Desktop Save dialogs let you edit it. Backup metadata keeps its ISO UTC time.

Import/Export form the leftmost action column, followed by Copy/Options and
Erase/Exit Game.

Import replaces all supported files, rather than merging individual campaigns.
The version-1 `project-midas-saves` format is shared with the existing browser
backup controls and carries:

- All four campaign slots in `Data/Saves/eeprom.bin`.
- Named `.geprofile` profiles and `.gedevice` device bindings.
- `midas.ini` settings and `gamecontrolleruser.txt` custom controller mappings.

ROMs, textures, logs, profile statistics and suspend points are excluded.
Suspend points remain tied to their original executable. Each destination
requires its own ROM. Imported settings also replace display/input preferences.

Windows uses its standard Open/Save dialogs. Linux requires `zenity` or
`kdialog` on `PATH`; install one through your distribution's package manager if
neither is available. Canceling a picker or confirmation changes no saves.

The desktop adapter validates paths, file counts/sizes, base64 and SHA-256
before confirmation. Confirmed imports are staged outside the export allowlist.
The running game's files and cached EEPROM stay intact until shutdown; a fresh
process applies the import before settings, profiles or EEPROM are loaded.
A pre-import backup and restore journal support retry after interruption or a
write failure. An unrecoverable startup error stops initialization and retains
the recovery files; resolve the reported folder/space problem and reopen.
Do not run multiple desktop instances against the same save folder during import.

## Verification

Run the ROM-free codec/storage checks from the repository root:

```sh
MIDAS_SAVE_TEST_I686=1 bash scripts/port/test_save_transfer.sh
cd web
bun install --frozen-lockfile
bun tools/native-backup-audit.ts ../build/save-transfer-tests/driver
```

The first command checks validation, binary round trips, replacement, staging,
interrupted restore, temporary-marker cleanup and linked-directory rejection.
The second uses the actual web codec in both directions. A MinGW-built driver
can also be passed as `driver.exe` on Windows. These tests use temporary synthetic
saves; they do not read a player's campaign data or ROM.

To include an export produced by the desktop UI, append its JSON path and an
optional output JSON path to `native-backup-audit.ts`. The audit checks that
export in the real web codec and writes a web-generated backup with a test
profile/settings for the isolated native acceptance flow.

For native acceptance, use a separate executable/runtime directory with its own
settings and saves. Check mouse/controller activation of both Files buttons,
picker cancellation, invalid data, confirmation cancellation, export and import
restart. Compare exported EEPROM/profile bytes with the input backup after
relaunch. Check both widescreen and 4:3 layouts. Remove the temporary executable
after closing the audit; retain useful logs/captures in ignored build folders.
