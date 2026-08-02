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
const trayLibraryInteraction = `async () => {
    const row = document.querySelector('.artist-showcase');
    const toggle = row?.querySelector('.artist-identity');

    const previewRequestCount = () => new Set(performance.getEntriesByType('resource')
        .filter(entry => entry.name.includes('preview-'))
        .map(entry => entry.name)).size;
    const preloadDeadline = Date.now() + 5000;
    while (previewRequestCount() < 6 && Date.now() < preloadDeadline) {
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    const backgroundPreviewCount = previewRequestCount();

    const supportButton = document.querySelector('.main-header .nav-actions .support-btn');
    supportButton?.focus();
    const focusedSupportBackground = supportButton ? getComputedStyle(supportButton).backgroundImage : '';
    const supportStayedPink = focusedSupportBackground.includes('rgb(247, 125, 170)')
        && focusedSupportBackground.includes('rgb(247, 153, 184)');

    toggle?.click();
    await new Promise(resolve => setTimeout(resolve, 1500));

    const result = {
        expanded: row?.classList.contains('expanded') || false,
        cardCount: row?.querySelectorAll('.artist-details .animation-item').length || 0,
        animatedCardCount: [...(row?.querySelectorAll('.artist-details .animation-item img') || [])]
            .filter(image => image.currentSrc.includes('preview-')).length,
        hasLoadMore: Boolean(row?.querySelector('.load-more')),
        automaticPreviewRequests: previewRequestCount(),
        backgroundPreviewCount,
        supportStayedPink,
        focusedSupportBackground,
    };

    return {
        ok: result.expanded
            && result.cardCount === 6
            && result.animatedCardCount === 6
            && !result.hasLoadMore
            && result.automaticPreviewRequests >= 6
            && result.backgroundPreviewCount >= 6
            && result.supportStayedPink,
        ...result,
    };
}`;
const traySorterInteraction = `async () => {
    const source = await (await fetch('/assets/catime.webp')).arrayBuffer();
    const uniqueCanvas = document.createElement('canvas');
    uniqueCanvas.width = 32;
    uniqueCanvas.height = 32;
    const uniqueContext = uniqueCanvas.getContext('2d');
    uniqueContext.fillStyle = '#e43f52';
    uniqueContext.fillRect(0, 0, 32, 32);
    const uniqueBlob = await new Promise(resolve => uniqueCanvas.toBlob(resolve, 'image/png'));
    const files = [
        new File([source], 'third.png', { type: 'image/png' }),
        new File([source], 'first.webp', { type: 'image/webp' }),
        new File([source], 'second.jpg', { type: 'image/jpeg' }),
        new File([uniqueBlob], 'unique.png', { type: 'image/png' }),
    ];
    window.trayIconSorter.addFiles(files);
    await window.trayIconSorter.scanForDuplicates();

    const duplicateStates = Array.from(
        document.querySelectorAll('.image-card'),
        card => card.classList.contains('is-duplicate'),
    );
    const duplicateCount = document.getElementById('duplicateCount').textContent;

    const names = () => Array.from(document.querySelectorAll('[data-role="output-name"]'), node => node.textContent);
    const before = names();
    let dragSurfaces = document.querySelectorAll('[data-role="drag-surface"]');
    const usesCustomPointerDrag = Array.from(dragSurfaces).every(surface => !surface.draggable)
        && Array.from(document.querySelectorAll('.card-preview img')).every(image => !image.draggable);
    const scrollCalls = [];
    const originalScrollBy = window.scrollBy;
    window.scrollBy = (x, y) => scrollCalls.push([x, y]);

    const startPointerDrag = (index, pointerId) => {
        const surface = document.querySelectorAll('[data-role="drag-surface"]')[index];
        const rect = surface.getBoundingClientRect();
        surface.dispatchEvent(new PointerEvent('pointerdown', {
            bubbles: true,
            cancelable: true,
            button: 0,
            buttons: 1,
            pointerId,
            pointerType: 'mouse',
            clientX: rect.left + (rect.width / 2),
            clientY: rect.top + (rect.height / 2),
        }));
        return { surface, rect };
    };

    let { surface: pointerSurface, rect: pointerRect } = startPointerDrag(0, 71);
    const initialGhostTransform = document.querySelector('.drag-ghost')?.style.transform;
    const ghostHasNoTransition = getComputedStyle(document.querySelector('.drag-ghost')).transitionDuration === '0s';
    pointerSurface.dispatchEvent(new PointerEvent('pointermove', {
        bubbles: true,
        cancelable: true,
        button: 0,
        buttons: 1,
        pointerId: 71,
        pointerType: 'mouse',
        clientX: pointerRect.left + (pointerRect.width / 2) + 30,
        clientY: pointerRect.top + (pointerRect.height / 2) + 20,
    }));
    const movedGhostTransform = document.querySelector('.drag-ghost')?.style.transform;
    const ghostFollowsPointer = Boolean(initialGhostTransform)
        && initialGhostTransform !== movedGhostTransform;
    window.dispatchEvent(new WheelEvent('wheel', { deltaY: 120, bubbles: true, cancelable: true }));
    window.dispatchEvent(new PointerEvent('pointerup', {
        bubbles: true,
        cancelable: true,
        button: 0,
        pointerId: 71,
        pointerType: 'mouse',
        clientX: pointerRect.left + (pointerRect.width / 2),
        clientY: pointerRect.top + (pointerRect.height / 2),
    }));
    const ghostRemovedAfterPointerUp = !document.querySelector('.drag-ghost');

    ({ surface: pointerSurface, rect: pointerRect } = startPointerDrag(0, 72));
    const secondSurface = document.querySelectorAll('[data-role="drag-surface"]')[1];
    const secondRect = secondSurface.getBoundingClientRect();
    secondSurface.dispatchEvent(new PointerEvent('pointerdown', {
        bubbles: true,
        cancelable: true,
        button: 0,
        buttons: 1,
        pointerId: 73,
        pointerType: 'touch',
        clientX: secondRect.left + (secondRect.width / 2),
        clientY: secondRect.top + (secondRect.height / 2),
    }));
    const ignoresSecondPointer = document.querySelectorAll('.drag-ghost').length === 1
        && window.trayIconSorter.touchDrag?.pointerId === 72;
    window.dispatchEvent(new PointerEvent('pointermove', {
        bubbles: true,
        cancelable: true,
        button: -1,
        buttons: 0,
        pointerId: 72,
        pointerType: 'mouse',
        clientX: pointerRect.left + (pointerRect.width / 2),
        clientY: pointerRect.top + (pointerRect.height / 2),
    }));
    const ghostRemovedAfterLostButton = !document.querySelector('.drag-ghost');

    startPointerDrag(0, 74);
    window.dispatchEvent(new Event('blur'));
    const ghostRemovedAfterBlur = !document.querySelector('.drag-ghost')
        && !document.body.classList.contains('is-touch-sorting');

    startPointerDrag(0, 75);
    window.dispatchEvent(new PointerEvent('pointercancel', {
        bubbles: true,
        cancelable: true,
        pointerId: 75,
        pointerType: 'mouse',
    }));
    const ghostRemovedAfterCancel = !document.querySelector('.drag-ghost');

    ({ surface: pointerSurface, rect: pointerRect } = startPointerDrag(0, 76));
    await window.trayIconSorter.scanForDuplicates();
    const dragSurvivesDuplicateRefresh = pointerSurface.isConnected
        && Boolean(document.querySelector('.drag-ghost'));
    const targetRect = document.querySelectorAll('[data-role="drag-surface"]')[1].getBoundingClientRect();
    window.dispatchEvent(new PointerEvent('pointermove', {
        bubbles: true,
        cancelable: true,
        button: -1,
        buttons: 1,
        pointerId: 76,
        pointerType: 'mouse',
        clientX: targetRect.right - 2,
        clientY: targetRect.top + (targetRect.height / 2),
    }));
    window.dispatchEvent(new PointerEvent('pointerup', {
        bubbles: true,
        cancelable: true,
        button: 0,
        buttons: 0,
        pointerId: 76,
        pointerType: 'mouse',
        clientX: targetRect.right - 2,
        clientY: targetRect.top + (targetRect.height / 2),
    }));

    const fileTransfer = new DataTransfer();
    fileTransfer.items.add(new File([source], 'overlay.webp', { type: 'image/webp' }));
    document.dispatchEvent(new DragEvent('dragenter', {
        bubbles: true,
        cancelable: true,
        dataTransfer: fileTransfer,
    }));
    const overlayOpened = !document.getElementById('fileDragOverlay').hidden;
    window.dispatchEvent(new Event('blur'));
    const overlayClearedAfterBlur = overlayOpened && document.getElementById('fileDragOverlay').hidden;

    window.scrollBy = originalScrollBy;
    const after = names();

    let download = null;
    window.trayIconSorter.triggerDownload = (blob, filename) => {
        download = { blob, filename };
    };
    await window.downloadSortedImages();

    const zipBytes = new Uint8Array(await download.blob.arrayBuffer());
    const zipText = new TextDecoder().decode(zipBytes);
    const signature = new DataView(zipBytes.buffer).getUint32(0, true);
    const result = {
        before,
        after,
        cardCount: document.querySelectorAll('.image-card').length,
        filename: download.filename,
        zipSignature: signature,
        containsExpectedNames: ['1.webp', '2.png', '3.jpg', '4.png'].every(name => zipText.includes(name)),
        duplicateStates,
        duplicateCount,
        dragWheelScrolled: scrollCalls.some(([, y]) => y > 120),
        usesCustomPointerDrag,
        ghostFollowsPointer,
        ghostHasNoTransition,
        ghostRemovedAfterPointerUp,
        ignoresSecondPointer,
        ghostRemovedAfterLostButton,
        ghostRemovedAfterBlur,
        ghostRemovedAfterCancel,
        dragSurvivesDuplicateRefresh,
        overlayClearedAfterBlur,
    };
    window.clearImages();

    return {
        ok: JSON.stringify(before) === JSON.stringify(['1.png', '2.webp', '3.jpg', '4.png'])
            && JSON.stringify(after) === JSON.stringify(['1.webp', '2.png', '3.jpg', '4.png'])
            && result.cardCount === 4
            && /^catime-tray-icons-\\d{8}-\\d{6}\\.zip$/.test(result.filename)
            && result.zipSignature === 0x04034b50
            && result.containsExpectedNames
            && JSON.stringify(result.duplicateStates) === JSON.stringify([true, true, true, false])
            && result.duplicateCount === '3'
            && result.dragWheelScrolled
            && result.usesCustomPointerDrag
            && result.ghostFollowsPointer
            && result.ghostHasNoTransition
            && result.ghostRemovedAfterPointerUp
            && result.ignoresSecondPointer
            && result.ghostRemovedAfterLostButton
            && result.ghostRemovedAfterBlur
            && result.ghostRemovedAfterCancel
            && result.dragSurvivesDuplicateRefresh
            && result.overlayClearedAfterBlur,
        ...result,
    };
}`;
const pages = [
    { path: '/', selector: 'main', minimum: 1 },
    { path: '/about', selector: 'main', minimum: 1 },
    { path: '/support', selector: '.support-project', minimum: 1 },
    { path: '/guide', selector: 'main', minimum: 1 },
    {
        path: '/tray',
        selector: '.artist-showcase',
        minimum: 1,
        mockTrayManifest: true,
        verifySupportHover: true,
        interaction: trayLibraryInteraction,
    },
    {
        path: '/tools/font-tool/',
        selector: '#uploadArea',
        minimum: 1,
        globals: ['clearFiles', 'setCharacters', 'startProcessing', 'downloadAllFonts'],
    },
    {
        path: '/tools/tray-icon-sorter/',
        selector: '#uploadZone',
        minimum: 1,
        globals: ['clearImages', 'downloadSortedImages'],
        verifySupportHover: true,
        interaction: traySorterInteraction,
    },
];
const selectedPages = process.env.VERIFY_PAGE
    ? pages.filter(page => page.path === process.env.VERIFY_PAGE)
    : pages;

