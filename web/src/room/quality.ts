export type RoomQuality = { scale: number; slowFrames: number };

// Ignore isolated hitches; lower only this room's pixel budget after sustained slowness.
export function advanceQuality(state: RoomQuality, frameMS: number): RoomQuality {
    if (state.scale <= 0.5) return state;
    const slowFrames = frameMS > 80 ? state.slowFrames + 1 : Math.max(0, state.slowFrames - 1);
    if (slowFrames < 8) return { ...state, slowFrames };
    return { scale: state.scale > 0.75 ? 0.75 : 0.5, slowFrames: 0 };
}
