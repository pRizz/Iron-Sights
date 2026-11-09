import { defineConfig } from 'vite';
import solid from 'vite-plugin-solid';
import { runtimeArtifacts } from './tools/runtime-artifacts.ts';
import { absoluteSiteTags, siteUrl } from './tools/site-meta.ts';

const maybeSite = siteUrl(process.env.MIDAS_SITE_URL);

export default defineConfig({
    base: './',
    plugins: [
        solid(),
        {
            name: 'iron-sights-site-meta',
            transformIndexHtml: () =>
                maybeSite
                    ? absoluteSiteTags(maybeSite).map((tag) => ({ ...tag, injectTo: 'head' as const }))
                    : [],
        },
        {
            name: 'iron-sights-local-runtime',
            configureServer(server) {
                server.middlewares.use(async (request, response, next) => {
                    const path = new URL(request.url || '/', 'http://localhost').pathname;
                    if (path !== '/runtime-manifest.json' && !path.startsWith('/runtime/'))
                        return next();
                    try {
                        const { manifest, files } = await runtimeArtifacts();
                        response.setHeader('Cache-Control', 'no-store');
                        if (path === '/runtime-manifest.json') {
                            response.setHeader('Content-Type', 'application/json');
                            response.end(JSON.stringify(manifest));
                            return;
                        }
                        const name = path.slice(('/' + manifest.base).length);
                        const maybeBytes = path.startsWith('/' + manifest.base)
                            ? files.get(name)
                            : undefined;
                        if (!maybeBytes) {
                            response.statusCode = 404;
                            response.end();
                            return;
                        }
                        response.setHeader(
                            'Content-Type',
                            name.endsWith('.wasm')
                                ? 'application/wasm'
                                : name.endsWith('.mjs')
                                  ? 'text/javascript'
                                  : 'text/plain',
                        );
                        response.end(maybeBytes);
                    } catch (error) {
                        server.config.logger.error(String(error));
                        response.statusCode = 503;
                        response.end('The local game build is unavailable.');
                    }
                });
            },
        },
    ],
    optimizeDeps: { exclude: ['mystic-ui'] },
    build: { outDir: '../build/web-site', emptyOutDir: true, manifest: 'frontend-manifest.json' },
});
