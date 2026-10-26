import { expect, test } from 'bun:test';
import { paintCrtScreen } from './loading-screen';
import { MISSION_THEME } from '../mission-theme';

function drawing(seconds: number, percent: number | null, reduced = false) {
    const text: string[] = [];
    const bars: number[][] = [];
    const context = {
        fillStyle: '',
        fillText: (value: string) => text.push(value),
        fillRect: (...values: number[]) => {
            if (context.fillStyle === MISSION_THEME.green) bars.push(values);
        },
        strokeRect: () => {},
    } as unknown as CanvasRenderingContext2D;
    paintCrtScreen(context, seconds, 'Starting GoldenEye', true, reduced, {
        label: 'Preparing shaders',
        maybePercent: percent,
    });
    return { text, bars };
}

test('CRT progress paints the actual stage and measured percentage', () => {
    const result = drawing(0, 25);
    expect(result.text).toContain('PREPARING SHADERS');
    expect(result.text).toContain('25%');
    expect(result.text).toContain('MISSION PREPARATION');
    expect(result.bars).toHaveLength(5); // 25% of the 20-cell gauge.
    expect(result.bars.every((bar) => bar[2] === 23.8)).toBe(true);
    expect(drawing(0, 0).bars).toHaveLength(0);
    expect(drawing(0, 100).bars).toHaveLength(20);
});

test('indeterminate work has no invented percentage and reduced motion holds the bar', () => {
    expect(drawing(0, null).text).toContain('PLEASE WAIT');
    expect(drawing(0, null).text.some((value) => value.includes('%'))).toBe(false);
    expect(drawing(0, null).bars).not.toEqual(drawing(1, null).bars);
    expect(drawing(0, null, true).bars).toEqual(drawing(1, null, true).bars);
});
