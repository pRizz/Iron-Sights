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
