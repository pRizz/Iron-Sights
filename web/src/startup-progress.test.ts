import { expect, test } from 'bun:test';
import { STARTUP_STAGES, stageNumber, stageTrack, workProgress } from './startup-progress';

test('initialization reports completed work without inventing a percentage for unknown work', () => {
    expect(workProgress('Preparing graphics', 43, 87).maybePercent).toBe(49);
    expect(workProgress('Starting game', 0, 0).maybePercent).toBeNull();
});

test('progress remains bounded and rejects non-finite measurements', () => {
    expect(workProgress('Preparing', -2, 10).maybePercent).toBe(0);
    expect(workProgress('Preparing', 12, 10).maybePercent).toBe(100);
    expect(workProgress('Preparing', NaN, 10).maybePercent).toBeNull();
});

test('every reported startup label maps to one stage in launch order', () => {
    const order = [
        'Checking browser',
        'Opening local saves',
        'Downloading game',
        'Compiling WebAssembly',
        'Restoring local saves',
        'Checking saved cartridge',
        'Preparing cartridge',
        'Opening cartridge',
        'Preparing graphics',
        'Preparing weapon icons',
        'Starting GoldenEye',
        'Ready to play',
    ];
    const positions = order.map((label) => stageTrack(label)?.current);
    expect(positions).toEqual([0, 0, 1, 2, 2, 3, 3, 3, 4, 5, 6, 7]);
    expect(positions.every((value, index) => index === 0 || value! >= positions[index - 1]!)).toBe(
        true,
    );
    expect(stageTrack('Preparing graphics')).toEqual({ current: 4, total: 7, name: 'Graphics' });
    expect(stageTrack('Preparing weapon icons')).toEqual({ current: 5, total: 7, name: 'Icons' });
});

test('the stage track hides for unknown or paused labels and caps its number when done', () => {
    expect(stageTrack('Startup paused · resume below')).toBeUndefined();
    expect(stageTrack('Something new')).toBeUndefined();
    expect(stageNumber(stageTrack('Checking browser')!)).toBe(1);
    expect(stageNumber(stageTrack('Ready to play')!)).toBe(STARTUP_STAGES.length);
});
