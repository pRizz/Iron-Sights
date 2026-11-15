import { expect, test } from 'bun:test';
import { createDoubleTap, DOUBLE_TAP, isSecondTap, isTap } from './touch-double-tap';

const at = (x: number, y: number, time: number) => ({ x, y, time });

test('a short, still press is a tap; a long or wandering one is not', () => {
    // Arrange
    const start = at(100, 100, 0);
    // Act / Assert
    expect(isTap(start, at(104, 100, DOUBLE_TAP.tapMs), 4)).toBe(true);
    expect(isTap(start, at(100, 100, DOUBLE_TAP.tapMs + 1), 0)).toBe(false);
    expect(isTap(start, at(100, 100, 100), DOUBLE_TAP.tapSlop + 1)).toBe(false);
});

test('the second touch must land soon and near the first tap', () => {
    // Arrange
    const tap = at(200, 150, 1000);
    // Act / Assert
    expect(isSecondTap(tap, at(230, 150, 1000 + DOUBLE_TAP.gapMs))).toBe(true);
    expect(isSecondTap(tap, at(200, 150, 1000 + DOUBLE_TAP.gapMs + 1))).toBe(false);
    expect(isSecondTap(tap, at(200 + DOUBLE_TAP.reach + 1, 150, 1100))).toBe(false);
    expect(isSecondTap(tap, at(200, 150, 999))).toBe(false);
});

test('tap then touch fires on the second touch, never on the first', () => {
    // Arrange
    const pair = createDoubleTap();
    // Act / Assert
    expect(pair.begin(at(100, 100, 0))).toBe(false);
    pair.end(at(100, 100, 0), at(101, 100, 90), 1, false);
    expect(pair.begin(at(110, 104, 250))).toBe(true);
});

test('a look drag never arms a pair, so swiping and re-touching does not fire', () => {
    // Arrange
    const pair = createDoubleTap();
    pair.begin(at(100, 100, 0));
    // Act
    pair.end(at(100, 100, 0), at(160, 100, 80), 60, false);
    // Assert
    expect(pair.begin(at(150, 100, 150))).toBe(false);
});

test('a pair is used once: the firing touch does not arm a third tap', () => {
    // Arrange
    const pair = createDoubleTap();
    pair.begin(at(100, 100, 0));
    pair.end(at(100, 100, 0), at(100, 100, 60), 0, false);
    // Act
    const second = pair.begin(at(100, 100, 120));
    pair.end(at(100, 100, 120), at(100, 100, 180), 0, second);
    // Assert
    expect(second).toBe(true);
    expect(pair.begin(at(100, 100, 240))).toBe(false);
});

test('a slow second touch only starts over, and clear forgets the tap', () => {
    // Arrange
    const pair = createDoubleTap();
    pair.begin(at(100, 100, 0));
    pair.end(at(100, 100, 0), at(100, 100, 60), 0, false);
    // Act / Assert
    expect(pair.begin(at(100, 100, 60 + DOUBLE_TAP.gapMs + 1))).toBe(false);
    pair.end(at(100, 100, 500), at(100, 100, 560), 0, false);
    pair.clear();
    expect(pair.begin(at(100, 100, 600))).toBe(false);
});
