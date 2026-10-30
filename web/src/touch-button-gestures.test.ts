import { expect, test } from 'bun:test';
import { createTouchButtonGestures } from './touch-button-gestures';

test('a stationary button tap activates once on release', () => {
    // Arrange / Act
    const gestures = createTouchButtonGestures<string>();
    gestures.begin(1, 'watch', 100, 100);
    expect(gestures.move(1, 104, 103)).toEqual({ x: 0, y: 0 });
    // Assert
    expect(gestures.maybeTakeTap(1, { x: 104, y: 103 })).toBe('watch');
    expect(gestures.maybeTakeTap(1, { x: 104, y: 103 })).toBeUndefined();
});

test('a swipe looks without activating, including after returning to its origin', () => {
    // Arrange
    const gestures = createTouchButtonGestures<string>();
    gestures.begin(1, 'aim', 100, 100);
    // Act / Assert
    expect(gestures.move(1, 120, 110)).toEqual({ x: 20, y: 10 });
    expect(gestures.move(1, 124, 116)).toEqual({ x: 4, y: 6 });
    expect(gestures.move(1, 100, 100)).toEqual({ x: -24, y: -16 });
    expect(gestures.maybeTakeTap(1, { x: 100, y: 100 })).toBeUndefined();
});

test('a release outside the slop cannot become a tap when move events were missed', () => {
    // Arrange / Act / Assert
    const gestures = createTouchButtonGestures<string>();
    gestures.begin(1, 'gadget', 100, 100);
    expect(gestures.maybeTakeTap(1, { x: 130, y: 100 })).toBeUndefined();
});

test('cancel, independent contacts and lifecycle clearing never activate stale buttons', () => {
    // Arrange
    const gestures = createTouchButtonGestures<string>();
    gestures.begin(1, 'watch', 100, 100);
    gestures.begin(2, 'gadget', 200, 100);
    // Act / Assert
    expect(gestures.maybeTakeTap(1)).toBeUndefined();
    expect(gestures.maybeTakeTap(2, { x: 200, y: 100 })).toBe('gadget');
    gestures.begin(3, 'watch', 100, 100);
    gestures.clear();
    expect(gestures.maybeTakeTap(3, { x: 100, y: 100 })).toBeUndefined();
    expect(gestures.move(3, 200, 100)).toEqual({ x: 0, y: 0 });
});

test('removing a button cancels its pending tap without cancelling another contact', () => {
    // Arrange / Act
    const gestures = createTouchButtonGestures<string>();
    gestures.begin(1, 'gadget', 100, 100);
    gestures.begin(2, 'watch', 200, 100);
    gestures.cancelWhere((target) => target === 'gadget');
    // Assert
    expect(gestures.maybeTakeTap(1, { x: 100, y: 100 })).toBeUndefined();
    expect(gestures.maybeTakeTap(2, { x: 200, y: 100 })).toBe('watch');
});
