import type { StartupProgress } from '../startup-progress';

export function paintCrtScreen(
    ctx: CanvasRenderingContext2D,
    seconds: number,
    label: string,
    powered: boolean,
    reduced: boolean,
    progress?: StartupProgress,
) {
    ctx.fillStyle = '#0c1b70';
    ctx.fillRect(0, 0, 640, 480);
    ctx.fillStyle = '#d0d9ff';
    ctx.font = '23px monospace';
    ctx.textAlign = 'left';
    ctx.fillText('VIDEO 3', 42, 60);
    ctx.textAlign = 'center';
    ctx.font = '26px monospace';
    ctx.fillText(progress ? 'GOLDENEYE 007' : powered ? 'SIGNAL FOUND' : 'NO SIGNAL', 320, 190);
    ctx.font = '20px monospace';
    ctx.fillStyle = '#c3d3ff';
    ctx.fillText((progress?.label ?? label).toUpperCase(), 320, 245, 532);
    if (progress) {
        const percent = progress.maybePercent;
        ctx.strokeStyle = '#93b9ee';
        ctx.lineWidth = 2;
        ctx.strokeRect(62, 290, 516, 24);
        ctx.fillStyle = '#a9dbf2';
        const width = percent === null ? 112 : 508 * Math.max(0, Math.min(1, percent / 100));
        const x =
            percent === null ? 66 + (reduced ? 0.5 : (Math.sin(seconds * 1.8) + 1) / 2) * 396 : 66;
        ctx.fillRect(x, 294, width, 16);
        ctx.font = '18px monospace';
        ctx.fillText(percent === null ? 'PLEASE WAIT' : `${Math.round(percent)}%`, 320, 350);
    }
    ctx.font = '14px monospace';
    ctx.fillStyle = '#aebdea';
    ctx.fillText('F2  CONTROLS', 320, 420);
}
