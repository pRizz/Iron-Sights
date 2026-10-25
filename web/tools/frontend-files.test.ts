import { expect, test } from 'bun:test';
import { frontendFiles } from './frontend-files';

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
