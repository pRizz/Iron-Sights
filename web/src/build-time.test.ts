import { describe, expect, test } from 'bun:test';
import { formatBuildTime } from './build-time';

describe('build time display', () => {
    test('uses the build instant for daylight saving and local date rollover', () => {
        // Arrange / act.
        const winter = formatBuildTime('2026-01-15T04:30:00Z', 'en-US', 'America/Chicago');
        const summer = formatBuildTime('2026-07-15T04:30:00Z', 'en-US', 'America/Chicago');
        // Assert.
        expect(winter).toBe('Jan 14, 2026, 10:30 PM CST');
        expect(summer).toBe('Jul 14, 2026, 11:30 PM CDT');
    });

    test('honors locale and non-hour time zones', () => {
        // Arrange / act.
        const display = formatBuildTime('2026-10-28T23:30:00Z', 'en-GB', 'Asia/Kathmandu');
        // Assert.
        expect(display).toBe('29 Oct 2026, 05:15 GMT+5:45');
    });

    test('preserves invalid timestamps and labels missing metadata', () => {
        // Arrange / act / assert.
        expect(formatBuildTime('not a date')).toBe('not a date');
        expect(formatBuildTime('')).toBe('Unavailable');
    });
});
