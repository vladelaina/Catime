(function() {
    const translations = {
        en: {
            'exe-label': 'EXE',
            'store-title': 'Microsoft Store',
            'store-description': 'Install with automatic updates managed by Windows.',
            'store-accessible-label': 'Get Catime from Microsoft Store',
            'package-title': 'Package managers',
            copy: 'Copy',
            copied: 'Copied',
            'page-title': 'Download Catime for Windows',
            'page-description': 'Download Catime for Windows from the direct installer, Microsoft Store, Chocolatey, Scoop, or Winget.',
            loading: 'Loading...',
            unavailable: 'See releases',
        },
        zh: {
            'exe-label': 'EXE',
            'store-title': 'Microsoft Store',
            'store-description': '交由 Windows 安装，并自动管理后续更新。',
            'store-accessible-label': '从 Microsoft Store 获取 Catime',
            'package-title': '包管理器',
            copy: '复制',
            copied: '已复制',
            'page-title': '下载 Catime Windows 版',
            'page-description': '通过独立安装包、Microsoft Store、Chocolatey、Scoop 或 Winget 下载 Catime Windows 版。',
            loading: '加载中...',
            unavailable: '查看发行版',
        },
    };

    const language = window.CatimeLocale?.getLanguage() === 'zh' ? 'zh' : 'en';
    const copy = translations[language];

    function applyTranslations() {
        document.querySelectorAll('[data-download-copy]').forEach(element => {
            const value = copy[element.dataset.downloadCopy];
            if (value) element.textContent = value;
        });

        document.title = copy['page-title'];
        const description = document.getElementById('meta-description');
        if (description) description.content = copy['page-description'];

        const version = document.getElementById('download-version');
        if (version) version.textContent = copy.loading;

        const storeBadge = document.getElementById('microsoft-store-badge');
        const storeLink = document.getElementById('microsoft-store-link');
        if (storeBadge) {
            const badgeLocale = language === 'zh' ? 'zh-cn' : 'en-us';
            storeBadge.src = `https://get.microsoft.com/images/${badgeLocale}%20light.svg`;
            storeBadge.alt = copy['store-accessible-label'];
        }
        if (storeLink) {
            storeLink.setAttribute('aria-label', copy['store-accessible-label']);
        }

    }

    function formatBytes(bytes) {
        if (!Number.isFinite(bytes) || bytes <= 0) return '--';
        if (bytes < 1024 * 1024) return `${Math.round(bytes / 1024)} KB`;
        return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
    }

    async function updateReleaseDetails() {
        let manifest = null;
        try {
            manifest = await window.CATIME_CONFIG_READY;
        } catch (error) {
        }

        const latest = manifest?.latest || (Array.isArray(manifest?.files) ? manifest.files[0] : null);
        const config = window.CATIME_CONFIG || {};
        const version = latest?.version || config.DOWNLOAD_VERSION || config.VERSION;
        const filename = latest?.file || config.DOWNLOAD_FILE;

        document.getElementById('download-version').textContent = version ? `v${version}` : copy.unavailable;
        document.getElementById('download-size').textContent = formatBytes(latest?.size);
        document.getElementById('download-filename').textContent = filename || 'catime.exe';
    }

    async function writeClipboard(value) {
        if (navigator.clipboard && window.isSecureContext) {
            await navigator.clipboard.writeText(value);
            return;
        }

        const field = document.createElement('textarea');
        field.value = value;
        field.setAttribute('readonly', '');
        field.style.position = 'fixed';
        field.style.opacity = '0';
        document.body.appendChild(field);
        field.select();
        const copied = document.execCommand('copy');
        field.remove();
        if (!copied) throw new Error('Copy command failed');
    }

    function initCopyCommands() {
        document.querySelectorAll('.copy-command').forEach(button => {
            button.addEventListener('click', async () => {
                const label = button.querySelector('.copy-label');
                const icon = button.querySelector('.command-block__copy');
                try {
                    await writeClipboard(button.dataset.command || '');
                    button.classList.add('is-copied');
                    if (label) label.textContent = copy.copied;
                    if (icon) icon.className = 'fas fa-check command-block__copy';
                    window.setTimeout(() => {
                        button.classList.remove('is-copied');
                        if (label) label.textContent = copy.copy;
                        if (icon) icon.className = 'far fa-copy command-block__copy';
                    }, 1600);
                } catch (error) {
                    button.classList.remove('is-copied');
                }
            });
        });
    }

    function updatePlatformNote() {
        const isWindows = /Windows/i.test(navigator.userAgent) || /^Win/i.test(navigator.platform || '');
        const note = document.getElementById('download-platform-note');
        if (!isWindows || !note) return;
        note.textContent = copy.detected;
        note.hidden = false;
    }

    document.addEventListener('DOMContentLoaded', () => {
        applyTranslations();
        initCopyCommands();
        updatePlatformNote();
        updateReleaseDetails();
    });
})();
