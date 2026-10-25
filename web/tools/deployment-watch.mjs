// Opt-in private acceptance: keep the watch animating across a real deployment.
const frame = async (page) => {
    const lines = await page.locator('pre').textContent();
    return Number([...lines.matchAll(/ge: frame (\d+)/g)].at(-1)?.[1] || 0);
};

export async function waitForDeployment(page, origin) {
    if (process.env.MIDAS_WAIT_FOR_DEPLOY !== '1') return null;
    const current = async () => {
        const url = new URL('runtime-manifest.json', origin.endsWith('/') ? origin : origin + '/');
        url.searchParams.set('update-audit', String(Date.now()));
        const response = await page.request.get(url.href, {
            headers: { 'Cache-Control': 'no-cache' },
        });
        if (!response.ok()) throw new Error('The deployed manifest could not be read');
        const value = await response.json();
        if (value.schema !== 1 || !/^[a-f0-9]{16}$/.test(value.version))
            throw new Error('Invalid deployed manifest');
        return { version: value.version, commit: value.build.commit };
    };
    const from = await current();
    const initialFrame = await frame(page);
    const started = Date.now();
    console.log('READY_FOR_DEPLOY_UPDATE ' + JSON.stringify({ from, initialFrame }));
    while (Date.now() - started < 15 * 60_000) {
        await page.waitForTimeout(2000);
        const to = await current();
        if (to.version === from.version) continue;
        await page.waitForTimeout(1200);
        const finalFrame = await frame(page);
        if (finalFrame <= initialFrame)
            throw new Error('The active game stopped during deployment');
        const report = { from, to, initialFrame, finalFrame, elapsedMs: Date.now() - started };
        console.log('DEPLOY_UPDATE_CONTINUED ' + JSON.stringify(report));
        return report;
    }
    throw new Error('No new deployed runtime appeared during the update acceptance window');
}
