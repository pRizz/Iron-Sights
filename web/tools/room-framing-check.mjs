import assert from 'node:assert/strict';
import { resolve } from 'node:path';

// Render the real production scene at its endpoint without starting the engine.
export async function checkRoomFraming(browser, origin, output) {
    const context = await browser.newContext({ viewport: { width: 1440, height: 900 } });
    try {
        const page = await context.newPage();
        await page.goto(origin);
        await page.keyboard.press('F2');
        await page.locator('.site-header').waitFor();
        await page.evaluate(async () => {
            const manifest = await (
                await fetch(new URL('frontend-manifest.json', document.baseURI))
            ).json();
            const module = await import(
                new URL(manifest['src/room/scene.ts'].file, document.baseURI).href
            );
            const canvas = document.createElement('canvas');
            canvas.className = 'framing-fixture';
            canvas.style.cssText =
                'position:fixed;inset:0;width:100%;height:100%;z-index:100;background:black';
            document.body.appendChild(canvas);
            globalThis.framingScene = await module.createRoomScene(canvas);
        });
        for (const [width, height, name] of [
            [1440, 900, 'wide'],
            [800, 1000, 'portrait'],
        ]) {
            await page.setViewportSize({ width, height });
            const corners = await page.evaluate(() => {
                globalThis.framingScene.render(
                    { phase: 'complete', elapsed: 0 },
                    0,
                    false,
                    'Starting GoldenEye',
                );
                return JSON.parse(
                    document.querySelector('.framing-fixture').dataset.roomScreenCorners,
                );
            });
            const edges = corners.flatMap(([x, y]) => [Math.abs(x), Math.abs(y)]);
            assert.ok(
                edges.every((edge) => edge <= 1.00001),
                'TV endpoint crops the screen',
            );
            assert.ok(
                Math.abs(Math.max(...edges) - 1) < 0.00001,
                'TV endpoint leaves space on both axes',
            );
            await page.screenshot({ path: resolve(output, `tv-endpoint-${name}.png`) });
        }
        await page.evaluate(() => globalThis.framingScene.dispose());
    } finally {
        await context.close();
    }
}
