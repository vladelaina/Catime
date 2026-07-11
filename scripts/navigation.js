class CatimeNavigation {
    constructor() {
        this.currentPage = this.getCurrentPage();
        this.lastScrollY = 0;
        this.ticking = false;
        this.init();
    }

    getCurrentPage() {
        const path = window.location.pathname;
        const filename = path.split('/').pop() || 'index.html';

        if (path.includes('/tray-animations/')) {
            return 'tray-animations';
        }
        
        if (path.includes('/tools/font-tool/')) {
            return 'font-tool';
        }
        
        return filename.replace('.html', '') || 'index';
    }

    getPathPrefix() {
        const path = window.location.pathname;
        if (path.includes('/tray-animations/')) {
            return '../';
        }
        if (path.includes('/tools/font-tool/')) {
            return '../../';
        }
        return '';
    }

    generateNavigation() {
        const prefix = this.getPathPrefix();
        
        return `
        <header class="main-header" id="main-header">
            <nav class="container">
                <a href="${prefix || './'}" class="logo">
                    <img src="${prefix}assets/catime.webp" class="logo-img" alt="Catime Logo"> Catime
                </a>
                <ul class="nav-links">
                    <li><a href="${prefix || './'}"${this.currentPage === 'index' ? ' class="active"' : ''}>Home</a></li>
                    <li><a href="${prefix}guide"${this.currentPage === 'guide' ? ' class="active"' : ''}>Guide</a></li>
                    <li><a href="${prefix}tray-animations/"${this.currentPage === 'tray-animations' ? ' class="active"' : ''}>Tray Animations</a></li>
                    <li><a href="${prefix}about"${this.currentPage === 'about' ? ' class="active"' : ''}>About</a></li>
                    <li class="dropdown">
                        <a href="#" class="dropdown-toggle">Tools <i class="fas fa-chevron-down"></i></a>
                        <ul class="dropdown-menu">
                            <li><a href="${prefix}tools/font-tool/"${this.currentPage === 'font-tool' ? ' class="active"' : ''}><i class="fas fa-font"></i> Font Simplifier</a></li>
                        </ul>
                    </li>
                    <li><a href="https://github.com/vladelaina/Catime-Plugins" target="_blank" rel="noopener noreferrer">Plugins</a></li>
                    <li><a href="https://github.com/vladelaina/Catime" target="_blank" rel="noopener noreferrer">GitHub</a></li>
                </ul>
                ${this.generateActionButtons(prefix)}
            </nav>
        </header>`;
    }

    generateActionButtons(prefix) {
        const downloadUrl = (typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_URL) ? CATIME_CONFIG.DOWNLOAD_URL : 'https://github.com/vladelaina/Catime/releases';
        const downloadAttrs = (typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_FILE)
            ? `download="${CATIME_CONFIG.DOWNLOAD_FILE}"`
            : 'target="_blank" rel="noopener noreferrer"';

        if (this.currentPage === 'support') {
            return `<div class="nav-actions"><a href="${downloadUrl}" class="nav-button" data-download ${downloadAttrs}><i class="fas fa-download"></i> <span>Download</span></a></div>`;
        } else {
            return `<div class="nav-actions action-buttons">
                        <a href="${prefix}support" class="nav-button support-btn"><i class="fas fa-mug-hot"></i> <span>Support</span></a>
                        <a href="${downloadUrl}" class="nav-button download-btn" data-download ${downloadAttrs}><i class="fas fa-download"></i> <span>Download</span></a>
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
        this.initializeScrollBehavior();
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

        header.classList.toggle('scrolled', currentScrollY > 50);

        if (!isMobile || currentScrollY <= 100) {
            header.classList.remove('nav-hidden');
        } else if (currentScrollY > this.lastScrollY) {
            header.classList.add('nav-hidden');
        } else {
            header.classList.remove('nav-hidden');
        }

        this.lastScrollY = currentScrollY;
    }
}

new CatimeNavigation();
