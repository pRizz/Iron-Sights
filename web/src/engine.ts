import { createSignal } from 'solid-js';
import { mountPersistentFiles, type PersistentFiles } from './persistent-files';
import { sha256 } from './rom';
import { parseRuntimeManifest, type RuntimeManifest } from './runtime-manifest';
import type { CreateGame, GameModule } from './runtime-types';
import type { SaveStatus } from './sync-queue';
import { acquireUserFilesLock, UserFilesInUse } from './user-lock';
import { workProgress, type StartupProgress } from './startup-progress';

export type EngineStatus =
    | { kind: 'loading'; label: string; maybePercent: number | null }
    | { kind: 'ready' }
    | { kind: 'error'; message: string; needsReload: boolean; reason: 'other-tab' | 'startup' };
const DEFAULT_SETTINGS =
    '[display]\nfullscreen=windowed\nwindow_width=960\nwindow_height=540\n' +
    'render_thread=0\nvsync=0\nmaxfps=60\ntickhz=60\nmsaa=1\nmusic_volume=70\nsfx_volume=70\n' +
    '[input]\nmouse=1\n[controls]\nstart=Return,P\n';

function browserSupport(pointerless: boolean): void {
    if (
        !globalThis.WebAssembly ||
        !globalThis.indexedDB ||
        !globalThis.crypto?.subtle ||
        !navigator.locks
    ) {
        throw new Error(
            'This browser cannot run the game with local saves. Try a current desktop Chrome, Edge or Firefox.',
        );
    }
    if (!pointerless && !('requestPointerLock' in HTMLCanvasElement.prototype))
        throw new Error('Mouse capture is unavailable in this browser.');
    const probe = document.createElement('canvas').getContext('webgl2');
    if (!probe)
        throw new Error(
            'WebGL 2 is unavailable. Enable graphics acceleration or try another desktop browser.',
        );
    probe.getExtension('WEBGL_lose_context')?.loseContext();
}

export function ensureBrowserSettings(game: GameModule): void {
    game.FS.mkdirTree('/user/Data/Saves');
    if (!game.FS.analyzePath('/user/midas.ini').exists)
        game.FS.writeFile('/user/midas.ini', DEFAULT_SETTINGS);
}

async function downloadWasm(
    url: URL,
    expected: number,
    progress: (percent: number) => void,
): Promise<ArrayBuffer> {
    const response = await fetch(url);
    if (!response.ok || !response.body)
        throw new Error('The game download failed. Check your connection and try again.');
    const reader = response.body.getReader();
    const bytes = new Uint8Array(expected);
    let received = 0;
    while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        if (received + value.length > expected) {
            await reader.cancel();
            throw new Error('The game download does not match this build. Reload the page.');
        }
        bytes.set(value, received);
        received += value.length;
        progress(Math.floor((received / expected) * 100));
    }
    if (received !== expected) throw new Error('The game download was interrupted. Try again.');
    return bytes.buffer;
}

