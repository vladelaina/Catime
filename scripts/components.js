class CatimeLocaleController {
    constructor() {
        this.languages = [
            { code: 'zh', label: '简体中文', htmlLang: 'zh-CN' },
            { code: 'en', label: 'English', htmlLang: 'en' },
        ];
        this.language = this.resolveLanguage();
        this.documentClickHandler = null;
        this.documentKeyHandler = null;

        this.persistInitialLanguage();
        this.updateDocumentLanguage();
    }

    resolveLanguage() {
        try {
            const saved = localStorage.getItem('catime-language');
            if (saved === 'zh' || saved === 'en') return saved;
        } catch (error) {
        }

        const browserLanguage = (navigator.languages && navigator.languages[0]) || navigator.language || 'en';
        return /^zh\b/i.test(browserLanguage) ? 'zh' : 'en';
    }

    persistInitialLanguage() {
        try {
            if (!localStorage.getItem('catime-language')) {
                localStorage.setItem('catime-language', this.language);
            }
        } catch (error) {
        }
    }

    getLanguage() {
        return this.language;
    }

    isChinese() {
        return this.language === 'zh';
    }

    translate(english, chinese) {
        return this.isChinese() ? chinese : english;
    }

    updateDocumentLanguage() {
        const language = this.languages.find(option => option.code === this.language);
        document.documentElement.lang = language ? language.htmlLang : 'en';
    }

    setLanguage(language) {
        if (!this.languages.some(option => option.code === language)) return;

        try {
            localStorage.setItem('catime-language', language);
        } catch (error) {
        }

        this.language = language;
        this.updateDocumentLanguage();
        window.dispatchEvent(new CustomEvent('catime-language-change', {
            detail: { language },
        }));
        window.location.reload();
    }

    translateCommonComponents() {
        const copy = this.isChinese()
            ? {
                'project-prefix': '© 2025-2026 Catime 项目，由',
                'project-suffix': ' 开发',
                'license-prefix': '基于',
                'license-suffix': '协议开源',
                'artist-prefix': '图标画师：',
                feedback: '反馈',
                privacy: '隐私政策',
            }
            : {
                'project-prefix': '© 2025-2026 Catime Project by',
                'project-suffix': '',
                'license-prefix': 'Open sourced under',
                'license-suffix': 'License',
                'artist-prefix': 'Icon Artist:',
                feedback: 'Feedback',
                privacy: 'Privacy Policy',
            };

        document.querySelectorAll('[data-footer-copy]').forEach(element => {
            const key = element.dataset.footerCopy;
            if (Object.prototype.hasOwnProperty.call(copy, key)) {
                element.textContent = copy[key];
            }
        });

        const scrollTooltip = document.querySelector('.scroll-progress-tooltip');
        if (scrollTooltip) {
            scrollTooltip.textContent = this.translate('Back to Top', '返回顶部');
        }
    }

    initializeFooter() {
        const switcher = document.querySelector('[data-language-switcher]');
        if (!switcher || switcher.dataset.initialized === 'true') {
            this.translateCommonComponents();
            return;
        }

        const currentButton = switcher.querySelector('.site-lang-current');
        const currentText = switcher.querySelector('.site-lang-current-text');
        const menu = switcher.querySelector('.site-lang-menu');
        const options = Array.from(switcher.querySelectorAll('.site-lang-option'));
        const currentLanguage = this.languages.find(option => option.code === this.language) || this.languages[1];

        switcher.dataset.initialized = 'true';
        currentText.textContent = currentLanguage.label;
        currentButton.setAttribute('aria-label', this.translate('Choose language', '选择语言'));

        const closeMenu = () => {
            menu.hidden = true;
            currentButton.setAttribute('aria-expanded', 'false');
            currentButton.classList.remove('is-open');
        };

        const openMenu = () => {
            menu.hidden = false;
            currentButton.setAttribute('aria-expanded', 'true');
            currentButton.classList.add('is-open');
        };

        options.forEach(option => {
            const isActive = option.dataset.langCode === this.language;
            option.classList.toggle('is-active', isActive);
            option.setAttribute('aria-checked', String(isActive));
            option.addEventListener('click', () => {
                const language = option.dataset.langCode;
                closeMenu();
                this.setLanguage(language);
            });
        });

        currentButton.addEventListener('click', event => {
            event.stopPropagation();
            if (menu.hidden) {
                openMenu();
            } else {
                closeMenu();
            }
        });

        this.documentClickHandler = event => {
            if (!switcher.contains(event.target)) closeMenu();
        };
        this.documentKeyHandler = event => {
            if (event.key === 'Escape' && !menu.hidden) {
                closeMenu();
                currentButton.focus();
            }
        };

        document.addEventListener('click', this.documentClickHandler);
        document.addEventListener('keydown', this.documentKeyHandler);
        this.translateCommonComponents();
    }
}

