// Check only the assembled tree; local build logs/captures are never published.
import { readdir, readFile, lstat } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { parseRuntimeManifest } from '../src/runtime-manifest.ts';
import { GAME_FILES } from './runtime-artifacts.ts';
import { ROOM_ASSETS } from './room-assets.ts';
import { frontendFiles } from './frontend-files';

const root = new URL('../../build/web-site/', import.meta.url);
const manifest = parseRuntimeManifest(
    JSON.parse(await readFile(new URL('runtime-manifest.json', root), 'utf8')),
);
const html = await readFile(new URL('index.html', root), 'utf8');
const frontend = frontendFiles(
    JSON.parse(await readFile(new URL('frontend-manifest.json', root), 'utf8')),
);
const maybeFavicon = html.match(
    /<link rel="icon" type="image\/x-icon" href="\.\/(assets\/[^"/]+\.ico)"/,
)?.[1];
if (!maybeFavicon || !frontend.includes(maybeFavicon))
    throw new Error('The site is missing its project favicon');
const [favicon, originalIcon] = await Promise.all([
    readFile(new URL(maybeFavicon, root)),
    readFile(new URL('../../port/win/ProjectMidas.ico', import.meta.url)),
]);
if (!favicon.equals(originalIcon))
    throw new Error('The favicon differs from the native project icon');
const linked = [...html.matchAll(/(?:src|href)="\.\/(assets\/[a-zA-Z0-9_-]+\.(?:js|css))"/g)].map(
    (match) => match[1],
);
if (
    !linked.some((file) => file.endsWith('.js')) ||
    !linked.some((file) => file.endsWith('.css')) ||
    linked.some((file) => !frontend.includes(file))
)
    throw new Error('The site is missing its built launcher assets');
const allowed = new Set([
    'index.html',
    'frontend-manifest.json',
    'runtime-manifest.json',
    'LICENSE.txt',
    'THIRD_PARTY_NOTICES.txt',
    ...frontend,
    ...ROOM_ASSETS.map((file) => file.path),
    ...GAME_FILES.map((file) => manifest.base + file),
]);
async function walk(relative = ''): Promise<void> {
    for (const name of await readdir(new URL(relative, root))) {
        const path = relative + name;
        const stat = await lstat(new URL(path, root));
        if (stat.isSymbolicLink()) throw new Error('A site artifact must not contain symlinks');
        if (stat.isDirectory()) await walk(path + '/');
        else if (!allowed.delete(path)) throw new Error('Unexpected public artifact: ' + path);
    }
}
await walk();
if (allowed.size) throw new Error('Missing public artifacts: ' + [...allowed].join(', '));
const wasm = await readFile(new URL(manifest.base + 'ProjectMidas.wasm', root));
if (
    wasm.length !== manifest.wasmBytes ||
    createHash('sha256').update(wasm).digest('hex') !== manifest.wasmSHA256
)
    throw new Error('The assembled Wasm does not match its manifest');
if (!WebAssembly.validate(wasm)) throw new Error('Invalid compiled Wasm');
for (const asset of ROOM_ASSETS) {
    const bytes = await readFile(new URL(asset.path, root));
    if (
        bytes.length !== asset.bytes ||
        createHash('sha256').update(bytes).digest('hex') !== asset.sha256
    )
        throw new Error('The public room asset does not match its recorded source: ' + asset.path);
}
console.log(`Verified exact public allowlist and Wasm digest (${manifest.version})`);
