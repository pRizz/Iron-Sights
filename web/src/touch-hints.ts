import { createEffect, createMemo, createSignal, onCleanup, type Accessor } from 'solid-js';

type TouchHintContext = 'inactive' | 'menu' | 'files';

export function createTouchHints(context: Accessor<TouchHintContext>): Accessor<boolean> {
    const [visible, setVisible] = createSignal(false);
    const currentContext = createMemo(context);
    createEffect(() => {
        const current = currentContext();
        setVisible(current !== 'inactive');
        if (current === 'inactive') return;
        const timer = setTimeout(() => setVisible(false), 5000);
        onCleanup(() => clearTimeout(timer));
    });
    return visible;
}
