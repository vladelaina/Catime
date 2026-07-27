class CatimeNavigation {
    constructor() {
        this.currentPage = this.getCurrentPage();
        this.lastScrollY = 0;
        this.ticking = false;
        this.headerMorphAnimations = [];
        this.init();
    }

    getCurrentPage() {
        const path = window.location.pathname;
        const filename = path.split('/').pop() || 'index.html';

        if (path === '/tray' || path.startsWith('/tray/')) {
            return 'tray';
        }
        
        if (path.includes('/tools/font-tool/')) {
            return 'font-tool';
        }
        
        return filename.replace('.html', '') || 'index';
    }

    getPathPrefix() {
        const path = window.location.pathname;
        if (path === '/tray' || path.startsWith('/tray/')) {
            return '../';
        }
        if (path.includes('/tools/font-tool/')) {
            return '../../';
        }
        return '';
    }

    translate(english, chinese) {
        if (window.CatimeLocale) {
            return window.CatimeLocale.translate(english, chinese);
        }

        try {
            return localStorage.getItem('catime-language') === 'zh' ? chinese : english;
        } catch (error) {
            return english;
        }
    }

    generateNavigation() {
        const prefix = this.getPathPrefix();
        const labels = {
            home: this.translate('Home', '首页'),
            guide: this.translate('Guide', '指南'),
            trayAnimations: this.translate('Tray Animations', '托盘动画'),
            about: this.translate('About', '关于'),
            tools: this.translate('Tools', '工具'),
            fontSimplifier: this.translate('Font Simplifier', '字体简化工具'),
            plugins: this.translate('Plugins', '插件'),
            openMenu: this.translate('Open navigation', '打开导航菜单'),
            closeMenu: this.translate('Close navigation', '关闭导航菜单'),
        };
        
        return `
        <header class="main-header" id="main-header">
            <span class="main-header-surface" aria-hidden="true"></span>
            <nav class="container">
                <a href="${prefix || './'}" class="logo">
                    <img src="${prefix}assets/catime.webp" class="logo-img" alt="Catime Logo"> Catime
                </a>
                <div class="nav-menu" id="nav-menu">
                    <ul class="nav-links">
                        <li><a href="${prefix || './'}"${this.currentPage === 'index' ? ' class="active"' : ''}>${labels.home}</a></li>
                        <li><a href="${prefix}guide"${this.currentPage === 'guide' ? ' class="active"' : ''}>${labels.guide}</a></li>
                        <li><a href="${prefix}tray"${this.currentPage === 'tray' ? ' class="active"' : ''}>${labels.trayAnimations}</a></li>
                        <li><a href="${prefix}about"${this.currentPage === 'about' ? ' class="active"' : ''}>${labels.about}</a></li>
                        <li class="dropdown${this.currentPage === 'font-tool' ? ' active' : ''}">
                            <a href="#" class="dropdown-toggle" role="button" aria-haspopup="true" aria-expanded="false">${labels.tools} <i class="fas fa-chevron-down" aria-hidden="true"></i></a>
                            <ul class="dropdown-menu">
                                <li><a href="${prefix}tools/font-tool/"${this.currentPage === 'font-tool' ? ' class="active"' : ''}><i class="fas fa-font" aria-hidden="true"></i> ${labels.fontSimplifier}</a></li>
                            </ul>
                        </li>
                        <li><a href="https://github.com/vladelaina/Catime-Plugins" target="_blank" rel="noopener noreferrer">${labels.plugins}</a></li>
                        <li><a href="https://github.com/vladelaina/Catime" target="_blank" rel="noopener noreferrer">GitHub</a></li>
                    </ul>
                    ${this.generateActionButtons(prefix)}
                </div>
                <button class="mobile-menu-backdrop" type="button" tabindex="-1" aria-label="${labels.closeMenu}"></button>
                <button class="mobile-menu-toggle" type="button" aria-label="${labels.openMenu}" aria-controls="nav-menu" aria-expanded="false" data-open-label="${labels.openMenu}" data-close-label="${labels.closeMenu}">
                    <span></span><span></span><span></span>
                </button>
            </nav>
        </header>`;
    }

    generateActionButtons(prefix) {
        const downloadUrl = (typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_URL) ? CATIME_CONFIG.DOWNLOAD_URL : 'https://github.com/vladelaina/Catime/releases';
        const downloadAttrs = (typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_FILE)
            ? `download="${CATIME_CONFIG.DOWNLOAD_FILE}"`
            : 'target="_blank" rel="noopener noreferrer"';

        if (this.currentPage === 'support') {
            return `<div class="nav-actions"><a href="${downloadUrl}" class="nav-button download-btn" data-download ${downloadAttrs}><i class="fas fa-download" aria-hidden="true"></i> <span>${this.translate('Download', '下载')}</span></a></div>`;
        } else {
            return `<div class="nav-actions action-buttons">
                        <a href="${prefix}support" class="nav-button support-btn"><i class="fas fa-mug-hot" aria-hidden="true"></i> <span>${this.translate('Support', '支持项目')}</span></a>
                        <a href="${downloadUrl}" class="nav-button download-btn" data-download ${downloadAttrs}><i class="fas fa-download" aria-hidden="true"></i> <span>${this.translate('Download', '下载')}</span></a>
                    </div>`;
        }
    }

    init() {
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.render());
        } else {
            this.render();
        }
    }

    render() {
        let navContainer = document.querySelector('.main-header');
        
        if (!navContainer) {
            navContainer = document.createElement('div');
            document.body.insertBefore(navContainer, document.body.firstChild);
        }
        
        navContainer.outerHTML = this.generateNavigation();
        
        this.initializeInteractions();
    }

    initializeInteractions() {
        this.initializeMobileMenu();
        this.initializeToolsMenu();
        this.initializeScrollBehavior();
    }

    initializeMobileMenu() {
        const header = document.getElementById('main-header');
        const toggle = header?.querySelector('.mobile-menu-toggle');
        const backdrop = header?.querySelector('.mobile-menu-backdrop');
        if (!header || !toggle || !backdrop) return;

        const closeMenu = () => {
            header.classList.remove('mobile-menu-open');
            toggle.classList.remove('active');
            toggle.setAttribute('aria-expanded', 'false');
            toggle.setAttribute('aria-label', toggle.dataset.openLabel);
            document.body.classList.remove('mobile-menu-locked');
            this.closeToolsMenu();
        };

        const openMenu = () => {
            header.classList.remove('nav-hidden');
            header.classList.add('mobile-menu-open');
            toggle.classList.add('active');
            toggle.setAttribute('aria-expanded', 'true');
            toggle.setAttribute('aria-label', toggle.dataset.closeLabel);
            document.body.classList.add('mobile-menu-locked');
        };

        toggle.addEventListener('click', () => {
            if (header.classList.contains('mobile-menu-open')) {
                closeMenu();
            } else {
                openMenu();
            }
        });

        backdrop.addEventListener('click', closeMenu);
        header.querySelectorAll('.nav-links a:not(.dropdown-toggle), .nav-actions a, .logo').forEach(link => {
            link.addEventListener('click', closeMenu);
        });

        document.addEventListener('keydown', event => {
            if (event.key === 'Escape' && header.classList.contains('mobile-menu-open')) {
                closeMenu();
                toggle.focus();
            }
        });

        window.addEventListener('resize', () => {
            if (window.innerWidth > 768) closeMenu();
        });
    }

    initializeToolsMenu() {
        const dropdown = document.querySelector('.main-header .dropdown');
        const toggle = dropdown?.querySelector('.dropdown-toggle');
        if (!dropdown || !toggle) return;

        toggle.addEventListener('click', event => {
            event.preventDefault();
            event.stopPropagation();
            const isOpen = dropdown.classList.toggle('dropdown-open');
            toggle.setAttribute('aria-expanded', String(isOpen));
        });

        document.addEventListener('click', event => {
            if (!dropdown.contains(event.target)) this.closeToolsMenu();
        });
    }

    closeToolsMenu() {
        const dropdown = document.querySelector('.main-header .dropdown');
        if (!dropdown) return;
        dropdown.classList.remove('dropdown-open');
        dropdown.querySelector('.dropdown-toggle')?.setAttribute('aria-expanded', 'false');
    }
    
    initializeScrollBehavior() {
        const isMobile = () => window.innerWidth <= 768;

        const handleScroll = () => {
            if (this.ticking) return;

            this.ticking = true;
            requestAnimationFrame(() => {
                this.updateHeaderState(isMobile());
                this.ticking = false;
            });
        };

        const handleResize = () => {
            this.updateHeaderState(isMobile());
        };

        window.addEventListener('scroll', handleScroll, { passive: true });
        window.addEventListener('resize', handleResize);

        this.updateHeaderState(isMobile());
    }

    updateHeaderState(isMobile) {
        const header = document.getElementById('main-header');
        if (!header) return;

        const currentScrollY = window.scrollY;

        this.updateScrolledState(header, currentScrollY > 50, isMobile);

        if (header.classList.contains('mobile-menu-open')) {
            header.classList.remove('nav-hidden');
            this.lastScrollY = currentScrollY;
            return;
        }

        if (!isMobile || currentScrollY <= 100) {
            header.classList.remove('nav-hidden');
        } else if (currentScrollY > this.lastScrollY) {
            header.classList.add('nav-hidden');
        } else {
            header.classList.remove('nav-hidden');
        }

        this.lastScrollY = currentScrollY;
    }

    updateScrolledState(header, shouldBeScrolled, isMobile) {
        if (header.classList.contains('scrolled') === shouldBeScrolled) return;

        if (isMobile || window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
            this.headerMorphAnimations.forEach(animation => animation.cancel());
            this.headerMorphAnimations = [];
            header.classList.toggle('scrolled', shouldBeScrolled);
            return;
        }

        const items = Array.from(header.querySelector('.container').children);
        const startHeaderRect = header.getBoundingClientRect();
        const startItemRects = items.map(item => item.getBoundingClientRect());

        this.headerMorphAnimations.forEach(animation => animation.cancel());
        this.headerMorphAnimations = [];
        header.classList.toggle('scrolled', shouldBeScrolled);

        const targetHeaderRect = header.getBoundingClientRect();
        const targetItemRects = items.map(item => item.getBoundingClientRect());
        const duration = 650;
        const easing = 'cubic-bezier(0.16, 1, 0.3, 1)';
        const startCenterX = startHeaderRect.left + startHeaderRect.width / 2;
        const startCenterY = startHeaderRect.top + startHeaderRect.height / 2;
        const targetCenterX = targetHeaderRect.left + targetHeaderRect.width / 2;
        const targetCenterY = targetHeaderRect.top + targetHeaderRect.height / 2;

        const surface = header.querySelector('.main-header-surface');
        this.headerMorphAnimations.push(surface.animate(
            [
                {
                    transform: `translate(${startCenterX - targetCenterX}px, ${startCenterY - targetCenterY}px) scale(${startHeaderRect.width / targetHeaderRect.width}, ${startHeaderRect.height / targetHeaderRect.height})`,
                },
                { transform: 'translate(0, 0) scale(1, 1)' },
            ],
            { duration, easing },
        ));

        items.forEach((item, index) => {
            const startRect = startItemRects[index];
            const targetRect = targetItemRects[index];
            const deltaX = startRect.left - targetRect.left;
            const deltaY = startRect.top - targetRect.top;

            this.headerMorphAnimations.push(item.animate(
                [
                    { transform: `translate(${deltaX}px, ${deltaY}px)` },
                    { transform: 'translate(0, 0)' },
                ],
                { duration, easing },
            ));
        });

        const animationBatch = [...this.headerMorphAnimations];
        Promise.allSettled(animationBatch.map(animation => animation.finished))
            .then(() => {
                const isCurrentBatch = this.headerMorphAnimations.length === animationBatch.length
                    && this.headerMorphAnimations.every((animation, index) => animation === animationBatch[index]);
                if (isCurrentBatch) {
                    this.headerMorphAnimations = [];
                }
            });
    }
}

new CatimeNavigation();
