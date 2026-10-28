import { createEffect, createSignal, onCleanup, onMount, Show } from 'solid-js';
import { Player } from './player';
import { StartupScreen } from './startup-screen';
import type { StartupProgress } from './startup-progress';
import { StoragePanel } from './storage-panel';
import { createBackup, parseBackup, MAX_BACKUP_TEXT, type UserFile } from './backup';
import { createCapture } from './capture';
import { createEngine, ensureBrowserSettings } from './engine';
import { createTouchInput, initialTouch } from './touch-input';
import { TouchLayoutSetting } from './touch-layout-setting';
import { createRomManager } from './rom-manager';
import { Room } from './room/room';
import { FileSaveActions } from './file-save-actions';
import { SaveTransferDialogs } from './save-transfer-dialogs';
import { restoredEntry, shouldAutoLaunch, type EntryMode } from './room/session';
import { missionStyle } from './mission-theme';

const SOURCE = 'https://github.com/pRizz/Iron-Sights';

export function App() {
    const firstTouch = initialTouch();
    const engine = createEngine(() => touch.enabled());
    const touch = createTouchInput(engine, firstTouch);
    const rom = createRomManager();
    const [started, setStarted] = createSignal(false);
    const [prepared, setPrepared] = createSignal(false);
    const [controls, setControls] = createSignal(
        new URL(location.href).searchParams.has('controls'),
    );
    const [entry, setEntry] = createSignal<EntryMode>('restoring');
    const entered = () => entry() === 'game';
    const roomVisible = () => entry() === 'room' || entry() === 'returning';
    const [hint, setHint] = createSignal(false);
    const [roomFailed, setRoomFailed] = createSignal(false);
    const capture = createCapture(engine, started, touch.enabled);
    const [action, setAction] = createSignal('');
    const [busy, setBusy] = createSignal(false);
    const [maybeImport, setImport] = createSignal<UserFile[]>();
    const [fileImport, setFileImport] = createSignal(false);
    const [pickRequested, setPickRequested] = createSignal(false);
    const [protection, setProtection] = createSignal('Checking storage protection');
    let canvas!: HTMLCanvasElement;
    let romInput!: HTMLInputElement;
    let backupInput!: HTMLInputElement;
    let dialog!: HTMLDialogElement;
    let experience!: HTMLDivElement;
    let launching = false;
    let maybeHintTimer: ReturnType<typeof setTimeout> | undefined;
    const ready = () => engine.status().kind === 'ready' && rom.status().kind === 'ready';
    const build = () => engine.maybeManifest()?.build;
    const canCloseControls = () => engine.status().kind !== 'error' && (entered() || !roomFailed());
    const maybeLoading = (): StartupProgress | undefined => {
        if (controls()) return undefined;
        if (engine.status().kind === 'error') return undefined;
        if (engine.maybeStartup()) return engine.maybeStartup();
        const status = engine.status();
        if (status.kind === 'loading') return status;
        if (entry() === 'restoring')
            return { label: 'Checking saved cartridge', maybePercent: null };
        return undefined;
    };

    function openControls(): void {
        capture.pause('manual');
        setControls(true);
        requestAnimationFrame(() => window.dispatchEvent(new Event('resize')));
    }

    function closeControls(): void {
        if (!canCloseControls()) return;
        setControls(false);
        const url = new URL(location.href);
        if (url.searchParams.has('controls')) {
            url.searchParams.delete('controls');
            history.replaceState(null, '', url);
        }
        requestAnimationFrame(() => {
            window.dispatchEvent(new Event('resize'));
            // Keep the explicit Resume gesture after interacting with controls.
            if (!started()) return;
            canvas.focus();
        });
    }

    function showHint(maybeMessage?: string): void {
        setAction(maybeMessage ?? '');
        setHint(true);
        clearTimeout(maybeHintTimer);
        maybeHintTimer = setTimeout(() => setHint(false), 9000);
    }

    createEffect(() => {
        if (engine.status().kind === 'error' || engine.saveStatus().kind === 'error')
            openControls();
    });
    createEffect(() => {
        engine.maybeGame()?._portWebTouchEnable(Number(touch.enabled()));
    });
    createEffect(() => {
        if (entry() === 'cached' && rom.status().kind === 'error') setEntry('room');
        if (shouldAutoLaunch(entry(), ready(), controls(), started())) void play('cached');
        // First-visit game preparation overlaps cartridge insertion and flight.
        if (entry() === 'room' && ready() && !controls() && !started()) void play('room');
    });

    onMount(() => {
        const detach = capture.attach(canvas);
        capture.onResumeRequest(() => void resumeGame());
        engine.onSaveTransfer((exporting) => transferFromFiles(exporting, true));
        engine.onRoomRequested(() => {
            if (!started()) return;
            capture.pause('manual');
            setRoomFailed(false);
            setControls(false);
            setEntry('returning');
            showHint('Console off · click its power switch to return to the game');
            void engine
                .maybeFiles()
                ?.flush()
                .catch((error) => {
                    setAction(String(error));
                    openControls();
                });
        });
        void engine.initialize(canvas);
        void rom.restore().then((remembered) => {
            if (entry() === 'restoring') setEntry(restoredEntry(remembered));
        });
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
        const keyboard = (event: KeyboardEvent) => {
            if (event.repeat || event.isComposing) return;
            if (event.key === 'F2') {
                event.preventDefault();
                event.stopImmediatePropagation();
                const maybeModal = document.querySelector<HTMLDialogElement>('dialog[open]');
                if (maybeModal) {
                    if (maybeModal.classList.contains('startup-error')) {
                        maybeModal.close();
                        openControls();
                        return;
                    }
                    if (
                        !maybeModal.classList.contains('rom-dialog') ||
                        maybeModal.dataset.busy === 'true'
                    )
                        return;
                    maybeModal.close();
                    openControls();
                    return;
                }
                controls() ? closeControls() : openControls();
            }
            if (event.altKey && event.key === 'Enter') {
                if (
                    event.target instanceof HTMLInputElement ||
                    event.target instanceof HTMLTextAreaElement
                )
                    return;
                event.preventDefault();
                event.stopImmediatePropagation();
                void fullscreen();
            }
        };
        window.addEventListener('keydown', keyboard, true);
        window.addEventListener('beforeunload', beforeClose);
        onCleanup(() => {
            detach();
            window.removeEventListener('dragover', preventNavigation);
            window.removeEventListener('drop', preventNavigation);
            window.removeEventListener('beforeunload', beforeClose);
            window.removeEventListener('keydown', keyboard, true);
            clearTimeout(maybeHintTimer);
        });
    });

    async function play(source: 'controls' | 'room' | 'cached' = 'controls'): Promise<void> {
        const fromRoom = source === 'room';
        const selected = rom.status();
        const game = engine.maybeGame();
        const files = engine.maybeFiles();
        if (!ready() || selected.kind !== 'ready' || !game || !files || launching) return;
        launching = true;
        try {
            if (touch.enabled() && source === 'controls') closeControls();
            if (started()) {
                await files.flush();
                if (!fromRoom) setEntry('game');
                await capture.resume();
                return;
            }
            const rememberSettings = ensureBrowserSettings(game, touch.enabled());
            game.ENV.GE_WEB_FIXED_FRAMEBUFFER = touch.enabled() ? '1' : '0';
            await files.flush();
            rememberSettings();
            const latest = rom.status();
            if (latest.kind !== 'ready' || latest.rom !== selected.rom || !ready()) return;
            if (source === 'cached' && controls()) return;
            game.FS.writeFile('/rom.z64', selected.rom.bytes.slice());
            files.markStarted();
            if (!fromRoom) setEntry('game');
            setStarted(true);
            capture.starting();
            if (!fromRoom) canvas.focus();
            await engine.start();
            // Hold the first actual game frames until the room reaches the TV.
            if (fromRoom && roomVisible() && !roomFailed()) game._portWebSetPaused(1);
            setPrepared(true);
            if (source === 'cached') showHint();
        } catch {
            engine.failed(
                'The game could not start. Download your saves, then reload to try again.',
            );
        } finally {
            launching = false;
        }
    }

    async function resumeGame(): Promise<void> {
        if (!started()) return;
        if (touch.enabled()) touch.gesture();
        setEntry('game');
        await capture.resume();
    }

    async function manageSaves(): Promise<void> {
        capture.pause('manual');
        try {
            await engine.maybeFiles()?.flush();
            const url = new URL(location.href);
            url.searchParams.set('controls', '1');
            location.assign(url);
        } catch (error) {
            setAction(`Could not restart for save management. ${String(error)}`);
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
            link.download = `iron-sights-saves-${new Date().toISOString().slice(0, 10)}.json`;
            link.click();
            setTimeout(() => URL.revokeObjectURL(url), 1000);
            setAction('Save backup downloaded. It contains no ROM.');
        } catch (error) {
            setAction(String(error));
        }
    }

    function transferFromFiles(exporting: boolean, nativeRequest = false): void {
        if (!engine.maybeGame()?._portWebFileMenuMetric(0)) return;
        if (exporting) {
            void downloadBackup();
            return;
        }
        setFileImport(true);
        capture.pause('manual');
        if (nativeRequest) setPickRequested(true);
        else backupInput.click();
    }

    function cancelFileImport(): void {
        if (fileImport()) void resumeGame();
        setFileImport(false);
        setPickRequested(false);
    }

    async function chooseBackup(file: File): Promise<void> {
        try {
            if (file.size > MAX_BACKUP_TEXT) throw new Error('The backup is too large.');
            setImport(await parseBackup(await file.text()));
            dialog.showModal();
        } catch (error) {
            setAction(error instanceof Error ? error.message : String(error));
            if (fileImport()) {
                showHint(error instanceof Error ? error.message : String(error));
                cancelFileImport();
            }
        }
    }

    async function restoreBackup(): Promise<void> {
        const files = engine.maybeFiles();
        const replacement = maybeImport();
        if (!files || !replacement) return;
        setBusy(true);
        try {
            if (started() && fileImport()) {
                await files.stageRestore(replacement);
                location.reload();
                return;
            }
            await files.replace(replacement);
            setAction('Backup restored. Your saved data is ready.');
        } catch (error) {
            setAction(`Backup could not be restored. ${String(error)}`);
            if (fileImport()) {
                showHint(`Backup could not be restored. ${String(error)}`);
                cancelFileImport();
            }
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
            else await experience.requestFullscreen();
        } catch {
            showHint('Fullscreen was refused. You can continue in the page.');
        }
    }

    return (
        <div
            ref={experience}
            class="experience"
            classList={{ 'controls-open': controls() }}
            style={missionStyle}
        >
            <div class="app-shell" id="top">
                <header class="site-header">
                    <a class="brand" href="#top">
                        <span class="brand-mark" aria-hidden="true">
                            IS
                        </span>
                        <span>
                            IRON <b>SIGHTS</b>
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
                            <h1>GoldenEye. In your browser.</h1>
                            <p>
                                Solo campaign with touch, mouse or keyboard. Your ROM and saves stay
                                on this device.
                            </p>
                        </div>
                        <span class="preview-tag">BROWSER PREVIEW</span>
                    </div>
                    <label class="check-row touch-setting">
                        <input
                            type="checkbox"
                            checked={touch.enabled()}
                            onChange={(event) => {
                                if (started()) capture.pause('manual');
                                touch.setEnabled(event.currentTarget.checked);
                            }}
                        />
                        On-screen touch controls
                    </label>
                    <TouchLayoutSetting
                        input={touch}
                        beforeChange={() => {
                            if (started()) capture.pause('manual');
                        }}
                    />
                    <div class="workspace">
                        <Player
                            engine={engine}
                            rom={rom}
                            capture={capture}
                            touch={touch}
                            touchVisible={() => entered() && !controls()}
                            openControls={openControls}
                            presenting={() => entered() || controls()}
                            resume={resumeGame}
                            started={started}
                            ready={ready}
                            onCanvas={(element) => {
                                canvas = element;
                            }}
                            chooseROM={() => romInput.click()}
                            fullscreen={fullscreen}
                            play={() => play()}
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
                            manageSaves={manageSaves}
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
                        <p>
                            Tested desktop browsers: Chrome, Edge and Firefox. Safari is
                            experimental.{' '}
                            <a
                                href={SOURCE + '/blob/main/scripts/port/web_compatibility.md'}
                                target="_blank"
                                rel="noreferrer"
                            >
                                Compatibility notes
                            </a>
                        </p>
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
                                        <a
                                            href={info().workflow_url}
                                            target="_blank"
                                            rel="noreferrer"
                                        >
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
                    <span>Iron Sights · A fork of Project Midas</span>
                    <div>
                        <a href={SOURCE + '/blob/main/LICENSE'} target="_blank" rel="noreferrer">
                            Port code &amp; licenses
                        </a>
                        <a href={SOURCE} target="_blank" rel="noreferrer">
                            Source
                        </a>
                        <a href="./THIRD_PARTY_NOTICES.txt" target="_blank" rel="noreferrer">
                            Third-party notices
                        </a>
                        <a href="https://openlinks.us/" target="_blank" rel="noreferrer">
                            Peter Ryszkiewicz ↗
                        </a>
                    </div>
                </footer>
                {/* Keep ROM selection unfiltered: iOS can disable unknown extensions. */}
                <input
                    ref={romInput}
                    id="rom-file"
                    type="file"
                    hidden
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
                    onCancel={cancelFileImport}
                    onChange={(event) => {
                        const maybeFile = event.currentTarget.files?.[0];
                        if (maybeFile) void chooseBackup(maybeFile);
                        event.currentTarget.value = '';
                    }}
                />
                <SaveTransferDialogs
                    maybeImport={maybeImport}
                    busy={busy}
                    fromGame={fileImport}
                    pickRequested={pickRequested}
                    dialog={(element) => {
                        dialog = element;
                    }}
                    choose={() => {
                        setPickRequested(false);
                        backupInput.click();
                    }}
                    cancel={() => {
                        setImport(undefined);
                        cancelFileImport();
                    }}
                    restore={() => void restoreBackup()}
                />
            </div>
            <StartupScreen
                engine={engine}
                progress={() => (roomVisible() && !roomFailed() ? undefined : maybeLoading())}
                showControls={openControls}
                paused={() => capture.state().kind === 'paused'}
                resume={() => void resumeGame()}
                retry={() => {
                    const status = engine.status();
                    if (status.kind === 'error' && status.needsReload) location.reload();
                    else {
                        setControls(false);
                        void engine.initialize(canvas);
                    }
                }}
            />
            <Show when={roomVisible()}>
                <Room
                    returning={entry() === 'returning'}
                    engine={engine}
                    rom={rom}
                    controls={controls}
                    ready={prepared}
                    progress={maybeLoading}
                    paused={() => capture.state().kind === 'paused'}
                    resume={async () => {
                        if (touch.enabled()) touch.gesture();
                        await capture.resume();
                    }}
                    openControls={openControls}
                    play={() => play('room')}
                    hint={showHint}
                    notify={setAction}
                    unavailable={() => {
                        setRoomFailed(true);
                        if (started()) setEntry('game');
                        openControls();
                    }}
                    finish={() => {
                        setEntry('game');
                        showHint();
                    }}
                />
            </Show>
            <FileSaveActions
                engine={engine}
                canvas={() => canvas}
                visible={() => entered() && !controls() && capture.state().kind !== 'paused'}
                transfer={transferFromFiles}
            />
            <Show when={controls()}>
                <button
                    class="controls-close"
                    disabled={!canCloseControls()}
                    onClick={closeControls}
                >
                    {canCloseControls()
                        ? `Return to ${entered() || entry() === 'cached' ? 'game' : 'room'} · F2`
                        : 'Standard launcher'}
                </button>
            </Show>
            <Show when={hint() && !controls()}>
                <p class="experience-hint" role="status">
                    {action() ||
                        (touch.enabled()
                            ? 'Move with your left thumb · drag right to look · Watch and Options are on screen'
                            : 'Alt+Enter fullscreen · Enter / P watch · F2 controls · Escape may exit fullscreen')}
                </p>
            </Show>
        </div>
    );
}
