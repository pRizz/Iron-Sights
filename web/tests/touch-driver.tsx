import { createSignal, Show } from 'solid-js';
import { render } from 'solid-js/web';
import { TouchControls } from '../src/touch-controls';
import { createTouchInput } from '../src/touch-input';
import { TouchLayoutSetting } from '../src/touch-layout-setting';
import { FileSaveActions } from '../src/file-save-actions';
import { HelperText } from '../src/helper-text';
import type { Engine } from '../src/engine';
import type { Capture, Playback } from '../src/capture';
import '../src/room.css';

let live = true;
let context = 2;
let gadgets = true;
let movement = [0, 0],
    look = [0, 0];
let stickSprint = false;
const held = new Set<number>();
const presses: number[] = [];
let accepts = 0;
let steps = 0;
let unbinds = 0;
// The native name keyboard as the phone-keyboard field sees it.
let nameText = '';
let nameFinish: number | undefined;
let windowKeys = 0;
// Corners the overlay says its controls cover, and the corner ammo boxes the
// runtime reports there (canvas fractions; none until the audit sets one).
const corners = [false, false];
const ammo: (number[] | undefined)[] = [undefined, undefined];
window.addEventListener('keydown', () => windowKeys++);
let cursor = [0, 0];
let exported = 0,
    imported = '';
let picker!: HTMLInputElement;
const [fileMenu, setFileMenu] = createSignal(false);
const [message, setMessage] = createSignal(false);
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
    _portWebTouchContext: () => (fileMenu() ? 0 : context),
    _portWebTouchHasGadgets: () => Number(gadgets && context === 2 && !fileMenu()),
    _portWebTouchHudCorner: (side: number, covered: number) => {
        corners[side] = !!covered;
    },
    _portWebTouchAmmoBox: (side: number, edge: number) =>
        (corners[side] && ammo[side]?.[edge]) ?? -1,
    _portWebTouchButton: (id: number, down: number) => {
        if (down) presses.push(id);
        if (id === 5 && down) accepts++;
        down ? held.add(id) : held.delete(id);
    },
    _portWebTouchStep: (count: number) => {
        steps += count;
    },
    _portWebTouchUnbind: () => {
        unbinds++;
    },
    _portWebTextChar: (field: number, index: number) =>
        context === 10 ? (field === 0 ? 'SAVE SET AS' : nameText).charCodeAt(index) || 0 : 0,
    _portWebTextClear: () => {
        nameText = '';
    },
    _portWebTextAppend: (code: number) => {
        if (nameText.length < 21) nameText += String.fromCharCode(code);
    },
    _portWebTextFinish: (ok: number) => {
        nameFinish = ok;
    },
    _portWebTouchMove: (x: number, y: number) => {
        movement = [x, y];
    },
    _portWebTouchStickSprint: (on: number) => {
        stickSprint = !!on;
    },
    _portWebTouchLook: (x: number, y: number) => {
        look = [look[0] + x, look[1] + y];
    },
    _portWebTouchReset: () => {
        movement = [0, 0];
        stickSprint = false;
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
            stickSprint,
            look,
            held: [...held],
            presses: [...presses],
            state: state().kind,
            accepts,
            steps,
            unbinds,
            nameText,
            nameFinish,
            windowKeys,
            corners: [...corners],
            cursor,
            exported,
            imported,
        }),
        fileMenu: setFileMenu,
        ammo: (side: number, box?: number[]) => {
            ammo[side] = box;
        },
        gadgets: (value: boolean) => {
            gadgets = value;
        },
        menu: (value: boolean) => {
            live = !value;
            context = value ? 3 : 2;
        },
        context: (value: number) => {
            context = value;
            live = value === 2;
        },
        resume: () => setState({ kind: 'playing' }),
        pause: capture.pause,
        message: setMessage,
        enabled: input.setEnabled,
        layout: input.setLayout,
    },
});
render(
    () => (
        <>
            <HelperText when={message()} as="p" class="experience-hint touch-message">
                Touch hint passthrough check
            </HelperText>
            <div
                data-fixture-settings
                style="position:fixed;top:70px;left:12px;z-index:12;color:white"
            >
                <TouchLayoutSetting input={input} beforeChange={() => {}} />
            </div>
            <TouchControls
                engine={engine}
                capture={capture}
                input={input}
                visible={() => true}
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
