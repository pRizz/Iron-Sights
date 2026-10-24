import { createSignal, onCleanup, onMount, Show } from 'solid-js';
import { Player } from './player';
import { StoragePanel } from './storage-panel';
import { createBackup, parseBackup, MAX_BACKUP_TEXT, type UserFile } from './backup';
import { createCapture } from './capture';
import { createEngine, ensureBrowserSettings } from './engine';
import { createRomManager } from './rom-manager';

const SOURCE = 'https://github.com/pRizz/Project-Midas-ADS';

export function App() {
    const engine = createEngine();
    const rom = createRomManager();
    const [started, setStarted] = createSignal(false);
    const capture = createCapture(engine, started);
    const [action, setAction] = createSignal('');
    const [busy, setBusy] = createSignal(false);
    const [maybeImport, setImport] = createSignal<UserFile[]>();
    const [protection, setProtection] = createSignal('Checking storage protection');
    let canvas!: HTMLCanvasElement;
    let romInput!: HTMLInputElement;
    let backupInput!: HTMLInputElement;
    let dialog!: HTMLDialogElement;
    const ready = () => engine.status().kind === 'ready' && rom.status().kind === 'ready';
    const build = () => engine.maybeManifest()?.build;

    onMount(() => {
        const detach = capture.attach(canvas);
        void engine.initialize(canvas);
        void rom.restore();
        void navigator.storage
            ?.persisted?.()
            .then((value) =>
                setProtection(value ? 'Storage protection granted' : 'Browser-managed storage'),
            )
            .catch(() => setProtection('Storage protection unavailable'));
        const preventNavigation = (event: DragEvent) => {
            if (event.dataTransfer?.types.includes('Files')) event.preventDefault();
        };
        window.addEventListener('dragover', preventNavigation);
        window.addEventListener('drop', preventNavigation);
        const beforeClose = (event: BeforeUnloadEvent) => {
            if (!engine.maybeFiles()?.pending()) return;
            event.preventDefault();
            event.returnValue = '';
        };
        window.addEventListener('beforeunload', beforeClose);
        onCleanup(() => {
            detach();
            window.removeEventListener('dragover', preventNavigation);
            window.removeEventListener('drop', preventNavigation);
            window.removeEventListener('beforeunload', beforeClose);
        });
    });

    async function play(): Promise<void> {
        const selected = rom.status();
        const game = engine.maybeGame();
        const files = engine.maybeFiles();
        if (!ready() || selected.kind !== 'ready' || !game || !files || started()) return;
        try {
            ensureBrowserSettings(game);
            await files.flush();
            game.FS.writeFile('/rom.z64', selected.rom.bytes.slice());
            files.markStarted();
            setStarted(true);
            capture.starting();
            canvas.focus();
            await new Promise(requestAnimationFrame);
            game.callMain(['--rom-file', '/rom.z64']);
        } catch {
            engine.failed(
                'The game could not start. Download your saves, then reload to try again.',
            );
        }
    }

    async function downloadBackup(): Promise<void> {
        const files = engine.maybeFiles();
        if (!files) return;
        try {
            const text = await createBackup(files.snapshot(), new Date().toISOString());
            const url = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
            const link = document.createElement('a');
            link.href = url;
            link.download = `midas-saves-${new Date().toISOString().slice(0, 10)}.json`;
            link.click();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
            setAction('Save backup downloaded. It contains no ROM.');
        } catch (error) {
            setAction(String(error));
        }
    }

    async function chooseBackup(file: File): Promise<void> {
        try {
            if (file.size > MAX_BACKUP_TEXT) throw new Error('The backup is too large.');
            setImport(await parseBackup(await file.text()));
            dialog.showModal();
        } catch (error) {
            setAction(error instanceof Error ? error.message : String(error));
        }
    }

    async function restoreBackup(): Promise<void> {
        const files = engine.maybeFiles();
        const replacement = maybeImport();
        if (!files || !replacement) return;
        setBusy(true);
        try {
            await files.replace(replacement);
            setAction('Backup restored. Your saved data is ready.');
        } catch (error) {
            setAction(`Backup could not be restored. ${String(error)}`);
        } finally {
            setBusy(false);
            dialog.close();
            setImport(undefined);
        }
    }

    async function protectStorage(): Promise<void> {
        try {
            setProtection(
                (await navigator.storage.persist())
                    ? 'Storage protection granted'
                    : 'Protection declined · backups remain available',
            );
        } catch {
            setProtection('Storage protection unavailable · backups remain available');
        }
    }

    async function fullscreen(): Promise<void> {
        try {
            if (document.fullscreenElement) await document.exitFullscreen();
            else await canvas.parentElement!.requestFullscreen();
        } catch {
            setAction('Fullscreen was refused. You can continue in the page.');
        }
    }

    return (
        <div class="app-shell" id="top">
            <header class="site-header">
                <a class="brand" href="#top">
                    <span class="brand-mark" aria-hidden="true">
                        M
                    </span>
                    <span>
                        PROJECT MIDAS <b>ADS</b>
                        <small>THE BROWSER PREVIEW</small>
                    </span>
                </a>
                <a class="source-link" href={SOURCE} target="_blank" rel="noreferrer">
                    <span aria-hidden="true">&lt;/&gt;</span> Source code
                </a>
            </header>
            <main>
                <div class="page-intro">
                    <div>
                        <p class="eyebrow">YOUR COPY. YOUR MISSION.</p>
                        <h1>GoldenEye. In your browser.</h1>
                        <p>
                            Solo campaign, mouse and keyboard. Your ROM and saves stay on this
                            device.
                        </p>
                    </div>
                    <span class="preview-tag">DESKTOP PREVIEW</span>
                </div>
                <div class="workspace">
                    <Player
                        engine={engine}
                        rom={rom}
                        capture={capture}
                        started={started}
                        ready={ready}
                        onCanvas={(element) => {
                            canvas = element;
                        }}
                        chooseROM={() => romInput.click()}
                        fullscreen={fullscreen}
                        play={play}
                        notify={setAction}
                    />
                    <StoragePanel
                        engine={engine}
                        rom={rom}
                        started={started}
                        protection={protection}
                        downloadBackup={downloadBackup}
                        chooseBackup={() => backupInput.click()}
                        protectStorage={protectStorage}
                        notify={setAction}
                    />
                </div>
                <Show when={action()}>
                    <p class="action-notice" role="status">
                        {action()}
                    </p>
                </Show>
                <details class="about-build">
                    <summary>About this build</summary>
                    <Show when={build()}>
                        {(info) => (
                            <p>
                                Game build {info().build_number}-{info().build_date} ·{' '}
                                {info().builder}
                                {info().dirty ? ' with local changes' : ''} ·{' '}
                                <a href={info().source_url} target="_blank" rel="noreferrer">
                                    {info().commit.slice(0, 9)}
                                </a>
                                <Show when={info().workflow_url}>
                                    {' '}
                                    ·{' '}
                                    <a href={info().workflow_url} target="_blank" rel="noreferrer">
                                        Build run
                                    </a>
                                </Show>
                            </p>
                        )}
                    </Show>
                    <details>
                        <summary>Technical details</summary>
                        <pre>{engine.logs().join('\n')}</pre>
                    </details>
                </details>
            </main>
            <footer>
                <span>Project Midas ADS · Browser preview</span>
                <div>
                    <a href={SOURCE + '/blob/main/LICENSE'} target="_blank" rel="noreferrer">
                        Port code &amp; licenses
                    </a>
                    <a href={SOURCE} target="_blank" rel="noreferrer">
                        Source
                    </a>
                    <a href="https://openlinks.us/" target="_blank" rel="noreferrer">
                        Peter Ryszkiewicz ↗
                    </a>
                </div>
            </footer>
            <input
                ref={romInput}
                id="rom-file"
                type="file"
                hidden
                accept=".z64,.v64,.n64,.rom,.bin"
                onChange={(event) => {
                    const maybeFile = event.currentTarget.files?.[0];
                    if (maybeFile) void rom.select(maybeFile);
                    event.currentTarget.value = '';
                }}
            />
            <input
                ref={backupInput}
                id="backup-file"
                type="file"
                hidden
                accept=".json"
                onChange={(event) => {
                    const maybeFile = event.currentTarget.files?.[0];
                    if (maybeFile) void chooseBackup(maybeFile);
                    event.currentTarget.value = '';
                }}
            />
            <dialog
                ref={dialog}
                onCancel={(event) => {
                    if (busy()) event.preventDefault();
                    else setImport(undefined);
                }}
            >
                <p class="eyebrow">RESTORE BACKUP</p>
                <h2>Replace local saves?</h2>
                <p>
                    This backup contains {maybeImport()?.length || 0} files. Your current campaign,
                    profiles and settings will be replaced.
                </p>
                <div class="dialog-actions">
                    <button
                        disabled={busy()}
                        onClick={() => {
                            dialog.close();
                            setImport(undefined);
                        }}
                    >
                        Cancel
                    </button>
                    <button
                        class="solid-primary"
                        disabled={busy()}
                        onClick={() => void restoreBackup()}
                    >
                        {busy() ? 'Restoring…' : 'Restore backup'}
                    </button>
                </div>
            </dialog>
        </div>
    );
}
