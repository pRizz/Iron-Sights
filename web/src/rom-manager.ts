import { createSignal } from 'solid-js';
import { validateRom, type ValidatedRom } from './rom';
import { recalledRom, rememberRom, forgetRom } from './rom-storage';

export type RomStatus =
    | { kind: 'empty' | 'checking' }
    | { kind: 'ready'; rom: ValidatedRom }
    | { kind: 'error'; message: string };
const PREFERENCE = 'project-midas-ads:remember-rom';

export function createRomManager() {
    const [status, setStatus] = createSignal<RomStatus>({ kind: 'empty' });
    const [remember, setRemember] = createSignal(true);
    const [remembered, setRemembered] = createSignal(false);
    const [canForget, setCanForget] = createSignal(false);
    const [notice, setNotice] = createSignal('');
    let selection = 0;
    let writes = Promise.resolve();
    const serialize = (operation: () => Promise<void>) => {
        writes = writes.catch(() => {}).then(operation);
        return writes;
    };

    async function select(file: Blob & { name?: string }, fromCache = false): Promise<void> {
        const current = ++selection;
        setStatus({ kind: 'checking' });
        setNotice('');
        try {
            const rom = await validateRom(file);
            if (current !== selection) return;
            setStatus({ kind: 'ready', rom });
            if (fromCache) {
                setRemembered(true);
                setCanForget(true);
                return;
            }
            if (remember()) {
                try {
                    await serialize(() => rememberRom(rom));
                    setRemembered(true);
                    setCanForget(true);
                } catch {
                    setRemembered(false);
                    setNotice(
                        'ROM selected, but it could not be remembered. You can play this session and choose it again next time.',
                    );
                }
            }
        } catch (error) {
            if (current === selection)
                setStatus({
                    kind: 'error',
                    message: error instanceof Error ? error.message : String(error),
                });
        }
    }

    async function restore(): Promise<boolean> {
        try {
            try {
                setRemember(localStorage.getItem(PREFERENCE) !== 'false');
            } catch {
                /* Preference storage is optional. */
            }
            const pendingSelection = selection;
            const maybeRom = await recalledRom();
            if (maybeRom) setCanForget(true);
            if (maybeRom && !remember()) {
                await serialize(forgetRom);
                setCanForget(false);
                return false;
            }
            if (selection === pendingSelection && maybeRom && remember()) {
                await select(maybeRom, true);
                return status().kind === 'ready' && remembered();
            }
        } catch {
            setCanForget(true);
            setNotice('The remembered ROM could not be read. Choose your original ROM again.');
        }
        return false;
    }

    async function toggleRemember(value: boolean): Promise<void> {
        setRemember(value);
        try {
            localStorage.setItem(PREFERENCE, String(value));
        } catch {
            /* Keep the current choice for this session. */
        }
        try {
            if (!value) {
                await serialize(forgetRom);
                setRemembered(false);
                setCanForget(false);
                return;
            }
            const selected = status();
            if (selected.kind === 'ready') {
                await serialize(() => rememberRom(selected.rom));
                setRemembered(true);
                setCanForget(true);
            }
        } catch {
            setNotice(
                'Browser storage could not be updated. Your selected ROM is still available for this session.',
            );
        }
    }

    async function forget(): Promise<void> {
        try {
            await serialize(forgetRom);
            setRemembered(false);
            setCanForget(false);
            if (status().kind === 'error') setStatus({ kind: 'empty' });
            setNotice('The remembered ROM was removed. Saves are unchanged.');
        } catch {
            setNotice('The remembered ROM could not be removed. Try again.');
        }
    }
    return {
        status,
        remember,
        remembered,
        canForget,
        notice,
        select,
        restore,
        toggleRemember,
        forget,
    };
}
export type RomManager = ReturnType<typeof createRomManager>;
