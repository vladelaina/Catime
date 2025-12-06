class ComponentLoader {
    constructor() {
        this.componentsPath = this.getComponentsPath();
        this.loadedComponents = new Set();
    }

    getComponentsPath() {
        const currentPath = window.location.pathname;
        const depth = (currentPath.match(/\//g) || []).length - 1;
        
        if (currentPath.includes('/tools/') || depth > 1) {
            return '../../components/';
        }
        return './components/';
    }

    async loadComponent(componentName, targetSelector = null) {
        if (this.loadedComponents.has(componentName)) {
            return;
        }

        try {
            const response = await fetch(`${this.componentsPath}${componentName}.html`);
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            
            const html = await response.text();
            
            if (targetSelector) {
                const target = document.querySelector(targetSelector);
                if (target) {
                    target.insertAdjacentHTML('afterend', html);
                } else {
                    document.body.insertAdjacentHTML('beforeend', html);
                }
            } else {
                document.body.insertAdjacentHTML('beforeend', html);
            }
            
            this.loadedComponents.add(componentName);
            
            this.dispatchComponentLoadedEvent(componentName);
            
        } catch (error) {
            console.error(`Error loading ${componentName}:`, error);
        }
    }

    dispatchComponentLoadedEvent(componentName) {
        const event = new CustomEvent('componentLoaded', {
            detail: { componentName }
        });
        document.dispatchEvent(event);
    }

    async loadFooter(targetSelector = 'body') {
        await this.loadComponent('footer', targetSelector, 'beforeend');
    }

    async loadScrollProgress(targetSelector = 'body') {
        await this.loadComponent('scroll-progress', targetSelector, 'afterbegin');
        
        if (typeof initScrollProgressIndicator === 'function') {
            setTimeout(() => {
                initScrollProgressIndicator();
            }, 150);
        }
    }

    async loadAllCommonComponents() {
        await Promise.all([
            this.loadScrollProgress(),
            this.loadFooter()
        ]);
        
        await new Promise(resolve => setTimeout(resolve, 100));
        
        const event = new CustomEvent('allComponentsLoaded');
        document.dispatchEvent(event);
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
        loadScrollProgress
    };
}
