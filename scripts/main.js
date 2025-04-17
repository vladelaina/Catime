// AOS 初始化
document.addEventListener('DOMContentLoaded', function() {
    // 初始化 AOS 动画库
    AOS.init({
        duration: 800,
        once: true,
        offset: 50,
    });

    // 初始化滚动进度指示器
    initScrollProgressIndicator();

    // 为图片添加3D倾斜效果
    document.querySelectorAll('.animated-image img').forEach(img => {
        const container = img.closest('.animated-image');
        if (!container) return;
        
        // 最大倾斜角度
        const maxRotateX = 10;
        const maxRotateY = 15;
        
        // 呼吸效果的定时器
        let breatheTimer = null;
        
        // 当前旋转角度
        let currentRotateX = 0;
        let currentRotateY = 0;
        
        // 设置呼吸效果
        function startBreatheEffect() {
            if (breatheTimer) return; // 避免重复启动
            
            let phase = 0;
            breatheTimer = setInterval(() => {
                // 添加缩放效果，轻微的呼吸效果
                const scale = 1.02 + Math.sin(phase) * 0.015;
                
                // 应用变换，结合当前的旋转角度和缩放效果
                img.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                
                phase += 0.05;
            }, 30);
        }

        // 停止呼吸效果
        function stopBreatheEffect() {
            if (breatheTimer) {
                clearInterval(breatheTimer);
                breatheTimer = null;
            }
        }
        
        container.addEventListener('mousemove', function(e) {
            // 获取鼠标在元素内的相对位置（0-1）
            const rect = container.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            // 计算倾斜角度（转换为-maxRotate到maxRotate的范围）
            currentRotateY = (x - 0.5) * 2 * maxRotateY;
            currentRotateX = (y - 0.5) * -2 * maxRotateX; // 负号让鼠标在顶部时，图片向上倾斜
        });
        
        // 鼠标离开时恢复原始状态
        container.addEventListener('mouseleave', function() {
            stopBreatheEffect(); // 停止呼吸效果
            currentRotateX = 0;
            currentRotateY = 0;
            // 直接设置回初始状态
            img.style.transform = 'scale(1) perspective(1000px)';
        });
        
        // 鼠标进入时准备变换并启动呼吸效果
        container.addEventListener('mouseenter', function() {
            img.style.transition = 'transform 0.2s ease-out';
            startBreatheEffect(); // 启动呼吸效果
        });
        
        // 添加点击效果：按下和回弹
        img.addEventListener('mousedown', function() {
            // 暂时停止呼吸效果
            stopBreatheEffect();
            // 按下效果 - 只添加轻微下沉，不缩放
            img.style.transform = `scale(0.98) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
        });
        
        // 鼠标释放时回弹
        document.addEventListener('mouseup', function(event) {
            if (container.matches(':hover')) {
                // 如果鼠标仍在图片上，回弹到悬停状态
                if (event.target === img || img.contains(event.target)) {
                    // 添加更快速的回弹效果
                    img.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                    
                    // 重新启动呼吸效果之前，先执行回弹动画
                    setTimeout(() => {
                        startBreatheEffect();
                    }, 150);
                }
            }
        });
    });
    
    // 语言切换功能初始化
    initLanguageToggle();
});

// 初始化滚动进度指示器
function initScrollProgressIndicator() {
    const scrollProgressContainer = document.getElementById('scrollProgressContainer');
    if (!scrollProgressContainer) return;

    const scrollProgressCircle = document.querySelector('.scroll-progress-circle-fill');
    const scrollProgressPercentage = document.querySelector('.scroll-progress-percentage');

    if (!scrollProgressCircle || !scrollProgressPercentage) return;

    // 窗口滚动时更新进度
    window.addEventListener('scroll', function() {
        updateScrollProgress();
    });

    // 点击滚动进度指示器返回顶部
    scrollProgressContainer.addEventListener('click', function() {
        // 添加点击效果
        this.classList.add('clicked');
        
        // 平滑滚动到顶部
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
        
        // 移除点击效果
        setTimeout(() => {
            this.classList.remove('clicked');
        }, 500);
    });

    // 初始化滚动进度
    updateScrollProgress();

    // 更新滚动进度函数
    function updateScrollProgress() {
        const scrollTop = window.scrollY;
        const scrollHeight = document.documentElement.scrollHeight - window.innerHeight;
        const scrollPercentage = (scrollTop / scrollHeight) * 100;
        
        // 更新圆形进度条
        const perimeter = Math.PI * 2 * 45; // 2πr，r=45
        const strokeDashoffset = perimeter * (1 - scrollPercentage / 100);
        scrollProgressCircle.style.strokeDashoffset = strokeDashoffset;
        
        // 更新百分比文本
        scrollProgressPercentage.textContent = `${Math.round(scrollPercentage)}%`;
        
        // 根据滚动位置切换容器可见性
        if (scrollTop > 300) {
            scrollProgressContainer.style.opacity = '1';
            scrollProgressContainer.style.transform = 'scale(1)';
            scrollProgressContainer.style.pointerEvents = 'auto';
        } else {
            scrollProgressContainer.style.opacity = '0';
            scrollProgressContainer.style.transform = 'scale(0.8)';
            scrollProgressContainer.style.pointerEvents = 'none';
        }
    }
}

// 语言切换功能
function initLanguageToggle() {
    const languageToggle = document.getElementById('language-toggle');
    
    if (languageToggle) {
        // 检查当前语言设置（默认是中文）
        let currentLang = localStorage.getItem('catime-language') || 'zh';
        
        // 根据当前语言设置按钮文本
        updateToggleText(currentLang);
        
        // 点击语言切换按钮
        languageToggle.addEventListener('click', function(e) {
            e.preventDefault();
            
            // 切换语言
            currentLang = currentLang === 'zh' ? 'en' : 'zh';
            
            // 保存语言设置
            localStorage.setItem('catime-language', currentLang);
            
            // 更新按钮文本
            updateToggleText(currentLang);
            
            // 重新加载页面以应用新语言
            window.location.reload();
        });
    }
    
    // 根据当前语言更新切换按钮文本
    function updateToggleText(lang) {
        if (!languageToggle) return;
        
        if (lang === 'zh') {
            // 当前是中文，显示切换到英文的选项
            languageToggle.innerHTML = '<i class="fas fa-language"></i> English';
        } else {
            // 当前是英文，显示切换到中文的选项
            languageToggle.innerHTML = '<i class="fas fa-language"></i> 中文';
        }
    }
    
    // 应用当前语言设置
    applyLanguage();
    
    // 根据当前语言设置应用翻译
    function applyLanguage() {
        const lang = localStorage.getItem('catime-language') || 'zh';
        
        // 设置html标签的lang属性
        const htmlRoot = document.getElementById('html-root');
        if (htmlRoot) {
            htmlRoot.lang = lang === 'zh' ? 'zh-CN' : 'en';
        }
        
        // 更新页面标题和描述
        if (lang === 'en') {
            const pageTitle = document.getElementById('page-title');
            if (pageTitle) {
                pageTitle.textContent = 'Catime - Timer & Pomodoro Clock';
            }
            
            const metaDescription = document.getElementById('meta-description');
            if (metaDescription) {
                metaDescription.setAttribute('content', 'Catime - A minimalist, modern, efficient transparent timer and pomodoro clock for Windows, with a cute style.');
            }
        }
        
        // 如果是中文，不需要翻译
        if (lang === 'zh') return;
        
        // 英文翻译映射
        const translations = {
            // 导航区域
            '首页': 'Home',
            '指南': 'Guide',
            '关于': 'About',
            '支持项目': 'Support',
            '下载': 'Download',
            
            // 英雄区域
            '时间管理，嚯~原来可以这么卡哇伊！': 'Time management, just got super kawaii!',
            '立即获取': 'Get Started',
            '查看源码': 'View Code',
            
            // 特性区域
            '核心特性 ✨': 'Core Features ✨',
            '轻巧可爱，功能不少': 'Light, cute, and feature-rich',
            '透明悬浮 & 穿透': 'Transparent & Click-through',
            '灵活计时模式': 'Flexible Timing Modes',
            '个性化定制': 'Personalization',
            '智能超时动作': 'Smart Timeout Actions', 
            '轻量高效': 'Lightweight & Efficient',
            '开源免费': 'Open Source & Free',
            
            // 社区区域
            '开源社区认可 ⭐': 'Community Recognition ⭐',
            '获得超过 1.4k 的 GitHub 星标，持续增长中': 'Over 1.4k GitHub stars and growing',
            
            // 使用场景区域
            '使用场景 🌟': 'Use Cases 🌟',
            'Catime，各种场景的得力助手': 'Catime, your assistant in various scenarios',
            '游戏场景': 'Gaming',
            '自动打开软件': 'Auto Launch Apps',
            'PPT演示时使用': 'Presentations',
            
            // 号召性区域
            '立刻下载，开启可爱又高效的专注旅程！': 'Download now and start your cute & efficient focus journey!',
            '免费带走 Catime': 'Get Catime for Free',
            
            // 特别感谢区域
            '特别感谢 🙏': 'Special Thanks 🙏',
            '特别感谢以下大佬对于本项目的推荐（按时间排序）': 'Special thanks to the following creators for recommending this project (chronological order)',
            
            // 贡献者区域
            '致谢贡献者 🙏': 'Contributors 🙏',
            '感谢所有为 Catime 做出贡献的小伙伴们！': 'Thanks to everyone who contributed to Catime!',
            
            // 动态生成的文本
            '准备好和 Catime 一起管理时间了吗？': 'Ready to manage time with Catime?'
        };
        
        // 遍历所有文本节点进行翻译
        translateTextNodes(document.body, translations);
    }
    
    // 递归翻译文本节点
    function translateTextNodes(element, translations) {
        if (element.nodeType === Node.TEXT_NODE) {
            // 处理文本节点
            let text = element.nodeValue.trim();
            if (text && translations[text]) {
                element.nodeValue = element.nodeValue.replace(text, translations[text]);
            }
        } else if (element.nodeType === Node.ELEMENT_NODE) {
            // 处理特定ID和其他属性
            translateElementById(element, translations);
            
            // 处理元素的子节点
            for (let i = 0; i < element.childNodes.length; i++) {
                translateTextNodes(element.childNodes[i], translations);
            }
        }
    }
    
    // 根据元素ID和其他属性进行翻译
    function translateElementById(element, translations) {
        // 翻译特定ID元素的文本
        const idMappings = {
            'intro': 'Catime',
            'transparent-title': 'Transparent & Click-through',
            'timer-title': 'Flexible Timing Modes',
            'custom-title': 'Personalization',
            'action-title': 'Smart Timeout Actions',
            'lightweight-title': 'Lightweight & Efficient',
            'opensource-title': 'Open Source & Free',
            'features-title': 'Core Features ✨',
            'stats-title': 'Community Recognition ⭐',
            'scenarios-title': 'Use Cases 🌟',
            'gaming-title': 'Gaming',
            'autoopen-title': 'Auto Launch Apps',
            'presentation-title': 'Presentations',
            'cta-title': 'Ready to manage time with Catime?',
            'thanks-title': 'Special Thanks 🙏',
            'contributors-title': 'Contributors 🙏'
        };
        
        // 处理特定ID的元素
        if (element.id && idMappings[element.id]) {
            // 对于有特定ID的元素，直接设置内容
            if (element.tagName === 'H1' || element.tagName === 'H2' || element.tagName === 'H3') {
                element.textContent = idMappings[element.id];
            }
        }
        
        // 翻译placeholder属性
        if (element.placeholder && translations[element.placeholder]) {
            element.placeholder = translations[element.placeholder];
        }
        
        // 翻译alt属性
        if (element.alt && translations[element.alt]) {
            element.alt = translations[element.alt];
        }
        
        // 翻译title属性
        if (element.title && translations[element.title]) {
            element.title = translations[element.title];
        }
    }
}
