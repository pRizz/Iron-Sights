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
console.log(`Assembled browser preview: ${fileURLToPath(output)} (runtime ${manifest.version})`);
