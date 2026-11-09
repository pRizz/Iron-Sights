// Check only the assembled tree; local build logs/captures are never published.
import { readdir, readFile, lstat } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { parseRuntimeManifest } from '../src/runtime-manifest.ts';
import { GAME_FILES } from './runtime-artifacts.ts';
import { ROOM_ASSETS } from './room-assets.ts';
import { frontendFiles, ICON_SOURCES } from './frontend-files';
import { publicBrandAssets } from './brand-assets';
import { absoluteSiteTags, renderedSiteTag, siteUrl, SOCIAL_CARD, WEB_MANIFEST } from './site-meta';

const root = new URL('../../build/web-site/', import.meta.url);
const manifest = parseRuntimeManifest(
    JSON.parse(await readFile(new URL('runtime-manifest.json', root), 'utf8')),
);
const html = await readFile(new URL('index.html', root), 'utf8');
const frontend = frontendFiles(
    JSON.parse(await readFile(new URL('frontend-manifest.json', root), 'utf8')),
);
const iconLinks: Record<(typeof ICON_SOURCES)[number]['source'], RegExp> = {
    'icons/favicon.ico': /<link rel="icon" type="image\/x-icon" href="\.\/(assets\/[^"/]+\.ico)"/,
    'icons/apple-touch-icon.png': /<link rel="apple-touch-icon" href="\.\/(assets\/[^"/]+\.png)"/,
};
for (const { source, output } of ICON_SOURCES) {
    const maybeIcon = html.match(iconLinks[source])?.[1];
    if (!maybeIcon || !output.test(maybeIcon) || !frontend.includes(maybeIcon))
        throw new Error('The site is missing its launcher icon: ' + source);
    const [emitted, committed] = await Promise.all([
        readFile(new URL(maybeIcon, root)),
        readFile(new URL('../' + source, import.meta.url)),
    ]);
    if (!emitted.equals(committed))
        throw new Error('The launcher icon differs from its generated source: ' + source);
}
const linked = [...html.matchAll(/(?:src|href)="\.\/(assets\/[a-zA-Z0-9_-]+\.(?:js|css))"/g)].map(
    (match) => match[1],
);
if (
    !linked.some((file) => file.endsWith('.js')) ||
    !linked.some((file) => file.endsWith('.css')) ||
    linked.some((file) => !frontend.includes(file))
)
    throw new Error('The site is missing its built launcher assets');
const brandAssets = publicBrandAssets(
    await readFile(new URL('../../scripts/port/brand-art.sha256', import.meta.url), 'utf8'),
);
for (const asset of brandAssets) {
    const bytes = await readFile(new URL(asset.path, root));
    if (createHash('sha256').update(bytes).digest('hex') !== asset.sha256)
        throw new Error('The published image differs from its generated source: ' + asset.path);
}
if (!html.includes(`<link rel="manifest" href="./${WEB_MANIFEST}"`))
    throw new Error('The site is missing its web app manifest link');
const webManifest = JSON.parse(await readFile(new URL(WEB_MANIFEST, root), 'utf8'));
if (
    !Array.isArray(webManifest.icons) ||
    !webManifest.icons.length ||
    webManifest.icons.some(
        (icon: { src?: unknown }) => !brandAssets.some((asset) => asset.path === icon.src),
    )
)
    throw new Error('The web app manifest names an icon the site does not publish');
const maybeSite = siteUrl(process.env.MIDAS_SITE_URL);
if (maybeSite)
    for (const tag of absoluteSiteTags(maybeSite))
        if (!html.includes(renderedSiteTag(tag)))
            throw new Error('The site is missing its absolute tag: ' + renderedSiteTag(tag));
if (!brandAssets.some((asset) => asset.path === SOCIAL_CARD))
    throw new Error('The site is missing its link-preview image');
const allowed = new Set([
    'index.html',
    'frontend-manifest.json',
    'runtime-manifest.json',
    'LICENSE.txt',
    'THIRD_PARTY_NOTICES.txt',
    WEB_MANIFEST,
    ...brandAssets.map((asset) => asset.path),
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
