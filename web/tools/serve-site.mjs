// Local audit server with a fixed assembled-site root; never serve the checkout.
import { createServer } from 'node:http';
import { readFile, realpath, stat } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { isAbsolute, relative, resolve } from 'node:path';

const root = await realpath(fileURLToPath(new URL('../../build/web-site/', import.meta.url)));
const port = Number(process.argv[2] || 4176);
const prefix = process.argv[3] || '/';
if (
    !Number.isSafeInteger(port) ||
    port < 1024 ||
    port > 65535 ||
    !prefix.startsWith('/') ||
    !prefix.endsWith('/')
)
    throw new Error('Usage: serve-site.mjs [local-port] [/project-prefix/]');
const types = {
    '.mjs': 'text/javascript',
    '.js': 'text/javascript',
    '.css': 'text/css',
    '.wasm': 'application/wasm',
    '.json': 'application/json',
    '.html': 'text/html',
    '.txt': 'text/plain',
    '.avif': 'image/avif',
};
const server = createServer(async (request, response) => {
    try {
        if (!['GET', 'HEAD'].includes(request.method)) {
            response.writeHead(405).end();
            return;
        }
        const pathname = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
        if (!pathname.startsWith(prefix)) {
            response.writeHead(404).end();
            return;
        }
        const suffix = pathname.slice(prefix.length) || 'index.html';
        const path = await realpath(resolve(root, suffix));
        const within = relative(root, path);
        if (isAbsolute(within) || within.startsWith('..') || !(await stat(path)).isFile()) {
            response.writeHead(404).end();
            return;
        }
        const extension = path.slice(path.lastIndexOf('.'));
        response.writeHead(200, {
            'Content-Type': types[extension] || 'application/octet-stream',
            'Cache-Control': 'no-store',
        });
        response.end(request.method === 'HEAD' ? undefined : await readFile(path));
    } catch {
        response.writeHead(404).end();
    }
});
server.listen(port, '127.0.0.1', () =>
    console.log(`Audit site: http://127.0.0.1:${port}${prefix}`),
);
process.on('SIGTERM', () => server.close());
