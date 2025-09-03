// 统一导航管理 - Catime Project
// 所有页面的导航都从这里统一生成和管理

class CatimeNavigation {
    constructor() {
        this.currentPage = this.getCurrentPage();
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
        
        return `
        <header class="main-header">
            <nav class="container">
                <a href="${prefix}index.html" class="logo">
                    <img src="${prefix}assets/catime.webp" class="logo-img" alt="Catime Logo"> Catime
                </a>
                <ul class="nav-links">
                    <li><a href="${prefix}index.html"${this.currentPage === 'index' ? ' class="active"' : ''}>首页</a></li>
                    <li><a href="${prefix}guide.html"${this.currentPage === 'guide' ? ' class="active"' : ''}>指南</a></li>
                    <li><a href="${prefix}about.html"${this.currentPage === 'about' ? ' class="active"' : ''}>关于</a></li>
                    <li class="dropdown">
                        <a href="#" class="dropdown-toggle">工具 <i class="fas fa-chevron-down"></i></a>
                        <ul class="dropdown-menu">
                            <li><a href="${prefix}tools/font-tool/index.html"${this.currentPage === 'font-tool' ? ' class="active"' : ''}><i class="fas fa-font"></i> 字体简化工具</a></li>
                            <!-- 未来可以在这里添加更多工具 -->
                        </ul>
                    </li>
                    <li><a href="https://github.com/vladelaina/Catime" target="_blank" rel="noopener noreferrer">GitHub</a></li>
                    <li><a href="#" id="language-toggle"><i class="fas fa-language"></i> English</a></li>
                    ${this.generateActionButtons(prefix)}
                </ul>
            </nav>
        </header>`;
    }

    // 生成操作按钮（根据页面不同有所差异）
    generateActionButtons(prefix) {
        if (this.currentPage === 'support') {
            // 支持页面只显示下载按钮
            return `<li><a href="https://apps.microsoft.com/detail/9n3mzdf1z34v?referrer=appbadge&launch=true&mode=full" class="nav-button" target="_blank" rel="noopener noreferrer"><i class="fas fa-download"></i> <span>下载</span></a></li>`;
        } else {
            // 其他页面显示完整按钮
            return `<li class="action-buttons">
                        <a href="${prefix}support.html" class="nav-button support-btn"><i class="fas fa-mug-hot"></i> <span>支持项目</span></a>
                        <a href="https://apps.microsoft.com/detail/9n3mzdf1z34v?referrer=appbadge&launch=true&mode=full" class="nav-button download-btn" target="_blank" rel="noopener noreferrer"><i class="fas fa-download"></i> <span>下载</span></a>
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
        // 语言切换功能（如果需要的话）
        const languageToggle = document.getElementById('language-toggle');
        if (languageToggle) {
            // 这里可以添加语言切换逻辑
        }

        // 下拉菜单交互已经通过 CSS :hover 实现，无需额外 JavaScript
    }
}

// 自动初始化导航
new CatimeNavigation();
