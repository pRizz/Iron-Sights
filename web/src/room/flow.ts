export type RoomPhase =
    | 'empty'
    | 'inserted'
    | 'inserting'
    | 'waiting'
    | 'powering'
    | 'flying'
    | 'screen'
    | 'complete'
    | 'retreating';
export type RoomFlow = { phase: RoomPhase; elapsed: number };
export type RoomAction = 'restore' | 'select' | 'activate' | 'reset';

export function roomAction(state: RoomFlow, action: RoomAction): RoomFlow {
    if (action === 'reset') return { phase: 'empty', elapsed: 0 };
    if (action === 'restore' && state.phase === 'empty') return { phase: 'inserted', elapsed: 0 };
    if (action === 'select' && ['empty', 'inserted'].includes(state.phase))
        return { phase: 'inserting', elapsed: 0 };
    if (action === 'activate' && state.phase === 'inserted')
        return { phase: 'waiting', elapsed: 0 };
    return state;
}

export function roomDuration(phase: RoomPhase, reduced: boolean): number {
    if (reduced) return 0.16;
    if (phase === 'inserting') return 0.9;
    if (phase === 'powering') return 0.3;
    if (phase === 'flying') return 1.3;
    if (phase === 'retreating') return 1.5;
    return Infinity;
}

export function advanceRoom(
    state: RoomFlow,
    seconds: number,
    ready: boolean,
    reduced: boolean,
): RoomFlow {
    if (state.phase === 'waiting') return { phase: 'powering', elapsed: 0 };
    if (state.phase === 'screen') return ready ? { phase: 'complete', elapsed: 0 } : state;
    if (!['inserting', 'powering', 'flying', 'retreating'].includes(state.phase)) return state;
    const elapsed = state.elapsed + Math.max(0, Math.min(seconds, 0.1));
    if (elapsed < roomDuration(state.phase, reduced)) return { ...state, elapsed };
    const next = {
        inserting: 'waiting',
        powering: 'flying',
        flying: ready ? 'complete' : 'screen',
        retreating: 'inserted',
    } as const;
    return { phase: next[state.phase as keyof typeof next], elapsed: 0 };
}

export function easeOut(progress: number): number {
    return 1 - (1 - Math.max(0, Math.min(1, progress))) ** 3;
}
