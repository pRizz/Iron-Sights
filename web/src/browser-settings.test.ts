import { expect, test } from 'bun:test';
import { browserLookDefault } from './browser-settings';

test('partial browser bootstrap config explicitly defaults to Upright', () => {
    expect(browserLookDefault('[display]\nmaxfps=60\n[controls]\nstart=Return,P\n')).toBe(
        '[display]\nlook_updown=1\nmaxfps=60\n[controls]\nstart=Return,P\n',
    );
});
test('explicit Reverse, Upright and unrelated settings are preserved', () => {
    for (const value of [0, 1]) {
        const settings = `[display]\nlook_updown=${value}\n[controls]\nstart=Space\n`;
        expect(browserLookDefault(settings)).toBe(settings);
    }
});
test('default insertion is idempotent and handles CRLF or missing display sections', () => {
    for (const settings of [
        '[display]\r\nmaxfps=60\r\n',
        '[controls]\nstart=Return\n',
        '[display]',
    ]) {
        const next = browserLookDefault(settings);
        expect(next).toContain('look_updown=1');
        expect(browserLookDefault(next)).toBe(next);
    }
});
