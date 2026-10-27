import './save-actions.css';

type Props = {
    ready: boolean;
    started: boolean;
    importSave: () => void;
    exportSave: () => void;
};

export function RoomSaveActions(props: Props) {
    return (
        <section class="room-save-actions" aria-label="Save transfer">
            <div>
                <button disabled={!props.ready} onClick={props.importSave}>
                    Import Save
                </button>
                <button disabled={!props.ready} onClick={props.exportSave}>
                    Export Save
                </button>
            </div>
            <p>
                {!props.ready
                    ? 'Opening saved data…'
                    : props.started
                      ? 'Import opens restart and restore options.'
                      : 'Portable backup · saves, profiles and settings'}
            </p>
        </section>
    );
}
