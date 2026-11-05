import { For, Show } from 'solid-js';
import { stageNumber, stageTrack } from './startup-progress';

export function MissionProgress(props: { label: string; maybePercent: number | null }) {
    const maybeTrack = () => stageTrack(props.label);
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
            <Show when={maybeTrack()}>
                {(track) => (
                    <div class="mission-stages" data-stage={stageNumber(track())}>
                        <span class="visually-hidden">
                            {`Startup stage ${stageNumber(track())} of ${track().total}`}
                        </span>
                        <ol aria-hidden="true">
                            <For each={Array.from({ length: track().total }, (_, index) => index)}>
                                {(index) => (
                                    <li
                                        data-state={
                                            index < track().current
                                                ? 'done'
                                                : index === track().current
                                                  ? 'current'
                                                  : 'pending'
                                        }
                                    />
                                )}
                            </For>
                        </ol>
                        <p class="startup-measure" aria-hidden="true">
                            <span>{`Stage ${stageNumber(track())} of ${track().total}`}</span>
                            <span>{track().name}</span>
                        </p>
                    </div>
                )}
            </Show>
        </div>
    );
}
