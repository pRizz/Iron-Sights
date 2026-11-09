import { expect, test } from 'bun:test';
import { readFileSync } from 'node:fs';
import { publicBrandAssets } from './brand-assets';

const digest = 'a'.repeat(64);

test('keeps only web/public entries, at their published paths', () => {
    const checksums = [
        `${digest}  port/win/ProjectMidas.ico`,
        `${digest}  web/icons/favicon.ico`,
        `${digest}  web/public/icons/icon-192.png`,
        `${digest}  web/public/social-card.png`,
        '',
    ].join('\n');
    expect(publicBrandAssets(checksums)).toEqual([
        { path: 'icons/icon-192.png', sha256: digest },
        { path: 'social-card.png', sha256: digest },
    ]);
});

test('reads the committed generator record', () => {
    const checksums = readFileSync(new URL('../../scripts/port/brand-art.sha256', import.meta.url), 'utf8');
    expect(publicBrandAssets(checksums).map((asset) => asset.path)).toEqual([
        'icons/icon-192.png',
        'icons/icon-512.png',
        'icons/icon-maskable-512.png',
        'social-card.png',
    ]);
});

for (const line of [
    'not a checksum',
    `${digest} web/public/social-card.png`,
    `${digest}  web/public/../secret.png`,
    `${digest}  web/public/room/model.glb`,
])
    test('rejects checksum line ' + JSON.stringify(line), () => {
        expect(() => publicBrandAssets(line + '\n')).toThrow();
    });

test('rejects a record with no published images', () => {
    expect(() => publicBrandAssets(`${digest}  port/include/icon.png\n`)).toThrow();
});
