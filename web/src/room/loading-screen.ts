import type { StartupProgress } from '../startup-progress';
import { MISSION_THEME as theme } from '../mission-theme';

function paintMission(
    ctx: CanvasRenderingContext2D,
    seconds: number,
    reduced: boolean,
    progress: StartupProgress,
) {
    ctx.fillStyle = theme.background;
    ctx.fillRect(0, 0, 640, 480);
    ctx.strokeStyle = theme.edge;
    ctx.lineWidth = 2;
    ctx.strokeRect(24, 24, 592, 432);
    ctx.fillStyle = theme.gold;
    ctx.textAlign = 'left';
    ctx.font = `18px ${theme.font}`;
    ctx.fillText('MISSION PREPARATION', 44, 60);
    ctx.textAlign = 'right';
    ctx.font = `28px ${theme.font}`;
    ctx.fillText('007', 596, 60);
    ctx.fillStyle = theme.edge;
    ctx.fillRect(44, 82, 552, 1);
    ctx.textAlign = 'left';
    ctx.fillStyle = theme.text;
    ctx.font = `22px ${theme.font}`;
    ctx.fillText('GOLDENEYE 007', 44, 132);
    ctx.font = `28px ${theme.font}`;
    ctx.fillText(progress.label.toUpperCase(), 44, 204, 552);
    ctx.fillStyle = theme.muted;
    ctx.font = `14px ${theme.font}`;
    ctx.fillText('PREPARATION STATUS', 44, 262);
    ctx.textAlign = 'right';
    ctx.fillStyle = theme.green;
    const percent = progress.maybePercent;
    ctx.font = `18px ${theme.font}`;
    ctx.fillText(percent === null ? 'PLEASE WAIT' : `${Math.round(percent)}%`, 596, 262);
    const active = reduced ? 8 : Math.floor(((Math.sin(seconds * 1.8) + 1) / 2) * 16);
    for (let index = 0; index < 20; index++) {
        const x = 44 + index * 27.8;
        ctx.fillStyle = theme.panel;
        ctx.fillRect(x, 286, 23.8, 24);
        const amount =
            percent === null
                ? Number(index >= active && index < active + 4)
                : Math.max(0, Math.min(1, percent / 5 - index));
        if (amount > 0) {
            ctx.fillStyle = theme.green;
            ctx.fillRect(x, 286, 23.8 * amount, 24);
        }
    }
    ctx.fillStyle = theme.edge;
    ctx.fillRect(44, 376, 552, 1);
    ctx.fillStyle = theme.muted;
    ctx.textAlign = 'left';
    ctx.font = `15px ${theme.font}`;
    ctx.fillText('IRON SIGHTS', 44, 414);
    ctx.textAlign = 'right';
    ctx.fillText('F2  CONTROLS', 596, 414);
}

export function paintCrtScreen(
    ctx: CanvasRenderingContext2D,
    seconds: number,
    label: string,
    powered: boolean,
    reduced: boolean,
    progress?: StartupProgress,
) {
    if (progress) {
        paintMission(ctx, seconds, reduced, progress);
        return;
    }
    ctx.fillStyle = '#0c1b70';
    ctx.fillRect(0, 0, 640, 480);
    ctx.fillStyle = '#d0d9ff';
    ctx.font = '23px monospace';
    ctx.textAlign = 'left';
    ctx.fillText('VIDEO 3', 42, 60);
    ctx.textAlign = 'center';
    ctx.font = '26px monospace';
    ctx.fillText(powered ? 'SIGNAL FOUND' : 'NO SIGNAL', 320, 190);
    ctx.font = '20px monospace';
    ctx.fillStyle = '#c3d3ff';
    ctx.fillText(label.toUpperCase(), 320, 245, 532);
    ctx.font = '14px monospace';
    ctx.fillStyle = '#aebdea';
    ctx.fillText('F2  CONTROLS', 320, 420);
}
