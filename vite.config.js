import { resolve } from 'node:path';
import { cp } from 'node:fs/promises';
import { defineConfig } from 'vite';

const cleanRoutes = new Map([
    ['/guide', '/guide.html'],
    ['/about', '/about.html'],
    ['/support', '/support.html'],
    ['/tray-animations', '/tray-animations/index.html'],
    ['/tray-animations/', '/tray-animations/index.html'],
]);

function cleanUrlPlugin() {
    const rewriteCleanUrl = (request, _response, next) => {
        const url = new URL(request.url, 'http://localhost');
        const target = cleanRoutes.get(url.pathname);

        if (target) {
            request.url = `${target}${url.search}`;
        }

        next();
    };

    return {
        name: 'catime-clean-urls',
        configureServer(server) {
            server.middlewares.use(rewriteCleanUrl);
        },
        configurePreviewServer(server) {
            server.middlewares.use(rewriteCleanUrl);
        },
    };
}

function copyClassicScriptsPlugin() {
    return {
        name: 'catime-copy-classic-scripts',
        apply: 'build',
        async closeBundle() {
            await Promise.all([
                cp(resolve(import.meta.dirname, 'scripts'), resolve(import.meta.dirname, 'dist/scripts'), { recursive: true }),
                cp(resolve(import.meta.dirname, 'components'), resolve(import.meta.dirname, 'dist/components'), { recursive: true }),
                cp(resolve(import.meta.dirname, 'assets/catime.webp'), resolve(import.meta.dirname, 'dist/assets/catime.webp')),
                cp(
                    resolve(import.meta.dirname, 'tools/font-tool/script.js'),
                    resolve(import.meta.dirname, 'dist/tools/font-tool/script.js'),
                ),
            ]);
        },
    };
}

export default defineConfig({
    plugins: [cleanUrlPlugin(), copyClassicScriptsPlugin()],
    build: {
        rollupOptions: {
            input: {
                index: resolve(import.meta.dirname, 'index.html'),
                guide: resolve(import.meta.dirname, 'guide.html'),
                about: resolve(import.meta.dirname, 'about.html'),
                support: resolve(import.meta.dirname, 'support.html'),
                trayAnimations: resolve(import.meta.dirname, 'tray-animations/index.html'),
                fontTool: resolve(import.meta.dirname, 'tools/font-tool/index.html'),
            },
        },
    },
});
