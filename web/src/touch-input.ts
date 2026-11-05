import { createSignal } from 'solid-js';
import type { Engine } from './engine';

// Matches the browser-only enum in port/web/touch.c; saved controller IDs are separate.
export const TOUCH_ACTIONS = [
    'fire',
    'aim',
    'use',
    'reload',
    'watch',
    'accept',
    'back',
    'crouch',
    'previousWeapon',
    'nextWeapon',
    'previousGadget',
    'nextGadget',
    'detonate',
    'zoomIn',
    'zoomOut',
    'up',
    'down',
    'left',
    'right',
    'sprint',
] as const;
export type TouchAction = (typeof TOUCH_ACTIONS)[number];
const KEY = 'midas-ads-touch-v1';
const LAYOUT_KEY = 'midas-ads-touch-layout-v1';
export type TouchLayout = 'n64' | 'modern';

export function touchLayoutPreference(value: unknown): TouchLayout {
    return value === 'modern' ? 'modern' : 'n64';
}

export function initialTouchLayout(): TouchLayout {
    try {
        return touchLayoutPreference(localStorage.getItem(LAYOUT_KEY));
    } catch {
        return 'n64';
    }
}

export function defaultTouch(points: number, coarse: boolean, pointerLock: boolean): boolean {
    return points > 0 && (coarse || !pointerLock);
}

export function initialTouch(): boolean {
    try {
        const preference = localStorage.getItem(KEY);
        if (preference === 'on' || preference === 'off') return preference === 'on';
    } catch {
        /* Session controls still work when preferences cannot be saved. */
    }
    return defaultTouch(
        navigator.maxTouchPoints,
        matchMedia('(pointer: coarse)').matches,
        'requestPointerLock' in HTMLCanvasElement.prototype,
    );
}

export function stickVector(x: number, y: number, radius: number) {
    const length = Math.hypot(x, y) / radius;
    if (!Number.isFinite(length) || length <= 0.15) return { x: 0, y: 0 };
    const strength = Math.min(1, (length - 0.15) / 0.85);
    const scale = strength / Math.hypot(x, y);
    return { x: x * scale, y: -y * scale };
}

// Distances from the stick's centre, as fractions of its width (the ring's edge
// is 0.5). Dragging past ENGAGE sprints; the finger must come back inside RELEASE
// to stop, so a thumb resting near the boundary cannot flicker in and out.
export const STICK_SPRINT_ENGAGE = 0.7;
export const STICK_SPRINT_RELEASE = 0.6;

export function stickOverdrag(x: number, y: number, width: number, sprinting: boolean): boolean {
    const distance = Math.hypot(x, y) / width;
    if (!Number.isFinite(distance)) return false;
    return distance > (sprinting ? STICK_SPRINT_RELEASE : STICK_SPRINT_ENGAGE);
}

// A tap may wobble, but a drag (even one that returns to its origin) never selects.
export function menuTapMoved(x: number, y: number, startX: number, startY: number): boolean {
    return Math.hypot(x - startX, y - startY) > 10;
}

// An open slider edit turns sideways travel into whole value steps. The remainder
// carries over, so a slow drag still steps once per stride.
export function createValueScrub(stride = 18) {
    let maybeOrigin: number | undefined;
    return {
        begin(x: number) {
            maybeOrigin = x;
        },
        move(x: number): number {
            if (maybeOrigin === undefined) return 0;
            const steps = Math.trunc((x - maybeOrigin) / stride);
            maybeOrigin += steps * stride;
            return steps;
        },
    };
}

// Each finger owns one control. Two fingers holding the same button release independently.
export function createTouchState(send: (action: TouchAction, down: boolean) => void) {
    const fingers = new Map<number, TouchAction>();
    return {
        press(id: number, action: TouchAction) {
            if (fingers.has(id)) return;
            if (![...fingers.values()].includes(action)) send(action, true);
            fingers.set(id, action);
        },
        release(id: number) {
            const action = fingers.get(id);
            fingers.delete(id);
            if (action && ![...fingers.values()].includes(action)) send(action, false);
        },
        releaseAction(action: TouchAction) {
            let released = false;
            for (const [id, held] of fingers) {
                if (held !== action) continue;
                fingers.delete(id);
                released = true;
            }
            if (released) send(action, false);
        },
        clear() {
            for (const action of new Set(fingers.values())) send(action, false);
            fingers.clear();
        },
    };
}

export function createTouchInput(
    engine: Engine,
    initial: boolean,
    initialLayout = initialTouchLayout(),
) {
    const [enabled, setEnabled] = createSignal(initial);
    const [layout, setLayout] = createSignal<TouchLayout>(initialLayout);
    const state = createTouchState((action, down) =>
        engine.maybeGame()?._portWebTouchButton(TOUCH_ACTIONS.indexOf(action), Number(down)),
    );
    const reset = () => {
        state.clear();
        engine.maybeGame()?._portWebTouchReset();
    };
    return {
        enabled,
        layout,
        reset,
        state,
        gesture() {
            const maybeAudio = engine.maybeGame()?.SDL2?.audioContext;
            if (maybeAudio && maybeAudio.state !== 'running' && maybeAudio.state !== 'closed')
                void maybeAudio.resume().catch(() => {});
        },
        setEnabled(value: boolean) {
            reset();
            setEnabled(value);
            try {
                localStorage.setItem(KEY, value ? 'on' : 'off');
            } catch {
                /* Optional preference. */
            }
        },
        setLayout(value: TouchLayout) {
            reset();
            setLayout(value);
            try {
                localStorage.setItem(LAYOUT_KEY, value);
            } catch {
                /* The selected layout still works for this session. */
            }
        },
        move(x: number, y: number) {
            engine.maybeGame()?._portWebTouchMove(x, y);
        },
        look(x: number, y: number) {
            engine.maybeGame()?._portWebTouchLook(x, y);
        },
        stickSprint(on: boolean) {
            engine.maybeGame()?._portWebTouchStickSprint?.(Number(on));
        },
        step(count: number) {
            engine.maybeGame()?._portWebTouchStep?.(count);
        },
        unbind() {
            engine.maybeGame()?._portWebTouchUnbind?.();
        },
    };
}
export type TouchInput = ReturnType<typeof createTouchInput>;
