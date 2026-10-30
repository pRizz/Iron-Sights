type Point = { x: number; y: number };
type Gesture<T> = { target: T; start: Point; last: Point; dragged: boolean };

// A contact chooses one intent: a short tap or looking, even if it returns home.
export function createTouchButtonGestures<T>() {
    const gestures = new Map<number, Gesture<T>>();
    const move = (id: number, x: number, y: number): Point => {
        const maybeGesture = gestures.get(id);
        if (!maybeGesture) return { x: 0, y: 0 };
        const gesture = maybeGesture;
        gesture.dragged ||= Math.hypot(x - gesture.start.x, y - gesture.start.y) > 10;
        if (!gesture.dragged) return { x: 0, y: 0 };
        const delta = { x: x - gesture.last.x, y: y - gesture.last.y };
        gesture.last = { x, y };
        return delta;
    };
    return {
        begin(id: number, target: T, x: number, y: number) {
            gestures.set(id, { target, start: { x, y }, last: { x, y }, dragged: false });
        },
        move,
        cancelWhere(matches: (target: T) => boolean) {
            for (const [id, gesture] of gestures) {
                if (matches(gesture.target)) gestures.delete(id);
            }
        },
        maybeTakeTap(id: number, maybeRelease?: Point): T | undefined {
            if (maybeRelease) move(id, maybeRelease.x, maybeRelease.y);
            const maybeGesture = gestures.get(id);
            gestures.delete(id);
            return maybeRelease && !maybeGesture?.dragged ? maybeGesture?.target : undefined;
        },
        clear: () => gestures.clear(),
    };
}
