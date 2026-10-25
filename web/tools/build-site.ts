import { mkdir, writeFile, copyFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { runtimeArtifacts } from './runtime-artifacts.ts';

const { files, manifest } = await runtimeArtifacts();
const result = Bun.spawn([process.execPath, '--bun', 'vite', 'build'], {
    stdout: 'inherit',
    stderr: 'inherit',
});
if (await result.exited) throw new Error('Frontend build failed');
const output = new URL('../../build/web-site/', import.meta.url);
for (const [name, bytes] of files) {
    const target = new URL(manifest.base + name, output);
    await mkdir(fileURLToPath(new URL('.', target)), { recursive: true });
    await writeFile(target, bytes);
}
await writeFile(new URL('runtime-manifest.json', output), JSON.stringify(manifest, null, 2) + '\n');
await copyFile(new URL('../../LICENSE', import.meta.url), new URL('LICENSE.txt', output));
const notices = [
    ['Port dependencies', '../../tools/THIRD_PARTY_NOTICES.txt'],
    ['Enhanced fonts: license', '../../port/fast3d/fonts/LICENSE.txt'],
    ['Enhanced fonts: notice', '../../port/fast3d/fonts/NOTICE.txt'],
    ['Emscripten / C / C++ / SDL2 runtime', '../../build/web/video/runtime-notices.txt'],
    ['SolidJS', '../node_modules/solid-js/LICENSE'],
    ['MysticUI', '../node_modules/mystic-ui/LICENSE.md'],
    ['clsx', '../node_modules/clsx/license'],
    ['tailwind-merge', '../node_modules/tailwind-merge/LICENSE.md'],
];
let text = '';
for (const [name, path] of notices)
    text += `${name}\n${await Bun.file(new URL(path, import.meta.url)).text()}\n\n`;
await writeFile(new URL('THIRD_PARTY_NOTICES.txt', output), text);
console.log(`Assembled browser preview: ${fileURLToPath(output)} (runtime ${manifest.version})`);
