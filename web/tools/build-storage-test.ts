import { mkdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const outdir = fileURLToPath(new URL('../../build/web-storage-test', import.meta.url));
await mkdir(outdir, { recursive: true });
const result = await Bun.build({
  entrypoints: [fileURLToPath(new URL('../tests/storage-driver.ts', import.meta.url))],
  target: 'browser', outdir,
});
if (!result.success) throw new AggregateError(result.logs, 'Storage test bundle failed');
