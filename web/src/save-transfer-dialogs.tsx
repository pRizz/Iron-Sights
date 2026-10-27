import { createEffect, type Accessor } from 'solid-js';
import type { UserFile } from './backup';

type Props = {
    maybeImport: Accessor<UserFile[] | undefined>;
    busy: Accessor<boolean>;
    fromGame: Accessor<boolean>;
    pickRequested: Accessor<boolean>;
    dialog: (element: HTMLDialogElement) => void;
    choose: () => void;
    cancel: () => void;
    restore: () => void;
};

export function SaveTransferDialogs(props: Props) {
    let confirmation!: HTMLDialogElement;
    let picker!: HTMLDialogElement;
    createEffect(() => {
        if (props.pickRequested()) picker.showModal();
        else picker.close();
    });
    return (
        <>
            <dialog ref={picker} onCancel={props.cancel}>
                <p class="eyebrow">IMPORT SAVE</p>
                <h2>Import saves</h2>
                <p>
                    Select your backup file. You will confirm replacement before anything changes.
                </p>
                <div class="dialog-actions">
                    <button onClick={props.cancel}>Cancel</button>
                    <button
                        class="solid-primary"
                        onClick={() => {
                            picker.close();
                            props.choose();
                        }}
                    >
                        Choose backup file
                    </button>
                </div>
            </dialog>
            <dialog
                ref={(element) => {
                    confirmation = element;
                    props.dialog(element);
                }}
                onCancel={(event) => {
                    if (props.busy()) event.preventDefault();
                    else props.cancel();
                }}
            >
                <p class="eyebrow">RESTORE BACKUP</p>
                <h2>Replace local saves?</h2>
                <p>
                    This backup contains {props.maybeImport()?.length || 0} files. Your current
                    campaign, profiles and settings will be replaced.
                    {props.fromGame() && ' The game will restart to load the imported saves.'}
                </p>
                <div class="dialog-actions">
                    <button
                        disabled={props.busy()}
                        onClick={() => {
                            confirmation.close();
                            props.cancel();
                        }}
                    >
                        Cancel
                    </button>
                    <button class="solid-primary" disabled={props.busy()} onClick={props.restore}>
                        {props.busy()
                            ? 'Restoring…'
                            : props.fromGame()
                              ? 'Import and restart'
                              : 'Restore backup'}
                    </button>
                </div>
            </dialog>
        </>
    );
}
