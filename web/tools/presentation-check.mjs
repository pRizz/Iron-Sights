import assert from 'node:assert/strict';

// Check the actual element bounds, so browser/native menu mouse coordinates agree.
export async function checkPresentation(page) {
    await page.waitForFunction(() => {
        const canvas = document.querySelector('#canvas');
        const stage = canvas.parentElement;
        const rect = canvas.getBoundingClientRect();
        const outer = stage.getBoundingClientRect();
        return (
            rect.width > 0 &&
            rect.height > 0 &&
            Math.abs(rect.width / rect.height - canvas.width / canvas.height) < 0.002 &&
            Math.abs(rect.left + rect.width / 2 - outer.left - outer.width / 2) < 1 &&
            Math.abs(rect.top + rect.height / 2 - outer.top - outer.height / 2) < 1 &&
            rect.width <= outer.width + 1 &&
            rect.height <= outer.height + 1
        );
    });
    const color = await page
        .locator('.game-stage')
        .evaluate((element) => getComputedStyle(element).backgroundColor);
    assert.equal(color, 'rgb(0, 0, 0)');
}
