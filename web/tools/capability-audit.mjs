// ROM-free initialization/capability evidence; engine identity is not OS support.
import { launchAuditBrowser } from './browser-choice.mjs';

const { browser, kind } = await launchAuditBrowser();
try {
    const page = await browser.newPage();
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    const start = Date.now();
    await page.goto(process.argv[2] || 'http://127.0.0.1:4176');
    await page.waitForFunction(
        () =>
            document.body.textContent.includes('Saved on this device') ||
            document.querySelector('.error-panel'),
        undefined,
        { timeout: 60_000 },
    );
    const report = await page.evaluate(() => {
        const gl = document.createElement('canvas').getContext('webgl2');
        const debug = gl?.getExtension('WEBGL_debug_renderer_info');
        return {
            browser: navigator.userAgent,
            webgl2: !!gl,
            locks: !!navigator.locks,
            pointerLock: 'requestPointerLock' in HTMLCanvasElement.prototype,
            renderer: debug ? gl.getParameter(debug.UNMASKED_RENDERER_WEBGL) : null,
            error: document.querySelector('.error-panel')?.textContent || null,
            ready: document.body.textContent.includes('Saved on this device'),
        };
    });
    console.log(JSON.stringify({ kind, ...report, initializationMs: Date.now() - start, errors }));
    if (!report.ready || errors.length) process.exitCode = 1;
} finally {
    await browser.close();
}
