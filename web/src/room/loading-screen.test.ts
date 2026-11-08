import { expect, test } from 'bun:test';
import { crtScreenKey, paintCrtScreen } from './loading-screen';
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

test('the CRT repaint key changes exactly when the painted screen would', () => {
    const measured = { label: 'Preparing graphics', maybePercent: 40 };
    const waiting = { label: 'Opening cartridge', maybePercent: null };
    // A static signal screen and a measured gauge need no animation-driven repaint.
    expect(crtScreenKey(0, 'Insert your cartridge', false, false)).toBe(
        crtScreenKey(9, 'Insert your cartridge', false, false),
    );
    expect(crtScreenKey(0, 'Starting', false, false)).not.toBe(
        crtScreenKey(0, 'Starting', true, false),
    );
    expect(crtScreenKey(0, 'x', true, false, measured)).toBe(
        crtScreenKey(5, 'y', true, false, measured),
    );
    expect(crtScreenKey(0, 'x', true, false, measured)).not.toBe(
        crtScreenKey(0, 'x', true, false, { ...measured, maybePercent: 41 }),
    );
    // The indeterminate gauge animates unless motion is reduced.
    expect(crtScreenKey(0, 'x', true, false, waiting)).not.toBe(
        crtScreenKey(1, 'x', true, false, waiting),
    );
    expect(crtScreenKey(0, 'x', true, true, waiting)).toBe(
        crtScreenKey(1, 'x', true, true, waiting),
    );
    expect(drawing(0, null).bars).toEqual(drawing(0.01, null).bars);
});

function stageDrawing(label: string) {
    const text: string[] = [];
    const cells: { style: string; x: number }[] = [];
    const context = {
        fillStyle: '',
        fillText: (value: string) => text.push(value),
        fillRect: (x: number, y: number) => {
            if (y === 328) cells.push({ style: String(context.fillStyle), x });
        },
        strokeRect: () => {},
    } as unknown as CanvasRenderingContext2D;
    paintCrtScreen(context, 0, '', true, false, { label, maybePercent: null });
    return { text, styles: cells.map((cell) => cell.style) };
}

test('CRT stage track shows where the current stage sits in the whole startup', () => {
    const { text, styles } = stageDrawing('Preparing graphics');
    expect(text).toContain('STAGE 5 OF 7');
    expect(text).toContain('GRAPHICS');
    const { green, gold, panel } = MISSION_THEME;
    expect(styles).toEqual([green, green, green, green, gold, panel, panel]);
    expect(stageDrawing('Ready to play').styles.every((style) => style === green)).toBe(true);
    expect(stageDrawing('Startup paused · resume below').styles).toHaveLength(0);
});