if (selectedPages.length === 0) {
    throw new Error(`Unknown VERIFY_PAGE: ${process.env.VERIFY_PAGE}`);
}

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
        client.send('DOM.enable'),
        client.send('CSS.enable'),
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

    if (page.verifySupportHover) {
        const { root } = await client.send('DOM.getDocument');
        const { nodeId } = await client.send('DOM.querySelector', {
            nodeId: root.nodeId,
            selector: '.main-header .nav-actions .support-btn',
        });
        if (!nodeId) throw new Error(`${page.path}: missing header support button`);
        if (viewport.mobile) {
            await client.send('CSS.forcePseudoState', { nodeId, forcedPseudoClasses: ['hover'] });
        } else {
            const { model } = await client.send('DOM.getBoxModel', { nodeId });
            const [x1, y1, x2, y2, , , x4, y4] = model.border;
            await client.send('Input.dispatchMouseEvent', {
                type: 'mouseMoved',
                x: (x1 + x2 + x4) / 3,
                y: (y1 + y2 + y4) / 3,
            });
            await new Promise(resolveHover => setTimeout(resolveHover, 400));
        }
        const hoverEvaluation = await client.send('Runtime.evaluate', {
            expression: `(() => {
                const style = getComputedStyle(document.querySelector('.main-header .nav-actions .support-btn'));
                return { backgroundColor: style.backgroundColor, backgroundImage: style.backgroundImage };
            })()`,
            returnByValue: true,
        });
        const hoverStyle = hoverEvaluation.result.value;
        const hoverPaint = `${hoverStyle.backgroundColor} ${hoverStyle.backgroundImage}`;
        if (!hoverPaint.includes('rgb(247, 125, 170)') || hoverPaint.includes('rgb(122, 162, 247)')) {
            throw new Error(`${page.path}: support hover is not pink: ${hoverPaint}`);
        }
        if (viewport.mobile) {
            await client.send('CSS.forcePseudoState', { nodeId, forcedPseudoClasses: [] });
        }
    }

    if (page.interaction) {
        const interactionEvaluation = await client.send('Runtime.evaluate', {
            expression: `(${page.interaction})()`,
            awaitPromise: true,
            returnByValue: true,
        });
        if (interactionEvaluation.exceptionDetails) {
            const message = interactionEvaluation.exceptionDetails.exception?.description
                || interactionEvaluation.exceptionDetails.text;
            throw new Error(`${page.path}: interaction failed: ${message}`);
        }
        if (!interactionEvaluation.result.value?.ok) {
            throw new Error(`${page.path}: interaction assertions failed: ${JSON.stringify(interactionEvaluation.result.value)}`);
        }
    }

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
    const files = Array(6).fill('catime.webp');
    const fileVersions = Array.from({ length: files.length }, (_, index) => `source-${index + 1}`);
    const posterVersions = Array.from({ length: files.length }, (_, index) => `poster-${index + 1}`);
    const previewVersions = Array.from({ length: files.length }, (_, index) => `preview-${index + 1}`);
    const manifest = {
        version: 'verify',
        generated: '2026-07-29T00:00:00.000Z',
        sections: {
            verification: {
                count: files.length,
                authorAvatar: `${assetUrl}?v=verify`,
                authorLinks: [],
                repository: 'https://github.com/catime-labs/tray-hub',
                cdnBase: `${baseUrl}/assets/`,
                files,
                fileVersions,
                posterCdnBase: `${baseUrl}/assets/`,
                posterFiles: files,
                posterVersions,
                previewCdnBase: `${baseUrl}/assets/`,
                previewFiles: files,
                previewVersions,
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
        for (const page of selectedPages) await verifyPage(debugPort, baseUrl, page, viewport);
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
