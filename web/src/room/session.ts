export type EntryMode = 'restoring' | 'room' | 'cached' | 'game' | 'returning';

export const restoredEntry = (remembered: boolean): EntryMode => (remembered ? 'cached' : 'room');

export function shouldAutoLaunch(
    entry: EntryMode,
    ready: boolean,
    controls: boolean,
    started: boolean,
) {
    return entry === 'cached' && ready && !controls && !started;
}
