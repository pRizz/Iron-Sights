import { createSignal } from 'solid-js';
import { render } from 'solid-js/web';
import { TouchControls } from '../src/touch-controls';
import { createTouchInput } from '../src/touch-input';
import { TouchLayoutSetting } from '../src/touch-layout-setting';
import { FileSaveActions } from '../src/file-save-actions';
import type { Engine } from '../src/engine';
import type { Capture, Playback } from '../src/capture';

let live = true;
let movement = [0, 0],
    look = [0, 0];
const held = new Set<number>();
let accepts = 0;
let cursor = [0, 0];
let exported = 0,
    imported = '';
let picker!: HTMLInputElement;
const [fileMenu, setFileMenu] = createSignal(false);
const canvas = document.createElement('canvas');
canvas.addEventListener('mousemove', (event) => {
    cursor = [event.clientX, event.clientY];
});
canvas.addEventListener('mousedown', () => {
    throw new Error('Menu synthesized a mouse press');
});
document.body.append(canvas);
const [state, setState] = createSignal<Playback>({ kind: 'playing' });
const game = {
    _portWebFileMenuMetric: (metric: number) =>
        metric === 0
            ? Number(fileMenu())
            : [0.7, 0.2, 0.25, 0.12, 0.7, 0.8, 0.25, 0.12][metric - 1],
    _portWebControlsLive: () => Number(live),
    _portWebTouchButton: (id: number, down: number) => {
        if (id === 5 && down) accepts++;
        down ? held.add(id) : held.delete(id);
    },
    _portWebTouchMove: (x: number, y: number) => {
        movement = [x, y];
    },
    _portWebTouchLook: (x: number, y: number) => {
        look = [look[0] + x, look[1] + y];
    },
    _portWebTouchReset: () => {
        movement = [0, 0];
        look = [0, 0];
        held.clear();
    },
};
const engine = { maybeGame: () => game } as unknown as Engine;
const capture = {
    state,
    pause: () => setState({ kind: 'paused', reason: 'manual' }),
} as unknown as Capture;
const input = createTouchInput(engine, true);
Object.assign(globalThis, {
    touchFixture: {
        snapshot: () => ({
            movement,
            look,
            held: [...held],
            state: state().kind,
            accepts,
            cursor,
            exported,
            imported,
        }),
        fileMenu: setFileMenu,
        menu: (value: boolean) => {
            live = !value;
        },
        resume: () => setState({ kind: 'playing' }),
        enabled: input.setEnabled,
        layout: input.setLayout,
    },
});
render(
    () => (
        <>
            <div style="position:fixed;top:70px;left:12px;z-index:12;color:white">
                <TouchLayoutSetting input={input} beforeChange={() => {}} />
            </div>
            <TouchControls
                engine={engine}
                capture={capture}
                input={input}
                visible={() => true}
                options={() => capture.pause('manual')}
                canvas={() => canvas}
            />
            <input
                ref={picker}
                type="file"
                hidden
                accept=".json"
                onChange={(event) => {
                    imported = event.currentTarget.files?.[0]?.name ?? '';
                }}
            />
            <FileSaveActions
                engine={engine}
                canvas={() => canvas}
                visible={() => true}
                transfer={(exporting) => {
                    if (exporting) exported++;
                    else picker.click();
                }}
            />
        </>
    ),
    document.body,
);
