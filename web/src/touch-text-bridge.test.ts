import { expect, test } from 'bun:test';
import {
    NAME_MAX_LENGTH,
    finishNativeName,
    readNativeText,
    sanitizeName,
    writeNativeName,
} from './touch-text-bridge';

function fakeGame(title: string) {
    let name = '';
    let finished: number | undefined;
    return {
        game: {
            _portWebTextChar: (field: number, index: number) =>
                (field === 0 ? title : name).charCodeAt(index) || 0,
            _portWebTextClear: () => {
                name = '';
            },
            // The native side caps the name too; the fake trusts the caller.
            _portWebTextAppend: (code: number) => {
                name += String.fromCharCode(code);
            },
            _portWebTextFinish: (ok: number) => {
                finished = ok;
            },
        },
        name: () => name,
        finished: () => finished,
    };
}

test('names keep the native keyboard rules', () => {
    expect(sanitizeName('Bond 007!')).toBe('Bond 007!');
    expect(sanitizeName('a|b<c')).toBe('abc');
    expect(sanitizeName('#1 #2')).toBe('1 #2');
    expect(sanitizeName('Café ☕ tab\there')).toBe('Caf  tabhere');
    expect(sanitizeName('x'.repeat(40))).toHaveLength(NAME_MAX_LENGTH);
});

test('the field reads the title and rewrites the whole name', () => {
    const fake = fakeGame('SAVE SET AS');
    expect(readNativeText(fake.game, 0)).toBe('SAVE SET AS');
    expect(writeNativeName(fake.game, 'Golden|Gun')).toBe('GoldenGun');
    expect(fake.name()).toBe('GoldenGun');
    // An autocorrected or swiped word replaces the name, never appends to it.
    expect(writeNativeName(fake.game, 'Golden Guns')).toBe('Golden Guns');
    finishNativeName(fake.game, true);
    expect(fake.finished()).toBe(1);
    finishNativeName(fake.game, false);
    expect(fake.finished()).toBe(0);
});

test('an older runtime without the text exports reads nothing', () => {
    expect(readNativeText({}, 1)).toBe('');
    expect(writeNativeName({}, 'Name')).toBe('');
});
