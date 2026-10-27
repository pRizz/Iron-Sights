import { expect, test } from 'bun:test';
import { FAVICON_SOURCE, frontendFiles } from './frontend-files';

test('includes the explicit project icon and its HTML entry reference', () => {
    const icon = 'assets/ProjectMidas-abc.ico';
    const manifest = {
        [FAVICON_SOURCE]: { src: FAVICON_SOURCE, file: icon },
        'index.html': { file: 'assets/main-abc.js', assets: [icon] },
    };
    expect(frontendFiles(manifest)).toEqual([icon, 'assets/main-abc.js']);
});

test('rejects an icon reference without its explicit source record', () => {
    expect(() =>
        frontendFiles({
            'index.html': {
                file: 'assets/main-abc.js',
                assets: ['assets/ProjectMidas-abc.ico'],
            },
        }),
    ).toThrow();
});

test('rejects a favicon entry that disguises an unrelated source', () => {
    expect(() =>
        frontendFiles({
            [FAVICON_SOURCE]: {
                src: '../private.ico',
                file: 'assets/ProjectMidas-abc.ico',
            },
        }),
    ).toThrow();
});

test('collects exactly the entry, CSS and dynamic scene graph', () => {
    const manifest = {
        'index.html': {
            file: 'assets/main-abc.js',
            css: ['assets/main-abc.css'],
            dynamicImports: ['scene'],
        },
        scene: { file: 'assets/scene-def.js', imports: ['index.html'] },
    };
    expect(frontendFiles(manifest)).toEqual([
        'assets/main-abc.js',
        'assets/main-abc.css',
        'assets/scene-def.js',
    ]);
});
for (const path of [
    '../rom.z64',
    'assets/rom.z64',
    'https://example.com/file.js',
    'assets/../file.js',
    'assets/file.js/extra',
    'assets/unapproved.ico',
])
    test('rejects unsafe or non-code artifact ' + path, () => {
        expect(() => frontendFiles({ a: { file: path } })).toThrow();
    });
for (const value of [
    null,
    [],
    {},
    { a: null },
    { a: { file: 'assets/a.js', css: 'bad' } },
    { a: { file: 'assets/a.js', dynamicImports: ['missing'] } },
    { a: { file: 'assets/a.js', assets: ['assets/private.png'] } },
])
    test('rejects malformed manifests ' + JSON.stringify(value), () => {
        expect(() => frontendFiles(value)).toThrow();
    });
