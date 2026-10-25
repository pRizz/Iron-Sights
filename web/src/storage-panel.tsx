import { createMemo, Show, type Accessor } from 'solid-js';
import type { Engine } from './engine';
import type { RomManager } from './rom-manager';

type StorageProps = {
    engine: Engine;
    rom: RomManager;
    started: Accessor<boolean>;
    protection: Accessor<string>;
    downloadBackup: () => Promise<void>;
    chooseBackup: () => void;
    protectStorage: () => Promise<void>;
    manageSaves: () => Promise<void>;
    notify: (message: string) => void;
};

export function StoragePanel(props: StorageProps) {
    const { engine, rom, started, protection } = props;
    const romName = () => {
        const value = rom.status();
        return value.kind === 'ready'
            ? value.rom.name
            : value.kind === 'checking'
              ? 'Checking ROM…'
              : 'No ROM selected';
    };
    const maybeSaveError = () => {
        const value = engine.saveStatus();
        return value.kind === 'error' ? value.message : undefined;
    };
    const saveLabel = createMemo(() => {
        const status = engine.saveStatus();
        if (!engine.maybeFiles()) return 'Opening saved data';
        return status.kind === 'saved'
            ? 'Saved on this device'
            : status.kind === 'saving'
              ? 'Saving…'
              : status.kind === 'pending'
                ? 'Save pending'
                : 'Save failed · retry or download a backup';
    });
    return (
        <aside class="sidebar">
            <section class="storage-card">
                <p class="eyebrow">ON THIS DEVICE</p>
                <h2>Your game data</h2>
                <div class="data-row">
                    <span class="data-icon" aria-hidden="true">
                        ◇
                    </span>
                    <div>
                        <strong>GoldenEye ROM</strong>
                        <p class="filename">{romName()}</p>
                    </div>
                </div>
                <Show when={rom.remembered()}>
                    <span class="remembered">✓ Remembered for next time</span>
                </Show>
                <label class="check-row">
                    <input
                        type="checkbox"
                        checked={rom.remember()}
                        onChange={(event) => void rom.toggleRemember(event.currentTarget.checked)}
                    />
                    Remember ROM on this device
                </label>
                <button
                    class="text-button"
                    disabled={!rom.canForget()}
                    onClick={() => void rom.forget()}
                >
                    Forget remembered ROM
                </button>
                <Show when={rom.notice()}>
                    <p class="notice" role="status">
                        {rom.notice()}
                    </p>
                </Show>
                <hr />
                <div class="data-row">
                    <span class="data-icon" aria-hidden="true">
                        ↳
                    </span>
                    <div>
                        <strong>Campaign &amp; settings</strong>
                        <p classList={{ 'save-error': engine.saveStatus().kind === 'error' }}>
                            {saveLabel()}
                        </p>
                    </div>
                </div>
                <Show when={maybeSaveError()}>
                    {(message) => (
                        <>
                            <p class="notice" role="alert">
                                {message()}
                            </p>
                            <button
                                onClick={() =>
                                    void engine
                                        .maybeFiles()
                                        ?.flush()
                                        .catch((error) => props.notify(String(error)))
                                }
                            >
                                Retry save
                            </button>
                        </>
                    )}
                </Show>
                <button
                    class="wide-button"
                    disabled={!engine.maybeFiles()}
                    onClick={() => void props.downloadBackup()}
                >
                    ↓ Download save backup
                </button>
                <button
                    class="wide-button secondary"
                    disabled={!engine.maybeFiles() || started()}
                    onClick={() => props.chooseBackup()}
                >
                    ↑ Restore save backup
                </button>
                <p class="fine-print">
                    {started()
                        ? 'Restart to manage saves before the game loads.'
                        : 'Backups include saves, profiles and settings.'}{' '}
                    ROMs are kept separately.
                </p>
                <Show when={started()}>
                    <button class="text-button" onClick={() => void props.manageSaves()}>
                        Restart to manage saves
                    </button>
                </Show>
                <hr />
                <p class="protection">{protection()}</p>
                <p class="fine-print">
                    Browser storage can be cleared. Keep a downloaded backup of your progress.
                </p>
                <button
                    class="text-button"
                    disabled={
                        !navigator.storage?.persist || protection() === 'Storage protection granted'
                    }
                    onClick={() => void props.protectStorage()}
                >
                    Request storage protection
                </button>
            </section>
            <section class="controls-card">
                <p class="eyebrow">DEFAULT CONTROLS</p>
                <div>
                    <kbd>W A S D</kbd>
                    <span>Move</span>
                </div>
                <div>
                    <kbd>MOUSE</kbd>
                    <span>Look &amp; aim</span>
                </div>
                <div>
                    <kbd>LEFT CLICK</kbd>
                    <span>Fire</span>
                </div>
                <div>
                    <kbd>P</kbd>
                    <span>Watch / pause</span>
                </div>
                <div>
                    <kbd>ESC</kbd>
                    <span>Release mouse</span>
                </div>
                <p class="fine-print">Change bindings in the game's Control Options.</p>
            </section>
        </aside>
    );
}
