import type { GameModule } from './runtime-types';

// Matches the native name keyboard (src/game/front.c): 21 characters, printable
// ASCII only, never '|' (the layout file's separator), '<' (the field's overflow
// mark) or a leading '#' (a comment line to the loader).
export const NAME_MAX_LENGTH = 21;

export function sanitizeName(text: string): string {
    let name = '';
    for (const character of text) {
        if (name.length >= NAME_MAX_LENGTH) break;
        const code = character.charCodeAt(0);
        if (character.length !== 1 || code < 0x20 || code > 0x7e) continue;
        if (character === '|' || character === '<') continue;
        if (character === '#' && name === '') continue;
        name += character;
    }
    return name;
}

type TextGame = Pick<
    GameModule,
    '_portWebTextChar' | '_portWebTextClear' | '_portWebTextAppend' | '_portWebTextFinish'
>;

// Field 0 is the keyboard's title, 1 the name; both cross a character at a time.
export function readNativeText(game: TextGame, field: 0 | 1): string {
    let text = '';
    for (let index = 0; index < 64; index++) {
        const code = game._portWebTextChar?.(field, index) ?? 0;
        if (code === 0) break;
        text += String.fromCharCode(code);
    }
    return text;
}

// Rewrites the whole name, so autocorrect, swipe typing and composition need no
// keystroke mapping; the native side applies the same rules again.
export function writeNativeName(game: TextGame, text: string): string {
    game._portWebTextClear?.();
    for (const character of sanitizeName(text)) game._portWebTextAppend?.(character.charCodeAt(0));
    return readNativeText(game, 1);
}

export function finishNativeName(game: TextGame, ok: boolean): void {
    game._portWebTextFinish?.(Number(ok));
}
