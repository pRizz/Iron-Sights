import { Show } from 'solid-js';
import { touchLayoutPreference, type TouchInput } from './touch-input';

export function TouchLayoutSetting(props: { input: TouchInput; beforeChange: () => void }) {
    return (
        <Show when={props.input.enabled()}>
            <label class="check-row touch-setting touch-layout-setting">
                Touch layout
                <select
                    aria-label="Touch layout"
                    value={props.input.layout()}
                    onChange={(event) => {
                        props.beforeChange();
                        props.input.setLayout(touchLayoutPreference(event.currentTarget.value));
                    }}
                >
                    <option value="n64">N64 controller</option>
                    <option value="modern">Modern actions</option>
                </select>
            </label>
        </Show>
    );
}
