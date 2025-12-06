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
        
        if (path.includes('/tools/font-tool/')) {
            return 'font-tool';
        }
        
        return filename.replace('.html', '') || 'index';
    }

    getPathPrefix() {
        const path = window.location.pathname;
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
                <a href="${prefix}index.html" class="logo">
                    <img src="${prefix}assets/catime.webp" class="logo-img" alt="Catime Logo"> Catime
                </a>
                <ul class="nav-links">
                    <li><a href="${prefix}index.html"${this.currentPage === 'index' ? ' class="active"' : ''}>Home</a></li>
                    <li><a href="${prefix}guide.html"${this.currentPage === 'guide' ? ' class="active"' : ''}>Guide</a></li>
                    <li><a href="${prefix}about.html"${this.currentPage === 'about' ? ' class="active"' : ''}>About</a></li>
                    <li class="dropdown">
                        <a href="#" class="dropdown-toggle">Tools <i class="fas fa-chevron-down"></i></a>
                        <ul class="dropdown-menu">
                            <li><a href="${prefix}tools/font-tool/index.html"${this.currentPage === 'font-tool' ? ' class="active"' : ''}><i class="fas fa-font"></i> Font Simplifier</a></li>
                        </ul>
                    </li>
                    <li><a href="https://github.com/vladelaina/Catime" target="_blank" rel="noopener noreferrer">GitHub</a></li>
                </ul>
                ${this.generateActionButtons(prefix)}
            </nav>
        </header>`;
    }

    generateActionButtons(prefix) {
        if (this.currentPage === 'support') {
            const url = (typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_URL) ? CATIME_CONFIG.DOWNLOAD_URL : 'https://github.com/vladelaina/Catime/releases';
            return `<div class="nav-actions"><a href="${url}" class="nav-button" target="_blank" rel="noopener noreferrer"><i class="fas fa-download"></i> <span>Download</span></a></div>`;
        } else {
            return `<div class="nav-actions action-buttons">
                        <a href="${prefix}support.html" class="nav-button support-btn"><i class="fas fa-mug-hot"></i> <span>Support</span></a>
                        <a href="${(typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_URL) ? CATIME_CONFIG.DOWNLOAD_URL : 'https://github.com/vladelaina/Catime/releases'}" class="nav-button download-btn" target="_blank" rel="noopener noreferrer"><i class="fas fa-download"></i> <span>Download</span></a>
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
            if (!isMobile()) return;
            
            if (!this.ticking) {
                requestAnimationFrame(() => {
                    this.updateNavVisibility();
                    this.ticking = false;
                });
                this.ticking = true;
            }
        };
        
        const handleResize = () => {
            const header = document.getElementById('main-header');
            if (!header) return;
            
            if (!isMobile()) {
                header.classList.remove('nav-hidden');
            }
        };
        
        window.addEventListener('scroll', handleScroll, { passive: true });
        window.addEventListener('resize', handleResize);
        
        handleResize();
    }
    
    updateNavVisibility() {
        const header = document.getElementById('main-header');
        if (!header) return;
        
        const currentScrollY = window.scrollY;
        const scrollThreshold = 100;
        
        if (currentScrollY <= scrollThreshold) {
            header.classList.remove('nav-hidden');
        } else {
            if (currentScrollY > this.lastScrollY) {
                header.classList.add('nav-hidden');
            } else {
                header.classList.remove('nav-hidden');
            }
        }
        
        this.lastScrollY = currentScrollY;
    }
}

new CatimeNavigation();
