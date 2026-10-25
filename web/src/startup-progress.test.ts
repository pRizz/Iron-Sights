import { expect, test } from 'bun:test';
import { workProgress } from './startup-progress';

test('initialization reports completed work without inventing a percentage for unknown work', () => {
    expect(workProgress('Preparing graphics', 43, 87).maybePercent).toBe(49);
    expect(workProgress('Starting game', 0, 0).maybePercent).toBeNull();
});

test('progress remains bounded and rejects non-finite measurements', () => {
    expect(workProgress('Preparing', -2, 10).maybePercent).toBe(0);
    expect(workProgress('Preparing', 12, 10).maybePercent).toBe(100);
    expect(workProgress('Preparing', NaN, 10).maybePercent).toBeNull();
});
