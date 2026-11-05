import { expect, test } from 'bun:test';
import { touchContext } from './touch-context';

test('native touch contexts distinguish menus, cutscenes, gameplay, watch and Options folder states', () => {
    const contexts = [
        'navigation',
        'cinematic',
        'game',
        'watch',
        'edit',
        'list',
        'capture',
        'assign',
    ] as const;
    for (const [native, context] of contexts.entries()) expect(touchContext(native)).toBe(context);
});

test('unknown native context keeps gameplay controls hidden', () => {
    for (const value of [undefined, null, -1, 8, NaN, Infinity, '2', {}])
        expect(touchContext(value)).toBe('cinematic');
});
