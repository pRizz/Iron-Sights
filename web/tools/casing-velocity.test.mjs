import { expect, test } from 'bun:test';
import { readFile } from 'node:fs/promises';

const gunfire = await readFile(new URL('../../src/game/gunfire.c', import.meta.url), 'utf8');

// Upstream's decompiled line multiplies an unset local by zero. GCC yields 0, but
// Clang folds it to NaN, and in Wasm a NaN casing never lands and draws at the camera.
test('ejected casing velocity never multiplies the unset local, including after merges', () => {
    expect(gunfire).toContain('casing->vel');
    expect(gunfire).not.toMatch(/casing->vel\.[xyz]\s*=\s*frac\s*\*/);
});
