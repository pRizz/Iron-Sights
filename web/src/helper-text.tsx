import { createEffect, createSignal, onCleanup, Show, type JSX } from 'solid-js';
import { Dynamic } from 'solid-js/web';
import './helper-text.css';

export function HelperText(props: {
    when: boolean;
    as?: 'span' | 'p';
    class?: string;
    role?: 'status';
    children: JSX.Element;
}) {
    const [mounted, setMounted] = createSignal(props.when);
    createEffect(() => {
        if (props.when) {
            setMounted(true);
            return;
        }
        const timer = setTimeout(() => setMounted(false), 350);
        onCleanup(() => clearTimeout(timer));
    });
    return (
        <Show when={mounted()}>
            <Dynamic
                component={props.as ?? 'span'}
                class={`helper-text ${props.class ?? ''}`}
                classList={{ 'helper-text-fading': !props.when }}
                aria-hidden={!props.when}
                role={props.role}
            >
                {props.children}
            </Dynamic>
        </Show>
    );
}
