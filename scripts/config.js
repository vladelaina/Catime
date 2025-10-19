/**
 * Catime 全局配置
 * 包含应用的基本配置信息
 */

// 全局配置对象
window.CATIME_CONFIG = {
    // 下载链接配置
    DOWNLOAD_URL: 'https://github.com/vladelaina/Catime/releases/download/v1.3.1/catime_1.3.1.exe',
    
    // GitHub 仓库配置
    GITHUB_URL: 'https://github.com/vladelaina/Catime',
    
    // 版本信息
    VERSION: '1.0.0',
    
    // 作者信息
    AUTHOR: {
        name: 'vladelaina',
        website: 'https://vladelaina.com/',
        github: 'https://github.com/vladelaina'
    },
    
    // 外部链接配置
    LINKS: {
        feedback: 'https://message.bilibili.com/#/whisper/mid1862395225',
        privacy: 'https://github.com/vladelaina/Catime/blob/main/PRIVACY.md',
        license: 'https://github.com/vladelaina/Catime/blob/main/LICENSE',
        artist: 'https://space.bilibili.com/26087398'
    },
    
    // 应用信息
    APP_INFO: {
        name: 'Catime',
        description: '一款为 Windows 设计的极简、现代、高效的透明计时器与番茄钟',
        keywords: ['计时器', '番茄钟', 'Windows', '透明', '悬浮'],
        license: 'Apache 2.0'
    }
};

// 输出配置加载成功信息
console.log('✅ Catime 全局配置已加载', window.CATIME_CONFIG);
