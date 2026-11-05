export type StartupProgress = { label: string; maybePercent: number | null };

export function workProgress(label: string, completed: number, total: number): StartupProgress {
    const measured = Number.isFinite(completed) && Number.isFinite(total) && total > 0;
    return {
        label,
        maybePercent: measured
            ? Math.round(Math.max(0, Math.min(1, completed / total)) * 100)
            : null,
    };
}

// Every startup label the engine, app or native runtime reports, grouped into the
// stages they occur in. A launch that skips a stage (a cached download, a room with
// no remembered cartridge) simply moves past it, so the track never goes backwards.
export const STARTUP_STAGES = [
    { name: 'Saves', labels: ['Checking browser', 'Opening local saves'] },
    { name: 'Download', labels: ['Downloading game'] },
    { name: 'Runtime', labels: ['Compiling WebAssembly', 'Restoring local saves'] },
    {
        name: 'Cartridge',
        labels: ['Checking saved cartridge', 'Preparing cartridge', 'Opening cartridge'],
    },
    { name: 'Graphics', labels: ['Preparing graphics'] },
    { name: 'Start', labels: ['Starting GoldenEye'] },
] as const;

// The room reports this once the game has started and only the handoff remains.
const COMPLETE_LABEL = 'Ready to play';

export type StageTrack = { current: number; total: number; name: string };

/** Position of a label in the startup sequence; `current` equals `total` once done. */
export function stageTrack(label: string): StageTrack | undefined {
    const total = STARTUP_STAGES.length;
    if (label === COMPLETE_LABEL) return { current: total, total, name: 'Complete' };
    const current = STARTUP_STAGES.findIndex((stage) =>
        (stage.labels as readonly string[]).includes(label),
    );
    return current < 0 ? undefined : { current, total, name: STARTUP_STAGES[current].name };
}

/** The 1-based stage number shown to the player. */
export function stageNumber(track: StageTrack): number {
    return Math.min(track.current + 1, track.total);
}
