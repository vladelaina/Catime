// 统一导航管理 - Catime Project
// 所有页面的导航都从这里统一生成和管理

class CatimeNavigation {
    constructor() {
        this.currentPage = this.getCurrentPage();
        this.lastScrollY = 0;
        this.ticking = false;
        this.init();
    }

    // 获取当前页面
    getCurrentPage() {
        const path = window.location.pathname;
        const filename = path.split('/').pop() || 'index.html';
        
        // 处理特殊路径
        if (path.includes('/tools/font-tool/')) {
            return 'font-tool';
        }
        
        return filename.replace('.html', '') || 'index';
    }

    // 获取相对路径前缀
    getPathPrefix() {
        const path = window.location.pathname;
        if (path.includes('/tools/font-tool/')) {
            return '../../';
        }
        return '';
    }

    // 生成导航 HTML
    generateNavigation() {
        const prefix = this.getPathPrefix();
        const lang = localStorage.getItem('catime-language') || 'zh';
        const isEnglish = lang === 'en';
        
        return `
        <header class="main-header" id="main-header">
            <nav class="container">
                <a href="${prefix}index.html" class="logo">
                    <img src="${prefix}assets/catime.webp" class="logo-img" alt="Catime Logo"> Catime
                </a>
                <ul class="nav-links">
                    <li><a href="${prefix}index.html"${this.currentPage === 'index' ? ' class="active"' : ''}>${isEnglish ? 'Home' : '首页'}</a></li>
                    <li><a href="${prefix}guide.html"${this.currentPage === 'guide' ? ' class="active"' : ''}>${isEnglish ? 'Guide' : '指南'}</a></li>
                    <li><a href="${prefix}about.html"${this.currentPage === 'about' ? ' class="active"' : ''}>${isEnglish ? 'About' : '关于'}</a></li>
                    <li class="dropdown">
                        <a href="#" class="dropdown-toggle">${isEnglish ? 'Tools' : '工具'} <i class="fas fa-chevron-down"></i></a>
                        <ul class="dropdown-menu">
                            <li><a href="${prefix}tools/font-tool/index.html"${this.currentPage === 'font-tool' ? ' class="active"' : ''}><i class="fas fa-font"></i> ${isEnglish ? 'Font Simplifier' : '字体简化工具'}</a></li>
                            <!-- 未来可以在这里添加更多工具 -->
                        </ul>
                    </li>
                    <li><a href="https://github.com/vladelaina/Catime" target="_blank" rel="noopener noreferrer">GitHub</a></li>
                    <li><a href="#" id="language-toggle"><i class="fas fa-language"></i> ${isEnglish ? '中文' : 'English'}</a></li>
                    ${this.generateActionButtons(prefix)}
                </ul>
            </nav>
        </header>`;
    }

    // 生成操作按钮（根据页面不同有所差异）
    generateActionButtons(prefix) {
        const lang = localStorage.getItem('catime-language') || 'zh';
        const isEnglish = lang === 'en';
        
        if (this.currentPage === 'support') {
            // 支持页面只显示下载按钮
            const url = (typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_URL) ? CATIME_CONFIG.DOWNLOAD_URL : 'https://github.com/vladelaina/Catime/releases';
            return `<li><a href="${url}" class="nav-button" target="_blank" rel="noopener noreferrer"><i class="fas fa-download"></i> <span>${isEnglish ? 'Download' : '下载'}</span></a></li>`;
        } else {
            // 其他页面显示完整按钮
            return `<li class="action-buttons">
                        <a href="${prefix}support.html" class="nav-button support-btn"><i class="fas fa-mug-hot"></i> <span>${isEnglish ? 'Support' : '支持项目'}</span></a>
                        <a href="${(typeof CATIME_CONFIG !== 'undefined' && CATIME_CONFIG.DOWNLOAD_URL) ? CATIME_CONFIG.DOWNLOAD_URL : 'https://github.com/vladelaina/Catime/releases'}" class="nav-button download-btn" target="_blank" rel="noopener noreferrer"><i class="fas fa-download"></i> <span>${isEnglish ? 'Download' : '下载'}</span></a>
                    </li>`;
        }
    }

