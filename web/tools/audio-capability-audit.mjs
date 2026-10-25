// Isolate the browser/host audio backend from the game's mixer and SDL.
import { createServer } from 'node:http';
import { launchAuditBrowser } from './browser-choice.mjs';

const server = createServer((request, response) =>
    response.end('<!doctype html><button>Audio probe</button>'),
);
await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
const { browser, kind } = await launchAuditBrowser();
try {
    const page = await browser.newPage();
    const errors = [];
    page.on('console', (message) => {
        if (message.type() === 'error') errors.push(message.text());
    });
    page.on('pageerror', (error) => errors.push(String(error)));
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.evaluate(() => {
        document.querySelector('button').onclick = () => {
            const Context = window.AudioContext || window.webkitAudioContext;
            if (!Context) {
                globalThis.midasAudioProbe = { error: 'Web Audio is unavailable' };
                return;
            }
            const context = new Context();
            const analyser = context.createAnalyser();
            const oscillator = context.createOscillator();
            oscillator.connect(analyser);
            analyser.connect(context.destination);
            oscillator.start();
            globalThis.midasAudioProbe = { context, analyser, error: null };
            void context.resume().catch((error) => {
                globalThis.midasAudioProbe.error = String(error);
            });
        };
    });
    await page.getByRole('button').click();
    await page.waitForTimeout(1500);
    const report = await page.evaluate(() => {
        const { context, analyser, error } = globalThis.midasAudioProbe || {};
        if (!context || !analyser)
            return {
                browser: navigator.userAgent,
                state: 'unavailable',
                error: error || 'Audio initialization failed',
                peak: 0,
            };
        const samples = new Float32Array(analyser.fftSize);
        analyser.getFloatTimeDomainData(samples);
        return {
            browser: navigator.userAgent,
            state: context.state,
            error,
            peak: Math.max(...samples.map(Math.abs)),
        };
    });
    console.log(JSON.stringify({ kind, ...report, errors }));
    if (report.peak < 0.01) process.exitCode = 1;
} finally {
    await browser.close();
    await new Promise((resolve) => server.close(resolve));
}
