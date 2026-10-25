import { Show, type Accessor } from 'solid-js';
import type { RomManager } from '../rom-manager';

type Props = {
    rom: RomManager;
    selecting: Accessor<boolean>;
    maybeError: Accessor<string | undefined>;
    controls: Accessor<boolean>;
    onDialog: (dialog: HTMLDialogElement) => void;
    select: (file: File) => Promise<void>;
    error: (message: string) => void;
};

export function RomDialog(props: Props) {
    let dialog!: HTMLDialogElement;
    let input!: HTMLInputElement;
    return (
        <dialog
            ref={(element) => {
                dialog = element;
                props.onDialog(element);
            }}
            class="rom-dialog"
            aria-labelledby="rom-dialog-title"
            onCancel={(event) => {
                if (props.selecting()) event.preventDefault();
            }}
            onDragOver={(event) => event.preventDefault()}
            onDrop={(event) => {
                event.preventDefault();
                event.stopPropagation();
                const files = event.dataTransfer?.files;
                if (files?.length === 1) void props.select(files[0]);
                else props.error('Drop one GoldenEye ROM at a time.');
            }}
        >
            <p class="eyebrow">YOUR COPY. YOUR MISSION.</p>
            <h2 id="rom-dialog-title">Insert your GoldenEye ROM</h2>
            <p>Drop your original US ROM here. Your file stays on this device.</p>
            <div class="rom-drop-zone">
                <button disabled={props.selecting()} onClick={() => input.click()}>
                    {props.selecting() ? 'Checking cartridge…' : 'Choose ROM'}
                </button>
                <small>12 MiB · .z64 / .v64 / .n64</small>
            </div>
            <Show when={props.maybeError()}>{(message) => <p role="alert">{message()}</p>}</Show>
            <Show when={!props.controls() && props.rom.notice()}>
                <p role="status">{props.rom.notice()}</p>
            </Show>
            <label class="check-row">
                <input
                    type="checkbox"
                    checked={props.rom.remember()}
                    onChange={(event) => void props.rom.toggleRemember(event.currentTarget.checked)}
                />
                Remember ROM on this device
            </label>
            <button class="text-button" disabled={props.selecting()} onClick={() => dialog.close()}>
                Back to the room
            </button>
            <input
                ref={input}
                type="file"
                hidden
                accept=".z64,.v64,.n64,.rom,.bin"
                onChange={(event) => {
                    const maybeFile = event.currentTarget.files?.[0];
                    if (maybeFile) void props.select(maybeFile);
                    event.currentTarget.value = '';
                }}
            />
        </dialog>
    );
}
