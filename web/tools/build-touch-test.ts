import { build } from 'vite';
import solid from 'vite-plugin-solid';
import { fileURLToPath } from 'node:url';
await build({
    configFile: false,
    plugins: [solid()],
    build: {
        outDir: '../build/web-touch-fixture',
        emptyOutDir: true,
        lib: {
            entry: fileURLToPath(new URL('../tests/touch-driver.tsx', import.meta.url)),
            formats: ['es'],
            fileName: 'driver',
            cssFileName: 'driver',
        },
    },
});
