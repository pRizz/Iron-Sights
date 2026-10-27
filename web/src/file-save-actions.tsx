import { createSignal, onCleanup, onMount, Show, type Accessor } from 'solid-js';
import type { Engine } from './engine';
import './file-save-actions.css';

type Props = {
    engine: Engine;
    canvas: () => HTMLCanvasElement;
    visible: Accessor<boolean>;
    transfer: (exporting: boolean) => void;
};

// Transparent, accessible browser targets follow the native menu's measured
// labels. A direct click preserves iPhone's file-picker user gesture.
export function FileSaveActions(props: Props) {
    const [bounds, setBounds] = createSignal<number[]>([]);
    onMount(() => {
        let frame = 0;
        const update = () => {
            const maybeGame = props.engine.maybeGame();
            let next: number[] = [];
            if (props.visible() && maybeGame?._portWebFileMenuMetric(0)) {
                const box = props.canvas().getBoundingClientRect();
                const width = Math.min(box.width, (box.height * 4) / 3);
                const left = box.left + (box.width - width) / 2;
                next = Array.from({ length: 8 }, (_, i) => {
                    const value = maybeGame._portWebFileMenuMetric(i + 1);
                    return i % 4 === 0
                        ? left + value * width
                        : i % 4 === 1
                          ? box.top + value * box.height
                          : i % 4 === 2
                            ? value * width
                            : value * box.height;
                });
            }
            if (next.join() !== bounds().join()) setBounds(next);
            frame = requestAnimationFrame(update);
        };
        frame = requestAnimationFrame(update);
        onCleanup(() => cancelAnimationFrame(frame));
    });
    return (
        <Show when={bounds().length === 8}>
            <section class="file-save-actions" aria-label="Save transfer">
                {['Import Save', 'Export Save'].map((label, index) => {
                    let touchHandled = false;
                    return (
                        <button
                            type="button"
                            aria-label={label}
                            style={{
                                left: `${bounds()[index * 4]}px`,
                                top: `${bounds()[index * 4 + 1]}px`,
                                width: `${bounds()[index * 4 + 2]}px`,
                                height: `${bounds()[index * 4 + 3]}px`,
                            }}
                            onPointerDown={(event) => {
                                touchHandled = event.pointerType === 'touch';
                                event.preventDefault();
                                event.stopPropagation();
                            }}
                            onTouchEnd={(event) => {
                                event.preventDefault();
                                event.stopPropagation();
                                touchHandled = true;
                                const maybeTouch = event.changedTouches[0];
                                const box = event.currentTarget.getBoundingClientRect();
                                if (
                                    maybeTouch &&
                                    maybeTouch.clientX >= box.left &&
                                    maybeTouch.clientX <= box.right &&
                                    maybeTouch.clientY >= box.top &&
                                    maybeTouch.clientY <= box.bottom
                                )
                                    props.transfer(index === 1);
                            }}
                            onClick={(event) => {
                                event.stopPropagation();
                                if (event.detail === 0 || !touchHandled)
                                    props.transfer(index === 1);
                            }}
                        >
                            <span class="visually-hidden">{label}</span>
                        </button>
                    );
                })}
            </section>
        </Show>
    );
}
