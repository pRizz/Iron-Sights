import { expect, test } from 'bun:test';
import { paintCrtScreen } from './loading-screen';

function drawing(seconds: number, percent: number | null, reduced = false) {
    const text: string[] = [];
    const rectangles: number[][] = [];
    const context = {
        fillText: (value: string) => text.push(value),
        fillRect: (...values: number[]) => rectangles.push(values),
        strokeRect: () => {},
    } as unknown as CanvasRenderingContext2D;
    paintCrtScreen(context, seconds, 'Starting GoldenEye', true, reduced, {
        label: 'Preparing shaders',
        maybePercent: percent,
    });
    return { text, bar: rectangles[1] };
}

test('CRT progress paints the actual stage and measured percentage', () => {
    const result = drawing(0, 25);
    expect(result.text).toContain('PREPARING SHADERS');
    expect(result.text).toContain('25%');
    expect(result.bar).toEqual([66, 294, 127, 16]);
});

test('indeterminate work has no invented percentage and reduced motion holds the bar', () => {
    expect(drawing(0, null).text).toContain('PLEASE WAIT');
    expect(drawing(0, null).text.some((value) => value.includes('%'))).toBe(false);
    expect(drawing(0, null).bar).not.toEqual(drawing(1, null).bar);
    expect(drawing(0, null, true).bar).toEqual(drawing(1, null, true).bar);
});
