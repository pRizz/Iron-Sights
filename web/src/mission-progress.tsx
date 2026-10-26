import { Show } from 'solid-js';

export function MissionProgress(props: { label: string; maybePercent: number | null }) {
    return (
        <div class="mission-progress" data-indeterminate={props.maybePercent === null}>
            <Show
                when={props.maybePercent !== null}
                fallback={<progress class="visually-hidden" aria-label={props.label} max="100" />}
            >
                <progress
                    class="visually-hidden"
                    aria-label={props.label}
                    max="100"
                    value={props.maybePercent ?? 0}
                />
            </Show>
            <div class="mission-gauge" aria-hidden="true">
                <span style={{ width: `${props.maybePercent ?? 25}%` }} />
            </div>
            <p class="startup-measure" aria-hidden="true">
                <span>
                    {props.maybePercent === null ? 'Operation in progress' : 'Preparation status'}
                </span>
                <strong>
                    {props.maybePercent === null ? 'Please wait' : `${props.maybePercent}%`}
                </strong>
            </p>
        </div>
    );
}
