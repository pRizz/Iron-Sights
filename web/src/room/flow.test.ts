import { expect, test } from 'bun:test';
import { advanceRoom, easeOut, roomAction, type RoomFlow } from './flow';

test('a cartridge in the returned room waits for the power gesture', () => {
    const initial: RoomFlow = { phase: 'empty', elapsed: 0 };
    const restored = roomAction(initial, 'restore');
    expect(advanceRoom(restored, 100, true, false).phase).toBe('inserted');
    expect(roomAction(restored, 'activate').phase).toBe('waiting');
});

test('selection inserts before waiting for engine readiness', () => {
    const selected = roomAction({ phase: 'empty', elapsed: 0 }, 'select');
    let state = selected;
    for (let index = 0; index < 22; index++) state = advanceRoom(state, 0.1, false, false);
    expect(state.phase).toBe('waiting');
    expect(advanceRoom(state, 0.1, false, false)).toEqual(state);
    expect(advanceRoom(state, 0.1, true, false).phase).toBe('powering');
});

test('duplicate interactions cannot restart insertion or flight', () => {
    for (const phase of [
        'inserting',
        'waiting',
        'powering',
        'flying',
        'complete',
        'retreating',
    ] as const) {
        const state = { phase, elapsed: 0.1 };
        expect(roomAction(state, 'select')).toEqual(state);
        expect(roomAction(state, 'activate')).toEqual(state);
        expect(roomAction(state, 'restore')).toEqual(state);
    }
});

test('exit retreats into an inserted room without automatically powering on', () => {
    for (const reduced of [false, true]) {
        let state: RoomFlow = { phase: 'retreating', elapsed: 0 };
        for (let frame = 0; frame < 40; frame++) state = advanceRoom(state, 0.1, true, reduced);
        expect(state.phase).toBe('inserted');
        expect(advanceRoom(state, 100, true, reduced).phase).toBe('inserted');
    }
});

test('normal and reduced sequences reach handoff only after readiness', () => {
    for (const reduced of [false, true]) {
        let state = roomAction({ phase: 'empty', elapsed: 0 }, 'select');
        for (let index = 0; index < 70; index++) state = advanceRoom(state, 0.1, true, reduced);
        expect(state.phase).toBe('complete');
        expect(roomAction(state, 'reset')).toEqual({ phase: 'empty', elapsed: 0 });
    }
});

test('background delays cannot skip through the animation', () => {
    const state = advanceRoom({ phase: 'inserting', elapsed: 0 }, 600, true, false);
    expect(state).toEqual({ phase: 'inserting', elapsed: 0.1 });
    expect(advanceRoom(state, -1, true, false)).toEqual(state);
});

test('ease-out has bounded endpoints and slows towards insertion', () => {
    expect(easeOut(-1)).toBe(0);
    expect(easeOut(2)).toBe(1);
    expect(easeOut(0.5)).toBeGreaterThan(0.5);
    expect(easeOut(0.75) - easeOut(0.5)).toBeLessThan(easeOut(0.5) - easeOut(0.25));
});
