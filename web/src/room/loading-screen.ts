import {
    stageNumber,
    stageTrack,
    type StageTrack,
    type StartupProgress,
} from '../startup-progress';
import { MISSION_THEME as theme } from '../mission-theme';

const gaugeStart = (seconds: number, reduced: boolean) =>
    reduced ? 8 : Math.floor(((Math.sin(seconds * 1.8) + 1) / 2) * 16);

// Everything the painted screen depends on: an unchanged key repaints identical pixels.
export function crtScreenKey(
    seconds: number,
    label: string,
    powered: boolean,
    reduced: boolean,
    progress?: StartupProgress,
): string {
    if (!progress) return ['signal', powered, label].join('|');
    const { label: stage, maybePercent } = progress;
    const gauge = maybePercent === null ? gaugeStart(seconds, reduced) : '';
    return ['mission', stage, maybePercent, gauge].join('|');
}

// Whole-startup track under the stage gauge: finished stages green, the current gold.
function paintStages(ctx: CanvasRenderingContext2D, track: StageTrack) {
    const gap = 8;
    const width = (552 - gap * (track.total - 1)) / track.total;
    for (let index = 0; index < track.total; index++) {
        ctx.fillStyle =
            index < track.current
                ? theme.green
                : index === track.current
                  ? theme.gold
                  : theme.panel;
        ctx.fillRect(44 + index * (width + gap), 328, width, 8);
        if (index > track.current) {
            ctx.strokeStyle = theme.edge;
            ctx.lineWidth = 1;
            ctx.strokeRect(44.5 + index * (width + gap), 328.5, width - 1, 7);
        }
    }
    ctx.fillStyle = theme.muted;
    ctx.font = `14px ${theme.font}`;
    ctx.textAlign = 'left';
    ctx.fillText(`STAGE ${stageNumber(track)} OF ${track.total}`, 44, 360);
    ctx.textAlign = 'right';
    ctx.fillText(track.name.toUpperCase(), 596, 360);
}

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
    const active = gaugeStart(seconds, reduced);
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
    const maybeTrack = stageTrack(progress.label);
    if (maybeTrack) paintStages(ctx, maybeTrack);
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
