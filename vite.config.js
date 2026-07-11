import { resolve } from 'node:path';
import { defineConfig } from 'vite';

const cleanRoutes = new Map([
    ['/guide', '/guide.html'],
    ['/about', '/about.html'],
    ['/support', '/support.html'],
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

export default defineConfig({
    plugins: [cleanUrlPlugin()],
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
