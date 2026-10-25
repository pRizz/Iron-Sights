// Private audit access to existing exported counters. Never bundled in the site.
import assert from 'node:assert/strict';

export async function installGameProbe(page) {
    await page.route('**/runtime/*/ProjectMidas.mjs', async (route) => {
        const response = await route.fetch();
        const source = await response.text();
        await route.fulfill({
            response,
            body: source.replace(
                'export default createMidas;',
                `
export default async function(options) {
    const game = await createMidas(options);
    globalThis.midasAuditGame = game;
    return game;
}`,
            ),
        });
    });
}

export async function navigateToDam(page) {
    for (let step = 0; step < 14; step++) {
        if (await page.evaluate(() => globalThis.midasAuditGame._portWebGetStage() === 33)) break;
        if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
            await page.getByRole('button', { name: 'Resume game' }).click();
        await page.keyboard.press('Enter', { delay: 120 });
        await page.waitForTimeout(1800);
    }
    assert.equal(
        await page.evaluate(() => globalThis.midasAuditGame._portWebGetStage()),
        33,
        'Normal menus did not reach Dam',
    );
}

export async function captureGameplay(page) {
    if (await page.getByRole('button', { name: 'Resume game' }).isVisible())
        await page.getByRole('button', { name: 'Resume game' }).click();
    await page.waitForFunction(() => globalThis.midasAuditGame._portWebControlsLive(), undefined, {
        timeout: 90_000,
    });
    if (!(await page.evaluate(() => !!document.pointerLockElement)))
        await page.locator('#canvas').click();
    await page.waitForFunction(() => !!document.pointerLockElement);
}
