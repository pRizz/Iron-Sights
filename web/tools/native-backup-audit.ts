/* Actual web codec <-> native driver interoperability, without ROM data. */
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { execFileSync } from 'node:child_process';
import { strict as assert } from 'node:assert';
import { createBackup, parseBackup, type UserFile } from '../src/backup';

const driver = resolve(process.argv[2]);
const maybeDesktopExport = process.argv[3];
const maybeWebOutput = process.argv[4];
const root = await mkdtemp(join(tmpdir(), 'iron-sights-transfer-'));
try {
    const files: UserFile[] = [
        {
            path: 'Data/Saves/eeprom.bin',
            bytes: Uint8Array.from({ length: 512 }, (_, i) => i % 256),
        },
        {
            path: 'Data/Profiles/Web!.geprofile',
            bytes: new TextEncoder().encode('native-web-profile\n'),
        },
        { path: 'Data/Devices/kbm.gedevice', bytes: new TextEncoder().encode('[binds]\n') },
        { path: 'midas.ini', bytes: new TextEncoder().encode('[display]\nalways_health=0\n') },
        { path: 'gamecontrolleruser.txt', bytes: new TextEncoder().encode('controller\n') },
    ];
    const input = join(root, 'browser.json');
    await writeFile(input, await createBackup(files, '2026-10-30T00:00:00Z'));
    execFileSync(driver, ['stage', root, input]);
    execFileSync(driver, ['apply', root]);
    for (const file of files)
        assert.deepEqual(new Uint8Array(await readFile(join(root, file.path))), file.bytes);
    const exported = execFileSync(driver, ['export', root], { encoding: 'utf8' });
    const restored = await parseBackup(exported);
    assert.deepEqual(
        restored.sort((a, b) => a.path.localeCompare(b.path)),
        files.sort((a, b) => a.path.localeCompare(b.path)),
    );
    // Native validation uses the web allowlist including watch-keyboard punctuation.
    for (const name of ['dots.name', 'Space Name', 'BOND!', 'René']) {
        const file = {
            path: `Data/Profiles/${name}.geprofile`,
            bytes: new Uint8Array([0, 255, 1]),
        };
        await writeFile(input, await createBackup([file], '2026-10-30T00:00:00Z'));
        execFileSync(driver, ['stage', root, input]);
        execFileSync(driver, ['apply', root]);
        const result = await parseBackup(
            execFileSync(driver, ['export', root], { encoding: 'utf8' }),
        );
        assert.deepEqual(result, [file]);
    }
    console.log(
        'PASS: actual web backups import natively; native exports parse in the web codec with exact bytes.',
    );
    console.log(
        'PASS: all portable file types, punctuation, UTF-8 profile names and full replacement.',
    );
    if (maybeDesktopExport) {
        const nativeFiles = await parseBackup(await readFile(maybeDesktopExport, 'utf8'));
        assert(nativeFiles.some((file) => file.path === 'Data/Saves/eeprom.bin'));
        console.log('PASS: actual desktop UI export parses in the web codec.');
        if (maybeWebOutput) {
            const incoming = nativeFiles.filter((file) => file.path !== 'midas.ini');
            incoming.push({
                path: 'midas.ini',
                bytes: new TextEncoder().encode(
                    '[display]\nfullscreen=window\ntexture_pack=0\nalways_health=0\n[diagnostics]\ncrash_reporting=0\n[input]\nmouse=1\n',
                ),
            });
            incoming.push({
                path: 'Data/Profiles/TRANSFER.geprofile',
                bytes: new TextEncoder().encode('[profile]\nname=TRANSFER\n'),
            });
            await writeFile(maybeWebOutput, await createBackup(incoming, new Date().toISOString()));
            console.log('Wrote web-codec backup for isolated desktop UI import acceptance.');
        }
    }
} finally {
    await rm(root, { recursive: true, force: true });
}
