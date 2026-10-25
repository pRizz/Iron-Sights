import { chromium, firefox, webkit } from '@playwright/test';

export async function launchAuditBrowser() {
    const choice = process.env.MIDAS_BROWSER || 'chromium';
    const headless = process.env.MIDAS_HEADED !== '1';
    if (choice === 'firefox' || choice === 'webkit') {
        return {
            kind: choice,
            browser: await (choice === 'firefox' ? firefox : webkit).launch({ headless }),
        };
    }
    if (!['chromium', 'chrome', 'msedge'].includes(choice))
        throw new Error('Unknown audit browser: ' + choice);
    return {
        kind: 'chromium',
        browser: await chromium.launch({
            headless,
            ...(choice === 'chromium' ? {} : { channel: choice }),
            args: ['--enable-unsafe-swiftshader'],
        }),
    };
}
