import { createSignal } from 'solid-js';
import { render } from 'solid-js/web';
import { TouchControls } from '../src/touch-controls';
import { createTouchInput } from '../src/touch-input';
import type { Engine } from '../src/engine';
import type { Capture, Playback } from '../src/capture';

let live = true;
let movement = [0, 0],
    look = [0, 0];
const held = new Set<number>();
const [state, setState] = createSignal<Playback>({ kind: 'playing' });
const game = {
    _portWebControlsLive: () => Number(live),
    _portWebTouchButton: (id: number, down: number) => (down ? held.add(id) : held.delete(id)),
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
        snapshot: () => ({ movement, look, held: [...held], state: state().kind }),
        menu: (value: boolean) => {
            live = !value;
        },
        resume: () => setState({ kind: 'playing' }),
        enabled: input.setEnabled,
    },
});
render(
    () => (
        <TouchControls
            engine={engine}
            capture={capture}
            input={input}
            visible={() => true}
            options={() => capture.pause('manual')}
        />
    ),
    document.body,
);
