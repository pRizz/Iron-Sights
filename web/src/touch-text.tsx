import { createEffect, createSignal, onCleanup, Show, type Accessor } from 'solid-js';
import type { Engine } from './engine';
import {
    NAME_MAX_LENGTH,
    finishNativeName,
    readNativeText,
    writeNativeName,
} from './touch-text-bridge';

// The phone's own keyboard for the native name keyboard (Save Layout / Set /
// Settings As). It opens only from the Phone keyboard button, a tap the browser
// accepts for focusing a text field, and the game's letter grid stays usable.
// The field docks to the top of the visible viewport, above the keyboard, so the
// keyboard may cover the game but never the name being typed. It sits outside
// the overlay's gesture-suppressing surface so its taps behave normally.
export function TouchText(props: { engine: Engine; open: Accessor<boolean> }) {
    const [typing, setTyping] = createSignal(false);
    const [title, setTitle] = createSignal('');
    const [dock, setDock] = createSignal({ top: 0, left: 0, width: 0 });
    let field!: HTMLInputElement;
    const game = () => props.engine.maybeGame();
    const stop = () => {
        setTyping(false);
        if (document.activeElement === field) field.blur();
    };
    const place = () => {
        const viewport = window.visualViewport;
        setDock({
            top: viewport?.offsetTop ?? 0,
            left: viewport?.offsetLeft ?? 0,
            width: viewport?.width ?? innerWidth,
        });
    };
    createEffect(() => {
        if (!props.open()) {
            stop();
            return;
        }
        // The native keyboard may retitle itself (NAME IN USE) or change the name
        // from the grid; the field follows it except while the player is typing.
        let frame = 0;
        const follow = () => {
            const maybeGame = game();
            if (maybeGame) {
                setTitle(readNativeText(maybeGame, 0));
                if (document.activeElement !== field) field.value = readNativeText(maybeGame, 1);
            }
            frame = requestAnimationFrame(follow);
        };
        follow();
        onCleanup(() => cancelAnimationFrame(frame));
    });
    createEffect(() => {
        if (!typing()) return;
        place();
        const viewport = window.visualViewport;
        viewport?.addEventListener('resize', place);
        viewport?.addEventListener('scroll', place);
        onCleanup(() => {
            viewport?.removeEventListener('resize', place);
            viewport?.removeEventListener('scroll', place);
        });
    });
    const start = () => {
        const maybeGame = game();
        if (!maybeGame) return;
        field.value = readNativeText(maybeGame, 1);
        setTitle(readNativeText(maybeGame, 0));
        place();
        setTyping(true);
        // Focus inside the tap itself; iOS shows its keyboard for nothing else.
        field.focus();
        field.setSelectionRange(field.value.length, field.value.length);
    };
    const finish = (ok: boolean) => {
        const maybeGame = game();
        if (maybeGame) finishNativeName(maybeGame, ok);
        stop();
    };
    // Typing belongs to the field: never let it reach the game's key bindings.
    const isolate = (event: KeyboardEvent) => event.stopPropagation();
    // Buttons keep focus in the field so the keyboard stays up until they act.
    const keepFocus = (event: PointerEvent) => event.preventDefault();
    return (
        <Show when={props.open()}>
            <Show when={!typing()}>
                <button type="button" class="touch-text-launch" onClick={start}>
                    Phone keyboard
                </button>
            </Show>
            <form
                class="touch-text"
                hidden={!typing()}
                aria-label={title() || 'Name'}
                style={{
                    top: `${dock().top}px`,
                    left: `${dock().left}px`,
                    width: `${dock().width}px`,
                }}
                onSubmit={(event) => {
                    event.preventDefault();
                    finish(true);
                }}
            >
                <label class="touch-text-title" for="touch-text-field">
                    {title()}
                </label>
                <input
                    id="touch-text-field"
                    ref={field}
                    type="text"
                    maxLength={NAME_MAX_LENGTH}
                    autocomplete="off"
                    autocapitalize="characters"
                    spellcheck={false}
                    enterkeyhint="done"
                    onKeyDown={isolate}
                    onKeyUp={isolate}
                    onKeyPress={isolate}
                    onInput={() => {
                        const maybeGame = game();
                        if (!maybeGame) return;
                        const accepted = writeNativeName(maybeGame, field.value);
                        if (accepted !== field.value) field.value = accepted;
                    }}
                    onBlur={() => setTyping(false)}
                />
                <button type="submit" onPointerDown={keepFocus}>
                    Done
                </button>
                <button type="button" onPointerDown={keepFocus} onClick={() => finish(false)}>
                    Cancel
                </button>
            </form>
        </Show>
    );
}
