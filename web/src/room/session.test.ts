import { expect, test } from 'bun:test';
import { restoredEntry, shouldAutoLaunch, type EntryMode } from './session';

test('only a successfully restored ROM selects automatic startup', () => {
    expect(restoredEntry(true)).toBe('cached');
    expect(restoredEntry(false)).toBe('room');
});
test('cached startup waits for readiness and respects save-management controls', () => {
    expect(shouldAutoLaunch('cached', true, false, false)).toBe(true);
    expect(shouldAutoLaunch('cached', false, false, false)).toBe(false);
    expect(shouldAutoLaunch('cached', true, true, false)).toBe(false);
    expect(shouldAutoLaunch('cached', true, false, true)).toBe(false);
});
for (const mode of ['restoring', 'room', 'game', 'returning'] as EntryMode[]) {
    test(`${mode} never automatically enters the game`, () => {
        expect(shouldAutoLaunch(mode, true, false, false)).toBe(false);
    });
}