window.CatimeLocale = new CatimeLocaleController();

class ComponentLoader {
    constructor() {
        this.componentsPath = this.getComponentsPath();
        this.loadedComponents = new Set();
    }

    getComponentsPath() {
        const currentPath = window.location.pathname;
        const depth = (currentPath.match(/\//g) || []).length - 1;

        if (currentPath === '/tray' || currentPath.startsWith('/tray/')
            || currentPath === '/plugins' || currentPath.startsWith('/plugins/')) {
            return '../components/';
        }

        if (currentPath.includes('/tools/') || depth > 1) {
            return '../../components/';
        }
        return './components/';
    }

    async loadComponent(componentName, targetSelector = null, position = 'beforeend') {
        if (this.loadedComponents.has(componentName)) return;

        try {
            const response = await fetch(`${this.componentsPath}${componentName}.html`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            const html = await response.text();
            const target = targetSelector ? document.querySelector(targetSelector) : null;
            (target || document.body).insertAdjacentHTML(target ? position : 'beforeend', html);

            this.loadedComponents.add(componentName);
            this.dispatchComponentLoadedEvent(componentName);
        } catch (error) {
            console.error(`Error loading ${componentName}:`, error);
        }
    }

    dispatchComponentLoadedEvent(componentName) {
        document.dispatchEvent(new CustomEvent('componentLoaded', {
            detail: { componentName },
        }));
    }

    async loadFooter(targetSelector = 'body') {
        await this.loadComponent('footer', targetSelector, 'beforeend');
        window.CatimeLocale.initializeFooter();
    }

    async loadScrollProgress(targetSelector = 'body') {
        await this.loadComponent('scroll-progress', targetSelector, 'afterbegin');

        if (typeof initScrollProgressIndicator === 'function') {
            setTimeout(() => initScrollProgressIndicator(), 150);
        }
        window.CatimeLocale.translateCommonComponents();
    }

    async loadAllCommonComponents() {
        await Promise.all([
            this.loadScrollProgress(),
            this.loadFooter(),
        ]);

        await new Promise(resolve => setTimeout(resolve, 100));
        window.CatimeLocale.translateCommonComponents();
        document.dispatchEvent(new CustomEvent('allComponentsLoaded'));
    }
}

window.componentLoader = new ComponentLoader();

function loadCommonComponents() {
    return window.componentLoader.loadAllCommonComponents();
}

function loadFooter(targetSelector) {
    return window.componentLoader.loadFooter(targetSelector);
}

function loadScrollProgress() {
    return window.componentLoader.loadScrollProgress();
}

document.addEventListener('DOMContentLoaded', function() {
    const script = document.querySelector('script[src*="components.js"]');
    if (script && script.hasAttribute('data-auto-load')) {
        loadCommonComponents();
    }
});

if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        ComponentLoader,
        loadCommonComponents,
        loadFooter,
        loadScrollProgress,
    };
}