export function createEngine(pointerless: () => boolean = () => false) {
    const [status, setStatus] = createSignal<EngineStatus>({
        kind: 'loading',
        label: 'Checking browser',
        maybePercent: null,
    });
    const [saveStatus, setSaveStatus] = createSignal<SaveStatus>({ kind: 'saved' });
    const [maybeGame, setGame] = createSignal<GameModule>();
    const [maybeFiles, setFiles] = createSignal<PersistentFiles>();
    const [maybeManifest, setManifest] = createSignal<RuntimeManifest>();
    const [logs, setLogs] = createSignal<string[]>([]);
    const [maybeStartup, setStartup] = createSignal<StartupProgress>();
    let maybeRelease: (() => void) | undefined;
    let initializing = false;
    let pointerRelease = () => {};
    let roomRequested = () => {};
    const log = (line: string) => setLogs((previous) => [...previous.slice(-99), line]);
    const loading = (label: string, maybePercent: number | null = null) =>
        setStatus({ kind: 'loading', label, maybePercent });
    const failed = (message: string, reason: 'other-tab' | 'startup' = 'startup') =>
        setStatus({ kind: 'error', message, needsReload: !!maybeGame(), reason });

    async function start(): Promise<void> {
        const game = maybeGame();
        if (!game) throw new Error('The game is unavailable');
        setStartup(workProgress('Preparing cartridge', 0, 0));
        // Paint feedback before entering potentially expensive synchronous Wasm work.
        await new Promise(requestAnimationFrame);
        await new Promise(requestAnimationFrame);
        game.callMain(['--rom-file', '/rom.z64']);
        while (game._portWebGetGfxTasks() < 3) {
            if (status().kind === 'error') throw new Error('Game initialization failed');
            await new Promise(requestAnimationFrame);
        }
        setStartup(undefined);
    }

    async function initialize(canvas: HTMLCanvasElement): Promise<void> {
        if (initializing || maybeGame()) return;
        initializing = true;
        try {
            browserSupport(pointerless());
            loading('Opening local saves');
            maybeRelease = await acquireUserFilesLock();
            const manifestURL = new URL(
                import.meta.env.BASE_URL + 'runtime-manifest.json',
                document.baseURI,
            );
            const response = await fetch(manifestURL, { cache: 'no-store' });
            if (!response.ok)
                throw new Error('The game files are unavailable. Reload or try again later.');
            const manifest = parseRuntimeManifest(await response.json());
            setManifest(manifest);
            const base = new URL(manifest.base, manifestURL);
            loading('Downloading game', 0);
            const wasmBinary = await downloadWasm(
                new URL('ProjectMidas.wasm', base),
                manifest.wasmBytes,
                (percent) => loading('Downloading game', percent),
            );
            if ((await sha256(new Uint8Array(wasmBinary))) !== manifest.wasmSHA256)
                throw new Error('The downloaded game does not match this build. Reload to update.');
            loading('Compiling WebAssembly');
            await new Promise(requestAnimationFrame);
            const compiled = await WebAssembly.compile(wasmBinary);
            const { default: create } = (await import(
                /* @vite-ignore */ new URL('ProjectMidas.mjs', base).href
            )) as { default: CreateGame };
            const game = await create({
                canvas,
                noInitialRun: true,
                noExitRuntime: true,
                instantiateWasm(imports, ready) {
                    const instance = new WebAssembly.Instance(compiled, imports);
                    ready(instance);
                    return instance.exports;
                },
                print: log,
                printErr: log,
                onPointerRelease: () => pointerRelease(),
                onRoomRequested: () => roomRequested(),
                onStartupProgress: (label, completed, total) =>
                    setStartup(workProgress(label, completed, total)),
                onAbort: (reason) => {
                    log(String(reason));
                    failed('The game stopped. Download a save backup, then reload to try again.');
                },
                onExit: () => failed('The game has closed. Reload to start again.'),
            });
            setGame(game);
            loading('Restoring local saves');
            const files = await mountPersistentFiles(game.FS, game.IDBFS, setSaveStatus);
            setFiles(files);
            ensureBrowserSettings(game);
            for (const name of [
                'gamecontrollerdb.txt',
                'gamecontrollerdefault.txt',
                'Data/shaders_base.txt',
            ]) {
                const asset = await fetch(new URL(name, base));
                if (!asset.ok)
                    throw new Error(
                        'A game configuration file could not be loaded. Reload to try again.',
                    );
                game.FS.writeFile('/user/' + name, new Uint8Array(await asset.arrayBuffer()));
            }
            await files.flush();
            setStatus({ kind: 'ready' });
        } catch (error) {
            log(String(error));
            failed(
                error instanceof Error
                    ? error.message
                    : 'Local storage could not be opened. Try a normal browsing window.',
                error instanceof UserFilesInUse ? 'other-tab' : 'startup',
            );
            if (!maybeGame()) {
                maybeRelease?.();
                maybeRelease = undefined;
            }
        } finally {
            initializing = false;
        }
    }
    return {
        initialize,
        start,
        maybeStartup,
        failed,
        status,
        saveStatus,
        maybeGame,
        maybeFiles,
        maybeManifest,
        logs,
        onPointerRelease: (callback: () => void) => {
            pointerRelease = callback;
        },
        onRoomRequested: (callback: () => void) => {
            roomRequested = callback;
        },
    };
}
export type Engine = ReturnType<typeof createEngine>;
