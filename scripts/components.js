/**
 * 通用组件加载器
 * 用于动态加载页脚和滚动进度指示器等通用组件
 */

class ComponentLoader {
    constructor() {
        this.componentsPath = this.getComponentsPath();
        this.loadedComponents = new Set();
    }

    /**
     * 获取组件路径（根据当前页面路径自动调整）
     */
    getComponentsPath() {
        const currentPath = window.location.pathname;
        const depth = (currentPath.match(/\//g) || []).length - 1;
        
        // 如果在子目录中（如 tools/font-tool/），需要向上回退
        if (currentPath.includes('/tools/') || depth > 1) {
            return '../../components/';
        }
        return './components/';
    }

    /**
     * 加载HTML组件
     */
    async loadComponent(componentName, targetSelector = null) {
        if (this.loadedComponents.has(componentName)) {
            console.log(`组件 ${componentName} 已加载，跳过重复加载`);
            return;
        }

        try {
            const response = await fetch(`${this.componentsPath}${componentName}.html`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const html = await response.text();
            
            if (targetSelector) {
                // 插入到指定位置
                const target = document.querySelector(targetSelector);
                if (target) {
                    target.insertAdjacentHTML('afterend', html);
                } else {
                    console.warn(`目标选择器 ${targetSelector} 未找到，将插入到body末尾`);
                    document.body.insertAdjacentHTML('beforeend', html);
                }
            } else {
                // 默认插入到body末尾
                document.body.insertAdjacentHTML('beforeend', html);
            }
            
            this.loadedComponents.add(componentName);
            console.log(`✅ 组件 ${componentName} 加载成功`);
            
            // 触发组件加载完成事件
            this.dispatchComponentLoadedEvent(componentName);
            
        } catch (error) {
            console.error(`❌ 加载组件 ${componentName} 失败:`, error);
        }
    }

    /**
     * 触发组件加载完成事件
     */
    dispatchComponentLoadedEvent(componentName) {
        const event = new CustomEvent('componentLoaded', {
            detail: { componentName }
        });
        document.dispatchEvent(event);
    }

    /**
     * 加载页脚组件
     */
    async loadFooter(targetSelector = 'body') {
        await this.loadComponent('footer', targetSelector, 'beforeend');
    }

    /**
     * 加载滚动进度指示器组件
     */
    async loadScrollProgress(targetSelector = 'body') {
        await this.loadComponent('scroll-progress', targetSelector, 'afterbegin');
        
        // 组件加载完成后，初始化滚动进度功能
        if (typeof initScrollProgressIndicator === 'function') {
            // 等待DOM更新
            setTimeout(() => {
                initScrollProgressIndicator();
                console.log('✅ 滚动进度指示器初始化完成');
            }, 150);
        }
        
        // 组件加载完成后，触发翻译
        setTimeout(() => {
            this.triggerScrollProgressTranslation();
        }, 200);
    }

    /**
     * 一次性加载所有通用组件
     */
    async loadAllCommonComponents() {
        console.log('🔄 开始加载通用组件...');
        
        // 并行加载组件
        await Promise.all([
            this.loadScrollProgress(),
            this.loadFooter()
        ]);
        
        console.log('✅ 所有通用组件加载完成');
        
        // 等待一小段时间确保DOM更新
        await new Promise(resolve => setTimeout(resolve, 100));
        
        // 触发所有组件加载完成事件
        const event = new CustomEvent('allComponentsLoaded');
        document.dispatchEvent(event);
        
        // 触发页脚翻译（如果有翻译功能）
        this.triggerFooterTranslation();
        
        // 触发滚动进度翻译
        this.triggerScrollProgressTranslation();
        
        console.log('🎉 组件初始化完全完成');
    }

    /**
     * 统一处理页脚翻译功能
     */
    translateFooterComponent() {
        // 获取当前语言设置
        const lang = localStorage.getItem('catime-language') || 'zh';
        
        const footerContent = document.querySelector('.main-footer .container');
        if (!footerContent) return;
        
        if (lang === 'en') {
            // 英文模式翻译
            // 翻译页脚文本内容
            footerContent.querySelectorAll('p').forEach(p => {
                const text = p.innerHTML;
                if (text.includes('基于') && text.includes('许可开源')) {
                    p.innerHTML = 'Released under <a href="https://github.com/vladelaina/Catime/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">Apache 2.0</a> license';
                } else if (text.includes('图标画师')) {
                    p.innerHTML = 'Icon Artist: <a href="https://space.bilibili.com/26087398" target="_blank" rel="noopener noreferrer">猫屋敷梨梨Official</a>';
                }
            });
            
            // 翻译页脚链接
            const footerLinks = footerContent.querySelectorAll('.footer-links a');
            footerLinks.forEach(link => {
                if (link.textContent === '问题反馈') {
                    link.textContent = 'Feedback';
                    // 英文模式下跳转到GitHub Issues
                    link.href = 'https://github.com/vladelaina/Catime/issues';
                    link.target = '_blank';
                    link.rel = 'noopener noreferrer';
                } else if (link.textContent === '隐私政策') {
                    link.textContent = 'Privacy Policy';
                }
            });
        } else {
            // 中文模式，恢复原文本和链接
            // 恢复页脚文本内容
            footerContent.querySelectorAll('p').forEach(p => {
                const text = p.innerHTML;
                if (text.includes('Released under') && text.includes('license')) {
                    p.innerHTML = '基于 <a href="https://github.com/vladelaina/Catime/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">Apache 2.0</a> 许可开源';
                } else if (text.includes('Icon Artist:')) {
                    p.innerHTML = '图标画师: <a href="https://space.bilibili.com/26087398" target="_blank" rel="noopener noreferrer">猫屋敷梨梨Official</a>';
                }
            });
            
            // 恢复页脚链接
            const footerLinks = footerContent.querySelectorAll('.footer-links a');
            footerLinks.forEach(link => {
                if (link.textContent === 'Feedback') {
                    link.textContent = '问题反馈';
                    // 中文模式下跳转到B站私信
                    link.href = 'https://message.bilibili.com/#/whisper/mid1862395225';
                    link.target = '_blank';
                    link.rel = 'noopener noreferrer';
                } else if (link.textContent === 'Privacy Policy') {
                    link.textContent = '隐私政策';
                }
            });
        }
    }

    /**
     * 统一处理滚动进度组件翻译功能
     */
    translateScrollProgressComponent() {
        // 获取当前语言设置
        const lang = localStorage.getItem('catime-language') || 'zh';
        
        const scrollTooltip = document.querySelector('.scroll-progress-tooltip');
        if (!scrollTooltip) return;
        
        if (lang === 'en') {
            // 英文模式
            scrollTooltip.textContent = 'Back to Top';
        } else {
            // 中文模式
            scrollTooltip.textContent = '返回顶部';
        }
    }

    /**
     * 触发页脚翻译功能 (保持兼容性)
     */
    triggerFooterTranslation() {
        this.translateFooterComponent();
    }

    /**
     * 触发滚动进度组件翻译功能 (保持兼容性)
     */
    triggerScrollProgressTranslation() {
        this.translateScrollProgressComponent();
    }
}

// 创建全局组件加载器实例
window.componentLoader = new ComponentLoader();

/**
 * 便捷函数：自动加载通用组件
 */
function loadCommonComponents() {
    return window.componentLoader.loadAllCommonComponents();
}

/**
 * 便捷函数：仅加载页脚
 */
function loadFooter(targetSelector) {
    return window.componentLoader.loadFooter(targetSelector);
}

/**
 * 便捷函数：仅加载滚动进度指示器
 */
function loadScrollProgress() {
    return window.componentLoader.loadScrollProgress();
}

/**
 * 便捷函数：翻译页脚组件
 */
function translateFooterComponent() {
    return window.componentLoader.translateFooterComponent();
}

/**
 * 便捷函数：翻译滚动进度组件
 */
function translateScrollProgressComponent() {
    return window.componentLoader.translateScrollProgressComponent();
}

/**
 * 便捷函数：翻译所有组件
 */
function translateAllComponents() {
    translateFooterComponent();
    translateScrollProgressComponent();
}

// 自动初始化（如果页面包含了这个脚本）
document.addEventListener('DOMContentLoaded', function() {
    // 检查是否有data-auto-load属性
    const script = document.querySelector('script[src*="components.js"]');
    if (script && script.hasAttribute('data-auto-load')) {
        loadCommonComponents();
    }
});

// 导出给其他脚本使用
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        ComponentLoader,
        loadCommonComponents,
        loadFooter,
        loadScrollProgress,
        translateFooterComponent,
        translateScrollProgressComponent,
        translateAllComponents
    };
}
