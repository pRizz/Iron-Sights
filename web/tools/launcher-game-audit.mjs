// Private actual-ROM launcher acceptance. Never upload these captures/backups.
import { expect } from '@playwright/test';
import { launchAuditBrowser } from './browser-choice.mjs';
import { waitForDeployment } from './deployment-watch.mjs';
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { observeAudio, readAudio } from './audio-observer.mjs';

const [romPath, outputPath = '../build/web-ui/game', origin = 'http://127.0.0.1:4176'] =
    process.argv.slice(2);
if (!romPath) throw new Error('Usage: launcher-game-audit.mjs local-rom output-directory [origin]');
const output = resolve(outputPath);
await mkdir(output, { recursive: true });
const { browser, kind } = await launchAuditBrowser();
const context = await browser.newContext({ viewport: { width: 1440, height: 1100 } });
if (process.env.MIDAS_NO_GAMEPADS === '1') {
    await context.addInitScript(() => {
        Object.defineProperty(Navigator.prototype, 'getGamepads', {
            value: undefined,
            configurable: true,
        });
    });
}
const page = await context.newPage();
if (kind === 'chromium') {
    const focusSession = await context.newCDPSession(page);
    await focusSession.send('Emulation.setFocusEmulationEnabled', { enabled: false });
}
await page.addInitScript(observeAudio);
const errors = [];
const requests = [];
page.on('pageerror', (error) => {
    errors.push(String(error));
    console.log('PAGE ERROR: ' + String(error));
});
page.on('request', (request) => requests.push({ method: request.method(), url: request.url() }));
async function backup(name) {
    const downloadEvent = page.waitForEvent('download');
    await page.getByRole('button', { name: 'Download save backup' }).click();
    const download = await downloadEvent;
    const path = resolve(output, name);
    await download.saveAs(path);
    return JSON.parse(await readFile(path, 'utf8'));
}
try {
    await page.goto(origin);
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    await expect(page.getByRole('button', { name: 'Choose ROM', exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Play', exact: true })).toHaveCount(0);
    await page.locator('#rom-file').setInputFiles(resolve(romPath));
    await page.getByText('✓ Remembered for next time').waitFor({ timeout: 60_000 });
    await expect(page.getByRole('button', { name: 'Choose ROM', exact: true })).toHaveCount(0);
    await expect(page.getByText('Drop it here, or choose a file to get started.')).toHaveCount(0);
    await page.evaluate(() => {
        globalThis.midasSawRomLogo = false;
        globalThis.midasSawNoGamepads = false;
        const log = document.querySelector('pre');
        new MutationObserver(() => {
            if (log.textContent.includes('logo textures and vertices loaded from the player ROM'))
                globalThis.midasSawRomLogo = true;
            if (
                log.textContent.includes(
                    'browser gamepads unavailable; continuing with keyboard and mouse',
                )
            )
                globalThis.midasSawNoGamepads = true;
        }).observe(log, { childList: true, subtree: true, characterData: true });
    });
    await page.getByRole('button', { name: 'Play', exact: true }).click();
    await page.waitForFunction(
        () => document.querySelector('pre').textContent.includes('frame 240'),
        undefined,
        { timeout: 90_000 },
    );
    // Edit preferences while still in the startup menu, before timing can
    // carry faster engines from the briefing into gameplay/capture.
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    await page.keyboard.press('Backquote', { delay: 120 });
    await page.waitForTimeout(200);
    for (const command of [
        'set maxfps 59',
        'set player1_profile Web.Audit!',
        'set player1_crosshair_color 12AB34',
    ]) {
        await page.keyboard.type(command, { delay: 30 });
        await page.keyboard.press('Enter');
        await page.waitForTimeout(500);
    }
    await page.keyboard.press('Backquote', { delay: 120 });
    for (let step = 0; step < 8; step++) {
        if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
            await page.getByRole('button', { name: 'Resume game' }).click();
        await page.keyboard.press('Enter', { delay: 120 });
        await page.waitForTimeout(1800);
        await page.screenshot({ path: resolve(output, `menu-${step}.png`) });
    }
    await page.waitForTimeout(1200);
    await page.getByText('Saved on this device', { exact: true }).waitFor();
    const first = await backup('first-backup.json');
    await writeFile(resolve(output, 'first-log.txt'), await page.locator('pre').textContent());
    console.log('Saved paths: ' + first.files.map((file) => file.path).join(', '));
    assert.ok(
        await page.evaluate(() => globalThis.midasSawRomLogo),
        'ROM-backed title logo was not initialized',
    );
    if (process.env.MIDAS_NO_GAMEPADS === '1')
        assert.ok(
            await page.evaluate(() => globalThis.midasSawNoGamepads),
            'The unsupported-gamepad regression path was not exercised',
        );
    const campaign = first.files.find((file) => file.path === 'Data/Saves/eeprom.bin');
    assert.ok(campaign, 'The actual game did not create its campaign EEPROM');
    assert.equal(Buffer.from(campaign.data, 'base64').length, 512);
    assert.ok(
        first.files.some((file) => file.path === 'Data/Profiles/Web.Audit!.geprofile'),
        'Native profile did not save',
    );
    assert.match(
        Buffer.from(
            first.files.find((file) => file.path === 'midas.ini').data,
            'base64',
        ).toString(),
        /maxfps=59/,
    );
    // Continue through the normal briefing into Dam. No direct-stage diagnostics.
    // Downloading moves focus into the page controls, so resume the menu first.
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.getByRole('button', { name: 'Resume game' }).waitFor({ state: 'hidden' });
    await page.waitForTimeout(300);
    if (
        !(await page.locator('.player-toolbar').getByText('Playing', { exact: true }).isVisible())
    ) {
        await page.keyboard.press('p', { delay: 120 });
        await page.getByRole('button', { name: 'Resume game' }).click({ timeout: 60_000 });
    }
    await page.waitForFunction(() => !!document.pointerLockElement);
    await page.waitForTimeout(1000);
    await page.screenshot({ path: resolve(output, 'dam.png') });
    const audio = await page.evaluate(readAudio);
    assert.ok(
        audio.some(
            (sample) =>
                sample.state === 'running' && Math.max(sample.peak, sample.outputPeak) > 0.00001,
        ),
        'No game audio reached the output',
    );
    const other = await context.newPage();
    await other.goto('about:blank');
    await other.bringToFront();
    await page.getByRole('button', { name: 'Resume game' }).waitFor();
    await page.waitForTimeout(1500);
    const pausedLog = await page.locator('pre').textContent();
    await page.waitForTimeout(2000);
    assert.equal(
        await page.locator('pre').textContent(),
        pausedLog,
        'Paused game continued logging simulation frames',
    );
    await page.bringToFront();
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    await page.keyboard.press('p', { delay: 120 });
    await page.waitForFunction(() => !document.pointerLockElement);
    await page.waitForTimeout(1500);
    assert.ok(
        !(await page.getByRole('button', { name: 'Resume game' }).isVisible()),
        'Watch was mistaken for unexpected capture loss',
    );
    await page.screenshot({ path: resolve(output, 'watch.png') });
    const deployment = await waitForDeployment(page, origin);
    await page.keyboard.press('p', { delay: 120 });
    await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => !!document.pointerLockElement);
    await other.close();
    // Menu/mission startup may update the EEPROM; compare the final flushed data.
    // Pointer Lock routes all mouse clicks to the canvas until released.
    await page.evaluate(() => document.exitPointerLock());
    await page.getByRole('button', { name: 'Resume game' }).waitFor();
    await page.getByRole('button', { name: 'Pause', exact: true }).click();
    await page.getByText('Saved on this device', { exact: true }).waitFor();
    const beforeReload = await backup('before-reload.json');
    await page.reload();
    await page.keyboard.press('F2');
    await page.getByText('✓ Remembered for next time').waitFor({ timeout: 60_000 });
    await page.getByText('Saved on this device', { exact: true }).waitFor();
    // Restored saves can be ready before configuration downloads finish.
    await expect(page.getByRole('button', { name: 'Play GoldenEye' })).toBeEnabled({
        timeout: 60_000,
    });
    await expect(page.getByRole('button', { name: 'Play', exact: true })).toBeEnabled();
    await expect(page.getByRole('button', { name: 'Choose ROM', exact: true })).toHaveCount(0);
    await expect(page.getByText("Your ROM is loaded. Start when you're ready.")).toBeVisible();
    const second = await backup('after-reload.json');
    assert.deepEqual(
        second.files,
        beforeReload.files,
        'Campaign, profiles or settings changed across reload',
    );
    await page.locator('#backup-file').setInputFiles({
        name: 'actual-save-backup.json',
        mimeType: 'application/json',
        buffer: Buffer.from(JSON.stringify(beforeReload)),
    });
    await page
        .getByRole('dialog')
        .getByRole('button', { name: 'Restore backup', exact: true })
        .click();
    await page.getByText('Backup restored. Your saved data is ready.').waitFor();
    assert.deepEqual(
        (await backup('after-restore.json')).files,
        beforeReload.files,
        'Actual save backup round trip changed files',
    );
    await page.screenshot({ path: resolve(output, 'repeat-launch.png'), fullPage: true });
    await page.getByRole('button', { name: 'Forget remembered ROM' }).click();
    await page.getByText('The remembered ROM was removed. Saves are unchanged.').waitFor();
    await page.reload();
    await page.keyboard.press('F2');
    await page.getByText('Saved on this device', { exact: true }).waitFor({ timeout: 60_000 });
    await expect(page.getByRole('button', { name: 'Choose ROM', exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: 'Play', exact: true })).toHaveCount(0);
    assert.ok(
        await page.getByRole('button', { name: 'Play GoldenEye' }).isDisabled(),
        'The forgotten ROM was loaded again',
    );
    assert.deepEqual(
        (await backup('after-forget.json')).files,
        beforeReload.files,
        'Forgetting the actual ROM changed saves',
    );
    assert.ok(
        requests.every((request) => request.method === 'GET'),
        'Unexpected outgoing request method',
    );
    assert.deepEqual(errors, []);
    await writeFile(
        resolve(output, 'report.json'),
        JSON.stringify(
            {
                checks: 'first/repeat ROM, game files, capture/focus/watch passed',
                audio,
                deployment,
                requests,
                errors,
            },
            null,
            2,
        ),
    );
    console.log(
        'Launcher: ROM recall/removal, campaign/profile/settings reload, Dam capture/audio/focus/watch passed; no upload requests',
    );
} catch (error) {
    await page
        .evaluate(readAudio)
        .then((audio) =>
            writeFile(resolve(output, 'failure-audio.json'), JSON.stringify(audio, null, 2)),
        )
        .catch(() => {});
    await page.screenshot({ path: resolve(output, 'failure.png'), fullPage: true }).catch(() => {});
    await writeFile(
        resolve(output, 'failure-log.txt'),
        await page
            .locator('pre')
            .textContent()
            .catch(() => ''),
    );
    throw error;
} finally {
    await browser.close();
}
