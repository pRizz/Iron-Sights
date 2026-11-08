import { expect, test } from 'bun:test';
import { ICON_SOURCES, frontendFiles } from './frontend-files';

const [FAVICON, TOUCH_ICON] = ICON_SOURCES.map(({ source }) => source);

test('includes the explicit launcher icons and their HTML entry references', () => {
    const favicon = 'assets/favicon-abc.ico';
    const touchIcon = 'assets/apple-touch-icon-abc.png';
    const manifest = {
        [FAVICON]: { src: FAVICON, file: favicon },
        [TOUCH_ICON]: { src: TOUCH_ICON, file: touchIcon },
        'index.html': { file: 'assets/main-abc.js', assets: [favicon, touchIcon] },
    };
    expect(frontendFiles(manifest)).toEqual([favicon, touchIcon, 'assets/main-abc.js']);
});

test('rejects an icon reference without its explicit source record', () => {
    expect(() =>
        frontendFiles({
            'index.html': {
                file: 'assets/main-abc.js',
                assets: ['assets/favicon-abc.ico'],
            },
        }),
    ).toThrow();
});

test('rejects an icon entry that disguises an unrelated source', () => {
    expect(() =>
        frontendFiles({
            [FAVICON]: {
                src: '../private.ico',
                file: 'assets/favicon-abc.ico',
            },
        }),
    ).toThrow();
});

test('rejects an icon emitted under another icon name', () => {
    expect(() =>
        frontendFiles({
            [TOUCH_ICON]: { src: TOUCH_ICON, file: 'assets/favicon-abc.ico' },
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
    'assets/apple-touch-icon-abc.png',
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
