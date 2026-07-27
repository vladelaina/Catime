import { resolve } from 'node:path';
import { cp, readFile } from 'node:fs/promises';
import { defineConfig } from 'vite';

const cleanRoutes = new Map([
    ['/guide', '/guide.html'],
    ['/about', '/about.html'],
    ['/support', '/support.html'],
    ['/tray', '/tray/index.html'],
    ['/tray/', '/tray/index.html'],
]);

const legacyRoutes = new Map([
    ['/tray-animations', '/tray'],
    ['/tray-animations/', '/tray'],
]);

const globalStyleModules = [
    'base.css',
    'home-intro/hero-core.css',
    'home-intro/hero-atmosphere.css',
    'home-intro/features.css',
    'home-intro/github-stats.css',
    'home-intro/use-cases.css',
    'home-intro/cta-footer.css',
    'blog/layout.css',
    'blog/text-effects.css',
    'blog/scenario-titles.css',
    'navigation-and-hero.css',
    'home-content/features.css',
    'home-content/github-stats.css',
    'home-content/use-cases.css',
    'home-content/closing.css',
    'home-effects/blog.css',
    'home-effects/text.css',
    'home-effects/scenes.css',
    'home-effects/media.css',
    'guide/wiki.css',
    'guide/progress.css',
    'guide/content.css',
    'guide/effects.css',
    'guide/accents.css',
    'community.css',
    'finale/editorial.css',
    'finale/modern-layout.css',
    'finale/narrative.css',
    'site-chrome.css',
];

function globalStylesPlugin() {
    const entry = resolve(import.meta.dirname, 'styles/style.css');
    const modulesDirectory = resolve(import.meta.dirname, 'styles/modules');

    return {
        name: 'catime-global-styles',
        enforce: 'pre',
        async load(id) {
            if (id.split('?')[0] !== entry) return null;
            const modules = await Promise.all(
                globalStyleModules.map((file) => readFile(resolve(modulesDirectory, file), 'utf8')),
            );
            return modules.join('');
        },
    };
}

function cleanUrlPlugin() {
    const rewriteCleanUrl = (request, response, next) => {
        const url = new URL(request.url, 'http://localhost');
        const redirect = legacyRoutes.get(url.pathname);

        if (redirect) {
            response.statusCode = 308;
            response.setHeader('Location', `${redirect}${url.search}`);
            response.end();
            return;
        }

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
                    resolve(import.meta.dirname, 'tools/font-tool/scripts'),
                    resolve(import.meta.dirname, 'dist/tools/font-tool/scripts'),
                    { recursive: true },
                ),
            ]);
        },
    };
}

export default defineConfig({
    plugins: [globalStylesPlugin(), cleanUrlPlugin(), copyClassicScriptsPlugin()],
    build: {
        rollupOptions: {
            input: {
                index: resolve(import.meta.dirname, 'index.html'),
                guide: resolve(import.meta.dirname, 'guide.html'),
                about: resolve(import.meta.dirname, 'about.html'),
                support: resolve(import.meta.dirname, 'support.html'),
                tray: resolve(import.meta.dirname, 'tray/index.html'),
                fontTool: resolve(import.meta.dirname, 'tools/font-tool/index.html'),
            },
        },
    },
});
