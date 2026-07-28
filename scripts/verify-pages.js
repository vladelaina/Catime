import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';
import { spawn } from 'node:child_process';
import net from 'node:net';

const ROOT = resolve(import.meta.dirname, '..');
const CHROMIUM = process.env.CHROMIUM_BIN || 'chromium';
const viewports = [
    { name: 'desktop', width: 1440, height: 900, deviceScaleFactor: 1, mobile: false },
    { name: 'mobile', width: 390, height: 844, deviceScaleFactor: 1, mobile: true },
];
const pages = [
    { path: '/', selector: 'main', minimum: 1 },
    { path: '/about', selector: 'main', minimum: 1 },
    { path: '/support', selector: '.support-project', minimum: 1 },
    { path: '/guide', selector: 'main', minimum: 1 },
    { path: '/tray', selector: '.artist-showcase', minimum: 1, mockTrayManifest: true },
    {
        path: '/tools/font-tool/',
        selector: '#uploadArea',
        minimum: 1,
        globals: ['clearFiles', 'setCharacters', 'startProcessing', 'downloadAllFonts'],
    },
];

function getAvailablePort() {
    return new Promise((resolvePort, reject) => {
        const server = net.createServer();
        server.unref();
        server.on('error', reject);
        server.listen(0, '127.0.0.1', () => {
            const { port } = server.address();
            server.close(() => resolvePort(port));
        });
    });
}

function stopChild(child) {
    if (child.exitCode !== null || child.signalCode) return Promise.resolve();
    return new Promise((resolveStop) => {
        const forceStop = setTimeout(() => child.kill('SIGKILL'), 1000);
        child.once('exit', () => {
            clearTimeout(forceStop);
            resolveStop();
        });
        child.kill('SIGTERM');
    });
}

async function waitFor(check, label, timeout = 10000) {
    const started = Date.now();
    while (Date.now() - started < timeout) {
        try {
            const value = await check();
            if (value) return value;
        } catch {
            // The server may not have started listening yet.
        }
        await new Promise((resolveWait) => setTimeout(resolveWait, 100));
    }
    throw new Error(`Timed out waiting for ${label}`);
}

function createCdpClient(url) {
    const socket = new WebSocket(url);
    const pending = new Map();
    const listeners = new Map();
    let sequence = 0;

    socket.addEventListener('message', ({ data }) => {
        const message = JSON.parse(data);
        if (message.id) {
            const request = pending.get(message.id);
            pending.delete(message.id);
            if (message.error) request.reject(new Error(message.error.message));
            else request.resolve(message.result);
            return;
        }

        for (const listener of listeners.get(message.method) || []) {
            listener(message.params);
        }
    });

    return {
        ready: new Promise((resolveReady, reject) => {
            socket.addEventListener('open', resolveReady, { once: true });
            socket.addEventListener('error', reject, { once: true });
        }),
        on(method, listener) {
            const methodListeners = listeners.get(method) || [];
            methodListeners.push(listener);
            listeners.set(method, methodListeners);
        },
        send(method, params = {}) {
            const id = ++sequence;
            return new Promise((resolveRequest, reject) => {
                pending.set(id, { resolve: resolveRequest, reject });
                socket.send(JSON.stringify({ id, method, params }));
            });
        },
        close() {
            socket.close();
        },
    };
}

