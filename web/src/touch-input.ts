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
        clear() {
            for (const action of new Set(fingers.values())) send(action, false);
            fingers.clear();
        },
    };
}

export function createTouchInput(engine: Engine, initial: boolean) {
    const [enabled, setEnabled] = createSignal(initial);
    const state = createTouchState((action, down) =>
        engine.maybeGame()?._portWebTouchButton(TOUCH_ACTIONS.indexOf(action), Number(down)),
    );
    const reset = () => {
        state.clear();
        engine.maybeGame()?._portWebTouchReset();
    };
    return {
        enabled,
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
        move(x: number, y: number) {
            engine.maybeGame()?._portWebTouchMove(x, y);
        },
        look(x: number, y: number) {
            engine.maybeGame()?._portWebTouchLook(x, y);
        },
    };
}
export type TouchInput = ReturnType<typeof createTouchInput>;
