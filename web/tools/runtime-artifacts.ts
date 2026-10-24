import { createHash } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import type { BuildInfo, RuntimeManifest } from '../src/runtime-manifest.ts';

export const GAME_FILES = [
    'ProjectMidas.mjs',
    'ProjectMidas.wasm',
    'gamecontrollerdb.txt',
    'gamecontrollerdefault.txt',
    'Data/shaders_base.txt',
] as const;
const source = new URL('../../build/web/video/', import.meta.url);

export async function runtimeArtifacts() {
    const [files, build] = await Promise.all([
        Promise.all(
            GAME_FILES.map(async (name) => [name, await readFile(new URL(name, source))] as const),
        ),
        readFile(new URL('generated/build-info.json', source), 'utf8').then(
            (text) => JSON.parse(text) as BuildInfo,
        ),
    ]);
    const hash = createHash('sha256');
    for (const [name, bytes] of files) hash.update(name).update(bytes);
    const version = hash.digest('hex').slice(0, 16);
    const wasm = files.find(([name]) => name === 'ProjectMidas.wasm')![1];
    const manifest: RuntimeManifest = {
        schema: 1,
        version,
        base: `runtime/${version}/`,
        wasmBytes: wasm.length,
        wasmSHA256: createHash('sha256').update(wasm).digest('hex'),
        build,
    };
    return { files: new Map<string, Buffer>(files), manifest };
}