    // 初始化导航
    init() {
        // 等待 DOM 加载完成
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.render());
        } else {
            this.render();
        }
    }

    // 渲染导航
    render() {
        // 查找导航容器
        let navContainer = document.querySelector('.main-header');
        
        if (!navContainer) {
            // 如果没有找到容器，创建一个并插入到 body 开头
            navContainer = document.createElement('div');
            document.body.insertBefore(navContainer, document.body.firstChild);
        }
        
        // 插入导航 HTML
        navContainer.outerHTML = this.generateNavigation();
        
        // 重新初始化可能需要的 JavaScript 功能
        this.initializeInteractions();
    }

    // 初始化交互功能
    initializeInteractions() {
        // 语言切换功能
        const languageToggle = document.getElementById('language-toggle');
        if (languageToggle) {
            // 设置初始按钮文本
            this.updateLanguageToggleText();
            
            // 添加点击事件监听器（避免重复绑定）
            if (!languageToggle.dataset.navListener) {
                languageToggle.addEventListener('click', (e) => {
                    e.preventDefault();
                    
                    const currentLang = localStorage.getItem('catime-language') || 'zh';
                    const newLang = currentLang === 'zh' ? 'en' : 'zh';
                    
                    localStorage.setItem('catime-language', newLang);
                    
                    // 重新渲染导航栏
                    this.render();
                    
                    // 如果当前页面有特定的语言切换处理，则重新加载页面
                    if (this.currentPage === 'font-tool' || typeof applyLanguage === 'function') {
                        window.location.reload();
                    } else {
                        // 否则手动触发组件翻译
                        if (typeof translateAllComponents === 'function') {
                            translateAllComponents();
                        }
                    }
                });
                
                languageToggle.dataset.navListener = 'true';
            }
        }

        // 下拉菜单交互已经通过 CSS :hover 实现，无需额外 JavaScript
        
        // 初始化移动端滚动隐藏行为
        this.initializeScrollBehavior();
    }
    
    // 初始化移动端滚动隐藏导航栏
    initializeScrollBehavior() {
        // 检查是否为移动设备
        const isMobile = () => window.innerWidth <= 768;
        
        // 处理滚动事件
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
        
        // 处理窗口大小改变
        const handleResize = () => {
            const header = document.getElementById('main-header');
            if (!header) return;
            
            if (!isMobile()) {
                // 桌面端：始终显示导航栏
                header.classList.remove('nav-hidden');
            }
        };
        
        // 添加事件监听器
        window.addEventListener('scroll', handleScroll, { passive: true });
        window.addEventListener('resize', handleResize);
        
        // 初始化时检查一次
        handleResize();
    }
    
    // 更新导航栏可见性
    updateNavVisibility() {
        const header = document.getElementById('main-header');
        if (!header) return;
        
        const currentScrollY = window.scrollY;
        const scrollThreshold = 100; // 滚动100px后开始隐藏
        
        // 如果在顶部附近，始终显示
        if (currentScrollY <= scrollThreshold) {
            header.classList.remove('nav-hidden');
        } else {
            // 向下滚动时隐藏，向上滚动时显示
            if (currentScrollY > this.lastScrollY) {
                // 向下滚动
                header.classList.add('nav-hidden');
            } else {
                // 向上滚动
                header.classList.remove('nav-hidden');
            }
        }
        
        this.lastScrollY = currentScrollY;
    }
    
    // 更新语言切换按钮文本
    updateLanguageToggleText() {
        const languageToggle = document.getElementById('language-toggle');
        if (!languageToggle) return;
        
        const lang = localStorage.getItem('catime-language') || 'zh';
        const isEnglish = lang === 'en';
        
        languageToggle.innerHTML = `<i class="fas fa-language"></i> ${isEnglish ? '中文' : 'English'}`;
    }
}

// 自动初始化导航
new CatimeNavigation();
