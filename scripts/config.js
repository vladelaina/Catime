const CATIME_DOWNLOAD_VERSION = '1.4.0';
const CATIME_DOWNLOAD_FILE = `catime_${CATIME_DOWNLOAD_VERSION}.exe`;
const CATIME_GITHUB_URL = 'https://github.com/vladelaina/Catime';

window.CATIME_CONFIG = {
    DOWNLOAD_FILE: CATIME_DOWNLOAD_FILE,
    DOWNLOAD_VERSION: CATIME_DOWNLOAD_VERSION,
    DOWNLOAD_URL: new URL(`../downloads/${CATIME_DOWNLOAD_FILE}`, document.currentScript.src).href,
    DOWNLOAD_MANIFEST_URL: new URL('../downloads/releases.json', document.currentScript.src).href,
    GITHUB_DOWNLOAD_URL: `${CATIME_GITHUB_URL}/releases/download/v${CATIME_DOWNLOAD_VERSION}/${CATIME_DOWNLOAD_FILE}`,

    GITHUB_URL: CATIME_GITHUB_URL,
    GITHUB_RELEASES_URL: `${CATIME_GITHUB_URL}/releases`,

    VERSION: CATIME_DOWNLOAD_VERSION,
    
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

console.log('✅ Catime 全局配置已加载', window.CATIME_CONFIG);
