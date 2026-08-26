const CATIME_GITHUB_URL = 'https://github.com/vladelaina/Catime';
const CATIME_DOWNLOAD_MANIFEST_URL = new URL('../downloads/releases.json', document.currentScript.src).href;

const catimeConfig = {
    // The manifest is generated from the installer filenames. Keep no version here,
    // so replacing downloads/catime_<version>.exe does not require another edit.
    DOWNLOAD_FILE: null,
    DOWNLOAD_VERSION: null,
    DOWNLOAD_URL: null,
    DOWNLOAD_MANIFEST_URL: CATIME_DOWNLOAD_MANIFEST_URL,
    GITHUB_DOWNLOAD_URL: `${CATIME_GITHUB_URL}/releases`,

    GITHUB_URL: CATIME_GITHUB_URL,
    GITHUB_RELEASES_URL: `${CATIME_GITHUB_URL}/releases`,

    VERSION: null,
    
    AUTHOR: {
        name: 'vladelaina',
        website: 'https://vladelaina.com/',
        github: 'https://github.com/vladelaina'
    },
    
    LINKS: {
        feedback: 'https://message.bilibili.com/#/whisper/mid1862395225',
        privacy: 'https://github.com/vladelaina/Catime/blob/main/PRIVACY.md',
        license: 'https://github.com/vladelaina/Catime/blob/main/LICENSE',
        artist: 'https://space.bilibili.com/26087398'
    },
    
    APP_INFO: {
        name: 'Catime',
        description: '一款为 Windows 设计的极简、现代、高效的透明计时器与番茄钟',
        keywords: ['计时器', '番茄钟', 'Windows', '透明', '悬浮'],
        license: 'Apache 2.0'
    }
};

window.CATIME_CONFIG = catimeConfig;

window.CATIME_CONFIG_READY = (async () => {
    if (window.location.protocol === 'file:' || typeof fetch !== 'function') return null;

    const controller = typeof AbortController === 'function' ? new AbortController() : null;
    const timeoutId = controller ? setTimeout(() => controller.abort(), 3000) : null;

    try {
        const response = await fetch(CATIME_DOWNLOAD_MANIFEST_URL, {
            cache: 'no-store',
            signal: controller?.signal,
        });
        if (!response.ok) return null;

        const manifest = await response.json();
        const latest = manifest?.latest || (Array.isArray(manifest?.files) ? manifest.files[0] : null);
        if (!latest?.file) return manifest;

        const installerUrl = new URL(latest.url || latest.file, CATIME_DOWNLOAD_MANIFEST_URL).href;
        const version = latest.version || null;

        Object.assign(catimeConfig, {
            DOWNLOAD_FILE: latest.file,
            DOWNLOAD_VERSION: version,
            DOWNLOAD_URL: installerUrl,
            GITHUB_DOWNLOAD_URL: version
                ? `${CATIME_GITHUB_URL}/releases/download/v${version}/${latest.file}`
                : `${CATIME_GITHUB_URL}/releases`,
            VERSION: version,
        });

        return manifest;
    } catch {
        return null;
    } finally {
        if (timeoutId) clearTimeout(timeoutId);
    }
})();

catimeConfig.DOWNLOAD_MANIFEST_PROMISE = window.CATIME_CONFIG_READY;

console.log('✅ Catime 全局配置已加载', window.CATIME_CONFIG);