async function verifyPage(debugPort, baseUrl, page, viewport) {
    const targetResponse = await fetch(
        `http://127.0.0.1:${debugPort}/json/new?${encodeURIComponent('about:blank')}`,
        { method: 'PUT', signal: AbortSignal.timeout(10000) },
    );
    const target = await targetResponse.json();
    const client = createCdpClient(target.webSocketDebuggerUrl);
    const exceptions = [];

    await client.ready;
    client.on('Runtime.exceptionThrown', ({ exceptionDetails }) => {
        exceptions.push(exceptionDetails.exception?.description || exceptionDetails.text);
    });
    await Promise.all([
        client.send('Page.enable'),
        client.send('Runtime.enable'),
        client.send('Emulation.setDeviceMetricsOverride', {
            width: viewport.width,
            height: viewport.height,
            deviceScaleFactor: viewport.deviceScaleFactor,
            mobile: viewport.mobile,
        }),
    ]);
    if (page.mockTrayManifest) await enableTrayManifestMock(client, baseUrl, exceptions);

    const loaded = new Promise((resolveLoaded) => client.on('Page.loadEventFired', resolveLoaded));
    await client.send('Page.navigate', { url: `${baseUrl}${page.path}` });
    await Promise.race([
        loaded,
        new Promise((_, reject) => setTimeout(() => reject(new Error(`Load timeout: ${page.path}`)), 12000)),
    ]);
    await new Promise((resolveWait) => setTimeout(resolveWait, 500));

    const expression = `(() => ({
        readyState: document.readyState,
        matches: document.querySelectorAll(${JSON.stringify(page.selector)}).length,
        missingGlobals: ${JSON.stringify(page.globals || [])}.filter(name => typeof window[name] !== 'function')
    }))()`;
    const evaluation = await client.send('Runtime.evaluate', { expression, returnByValue: true });
    const result = evaluation.result.value;
    client.close();

    if (exceptions.length) throw new Error(`${page.path}: ${exceptions.join(' | ')}`);
    if (result.readyState !== 'complete') throw new Error(`${page.path}: document is ${result.readyState}`);
    if (result.matches < page.minimum) throw new Error(`${page.path}: missing ${page.selector}`);
    if (result.missingGlobals.length) {
        throw new Error(`${page.path}: missing globals ${result.missingGlobals.join(', ')}`);
    }
    process.stdout.write(`PASS ${page.path} [${viewport.name}]\n`);
}

async function enableTrayManifestMock(client, baseUrl, exceptions) {
    const assetUrl = `${baseUrl}/assets/catime.webp`;
    const manifest = {
        version: 'verify',
        generated: '2026-07-29T00:00:00.000Z',
        sections: {
            verification: {
                count: 1,
                authorAvatar: `${assetUrl}?v=verify`,
                authorLinks: [],
                repository: 'https://github.com/catime-labs/tray-hub',
                cdnBase: `${baseUrl}/assets/`,
                files: ['catime.webp'],
                fileVersions: ['verify'],
                posterCdnBase: `${baseUrl}/assets/`,
                posterFiles: ['catime.webp'],
                posterVersions: ['verify'],
                previewCdnBase: `${baseUrl}/assets/`,
                previewFiles: ['catime.webp'],
                previewVersions: ['verify'],
            },
        },
    };
    const body = Buffer.from(JSON.stringify(manifest)).toString('base64');

    client.on('Fetch.requestPaused', ({ requestId }) => {
        client.send('Fetch.fulfillRequest', {
            requestId,
            responseCode: 200,
            responseHeaders: [
                { name: 'Content-Type', value: 'application/json; charset=utf-8' },
                { name: 'Access-Control-Allow-Origin', value: '*' },
                { name: 'Cache-Control', value: 'no-store' },
            ],
            body,
        }).catch(error => exceptions.push(`Unable to fulfill tray manifest: ${error.message}`));
    });
    await client.send('Fetch.enable', {
        patterns: [{ urlPattern: 'https://tray.cati.me/sections.json*', requestStage: 'Request' }],
    });
}

const profileDirectory = await mkdtemp(join(tmpdir(), 'catime-chromium-'));
const [serverPort, debugPort] = await Promise.all([getAvailablePort(), getAvailablePort()]);
const preview = spawn(
    resolve(ROOT, 'node_modules/.bin/vite'),
    ['preview', '--host', '127.0.0.1', '--port', String(serverPort), '--strictPort'],
    { cwd: ROOT, stdio: 'ignore' },
);
const chromium = spawn(CHROMIUM, [
    '--headless=new',
    '--no-sandbox',
    '--disable-gpu',
    '--hide-scrollbars',
    `--remote-debugging-port=${debugPort}`,
    `--user-data-dir=${profileDirectory}`,
    'about:blank',
], { stdio: 'ignore' });

try {
    const baseUrl = `http://127.0.0.1:${serverPort}`;
    await Promise.all([
        waitFor(async () => (await fetch(baseUrl, { signal: AbortSignal.timeout(2000) })).ok, 'preview server'),
        waitFor(
            async () => (
                await fetch(`http://127.0.0.1:${debugPort}/json/version`, { signal: AbortSignal.timeout(2000) })
            ).ok,
            'Chromium',
        ),
    ]);
    for (const viewport of viewports) {
        for (const page of pages) await verifyPage(debugPort, baseUrl, page, viewport);
    }
} finally {
    await Promise.all([stopChild(preview), stopChild(chromium)]);
    await rm(profileDirectory, {
        recursive: true,
        force: true,
        maxRetries: 5,
        retryDelay: 100,
    });
}

// Chromium's DevTools HTTP connection may keep Node's fetch pool alive.
process.exit(0);
