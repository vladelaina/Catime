// AOS 初始化
document.addEventListener('DOMContentLoaded', function() {
    // 初始化 AOS 动画库
    AOS.init({
        duration: 800,
        once: true,
        offset: 50,
    });

    // 设置所有下载按钮的URL
    setDownloadUrls();

    // 初始化滚动进度指示器（延后到组件加载完成）
    // initScrollProgressIndicator(); // 现在由组件加载器处理

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

// 设置所有下载按钮的URL
function setDownloadUrls() {
    // 检查全局配置是否存在
    if (typeof CATIME_CONFIG === 'undefined') {
        console.error('全局配置未加载');
        return;
    }

    // 设置所有带有id的下载按钮
    const downloadButtons = [
        'download-btn',
        'hero-download-btn',
        'cta-download-btn'
    ];

    downloadButtons.forEach(id => {
        const btn = document.getElementById(id);
        if (btn) {
            btn.href = CATIME_CONFIG.DOWNLOAD_URL;
        }
    });
}

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
    
    // 检查URL中是否有语言参数
    function getLanguageFromURL() {
        const urlParams = new URLSearchParams(window.location.search);
        return urlParams.get('lang');
    }
    
    // 检测浏览器语言
    function getBrowserLanguage() {
        return navigator.language || navigator.userLanguage || '';
    }
    
    // 从URL获取语言设置
    const urlLang = getLanguageFromURL();
    if (urlLang === 'en' || urlLang === 'zh') {
        localStorage.setItem('catime-language', urlLang);
    } else if (!localStorage.getItem('catime-language')) {
        // 如果URL中没有语言参数，且本地存储中也没有语言设置，则检查浏览器语言
        const browserLang = getBrowserLanguage().toLowerCase();
        // 如果浏览器语言不是中文（zh，zh-CN，zh-TW等），则设置为英文
        if (!browserLang.startsWith('zh')) {
            localStorage.setItem('catime-language', 'en');
        } else {
            localStorage.setItem('catime-language', 'zh');
        }
    }
    
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
            
            // 处理CTA区域的波浪字母
            handleWaveLetters();
            
            // 处理滚动进度提示
            const scrollTooltip = document.querySelector('.scroll-progress-tooltip');
            if (scrollTooltip) {
                scrollTooltip.textContent = 'Back to Top';
            }
            
            // 处理hero描述区域
            handleHeroDescription();

            // 处理问题反馈链接，在英文模式下指向GitHub issues页面
            const feedbackLink = document.querySelector('.footer-links a');
            if (feedbackLink) {
                feedbackLink.href = 'https://github.com/vladelaina/Catime/issues';
                feedbackLink.target = '_blank';
                feedbackLink.rel = 'noopener noreferrer';
            }
        } else {
            // 在中文模式下恢复原链接
            const feedbackLink = document.querySelector('.footer-links a');
            if (feedbackLink) {
                feedbackLink.href = 'https://message.bilibili.com/#/whisper/mid1862395225';
                feedbackLink.target = '_blank';
                feedbackLink.rel = 'noopener noreferrer';
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
            '博客': 'Blog',
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
            '遵循 Apache 2.0 协议，免费使用，欢迎大家一起贡献代码~': 'Licensed under Apache 2.0, free to use, contributions welcome!',
            '预设、自定义、番茄钟，多种模式满足你的不同时间管理需求。': 'Preset, custom, pomodoro - multiple modes to meet your time management needs.',
            '像幽灵猫猫一样悬浮，不挡视线，不影响操作，融入桌面背景。': 'Float like a ghost cat, not blocking your view or operations, blending with your desktop.',
            '字体、颜色、布局随心搭配，打造专属于你的可爱计时器。': 'Customize fonts, colors, and layout to create your own cute timer.',
            '时间到！自动执行任务，如提醒、锁屏、关机等，省心省力。': 'Time\'s up! Automatic tasks like reminders, screen lock, shutdown, etc. - hassle-free.',
            'C 语言编写，小巧玲珑，资源占用低，运行顺畅不卡顿。': 'Written in C, lightweight with low resource usage, runs smoothly without lag.',
            
            // 社区区域
            '开源社区认可 ⭐': 'Community Recognition ⭐',
            '获得超过 1.4k 的 GitHub 星标，持续增长中': 'Over 1.4k GitHub stars and growing',
            '获得超过 2k 的 GitHub 星标，持续增长中': 'Over 2k GitHub stars and growing',
            
            // 使用场景区域
            '使用场景 🌟': 'Use Cases 🌟',
            'Catime，各种场景的得力助手': 'Catime, your assistant in various scenarios',
            '游戏场景': 'Gaming',
            '自动打开软件': 'Auto Launch Apps',
            'PPT演示时使用': 'Presentations',
            '在游戏中设置计时器，随时关注休息时间。完全不会影响游戏操作，透明悬浮在游戏界面上，让你掌控游戏时间，避免沉迷过度。': 'Set a timer while gaming to track your break time. It won\'t affect gameplay, floating transparently on screen, helping you control gaming sessions and avoid excessive play.',
            '设置完成后自动打开指定程序，告别传统闹钟的重复确认。无论是提醒你查看邮件、打开会议软件，还是打开你最喜欢的应用，一切都能自动完成。': 'Automatically open specified programs when time\'s up, no more repeated alarm confirmations. Whether reminding you to check emails, opening meeting software, or launching your favorite apps - everything happens automatically.',
            '演讲和演示时的最佳拍档，透明悬浮在PPT上不影响观众视线，帮助你精确控制演讲时间，演示从此更加从容自信，不再担心超时。': 'The perfect companion for presentations, floating transparently on your slides without blocking the audience\'s view. Helps you precisely control your speaking time, making presentations more confident and worry-free.',
            
            // 号召性区域
            '立刻下载，开启可爱又高效的专注旅程！': 'Download now and start your cute & efficient focus journey!',
            '免费带走 Catime': 'Get Catime for Free',
            
            // 特别感谢区域
            '特别感谢 🙏': 'Special Thanks 🙏',
            '特别感谢以下大佬对于本项目的推荐': 'Special thanks to the following creators for recommending this project',
            '（按时间排序）': '(chronological order)',
            
            // 贡献者区域
            '致谢贡献者 🙏': 'Contributors 🙏',
            '感谢所有为 Catime 做出贡献的小伙伴们！': 'Thanks to everyone who contributed to Catime!',
            '（按时间排序）': '(chronological order)',
            
            // 动态生成的文本
            '准备好和 Catime 一起管理时间了吗？': 'Ready to manage time with Catime?',
            
            // 滚动指示器
            '返回顶部': 'Back to Top',
            
            // 页脚区域
            '基于': 'Released under',
            '许可开源': 'license',
            '图标画师:': 'Icon Artist:',
            '问题反馈': 'Feedback',
            '隐私政策': 'Privacy Policy'
        };
        
        // 遍历所有文本节点进行翻译
        translateTextNodes(document.body, translations);
        
        // 处理特殊情况：场景描述区域的字符级span
        handleScenarioDescriptions();
        
        // 处理页脚文本
        handleFooterTranslation();
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
            
            // 不翻译某些特定元素
            if (element.classList && (element.classList.contains('contributor-particles') || 
                element.classList.contains('scroll-progress-stars') ||
                element.classList.contains('scroll-progress-emoji-container'))) {
                return;
            }
            
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
    
    // 特殊处理场景描述（单个字符分割的span）
    function handleScenarioDescriptions() {
        // 获取当前语言
        const lang = localStorage.getItem('catime-language') || 'zh';
        
        // 如果不是英文，不进行替换
        if (lang !== 'en') return;
        
        const gamingScenario = document.querySelector('#scenario-gaming .scenario-content p');
        const autoopenScenario = document.querySelector('#scenario-autoopen .scenario-content p');
        const presentationScenario = document.querySelector('#scenario-presentation .scenario-content p');
        
        if (gamingScenario) {
            // 设置游戏场景描述的英文
            const gamingText = 'Set a timer while gaming to track your break time. It won\'t affect gameplay, floating transparently on screen, helping you control gaming sessions and avoid excessive play.';
            setSpanContent(gamingScenario, gamingText);
            
            // 添加英文版的特定CSS类以确保正确显示
            gamingScenario.classList.add('english-description');
        }
        
        if (autoopenScenario) {
            // 设置自动打开软件场景描述的英文
            const autoopenText = 'Automatically open specified programs when time\'s up, no more repeated alarm confirmations. Whether reminding you to check emails, opening meeting software, or launching your favorite apps - everything happens automatically.';
            setSpanContent(autoopenScenario, autoopenText);
            
            // 添加英文版的特定CSS类以确保正确显示
            autoopenScenario.classList.add('english-description');
        }
        
        if (presentationScenario) {
            // 设置PPT演示场景描述的英文
            const presentationText = 'The perfect companion for presentations, floating transparently on your slides without blocking the audience\'s view. Helps you precisely control your speaking time, making presentations more confident and worry-free.';
            setSpanContent(presentationScenario, presentationText);
            
            // 添加英文版的特定CSS类以确保正确显示
            presentationScenario.classList.add('english-description');
        }
        
        // 添加特定于英文描述的CSS样式规则，确保正确显示
        if (!document.getElementById('english-description-styles')) {
            const styleElement = document.createElement('style');
            styleElement.id = 'english-description-styles';
            styleElement.textContent = `
                .english-description {
                    letter-spacing: 0 !important;
                    line-height: 1.8 !important;
                }
                .english-description span {
                    display: inline-block !important;
                }
                .english-description span.space {
                    width: 0.3em !important;
                    margin-right: 0.05em !important;
                }
                .english-description:hover span {
                    animation: letterWave 2s ease-in-out infinite !important;
                }
                .english-description:hover span:nth-child(2n) {
                    animation-delay: 0.1s !important;
                }
                .english-description:hover span:nth-child(3n) {
                    animation-delay: 0.2s !important;
                }
                .english-description:hover span:nth-child(4n) {
                    animation-delay: 0.3s !important;
                }
            `;
            document.head.appendChild(styleElement);
        }
    }
    
    // 设置包含多个span的段落内容
    function setSpanContent(element, text) {
        // 先清空现有内容
        while (element.firstChild) {
            element.removeChild(element.firstChild);
        }
        
        // 将新文本拆分为字符并创建span
        for (let i = 0; i < text.length; i++) {
            const span = document.createElement('span');
            
            // 英文文本中的空格需要特殊处理，保持空格宽度
            if (text[i] === ' ') {
                span.innerHTML = '&nbsp;';
                span.className = 'space'; // 添加类名以便CSS处理
                span.style.marginRight = '0.25em'; // 为空格添加额外的右侧间距
            } else {
                span.textContent = text[i];
            }
            
            element.appendChild(span);
        }
    }
    
    // 特殊处理页脚的翻译
    function handleFooterTranslation() {
        // 获取当前语言
        const lang = localStorage.getItem('catime-language') || 'zh';
        
        // 如果不是英文，不进行替换
        if (lang !== 'en') return;
        
        const footerCopyright = document.querySelector('.main-footer p:first-child');
        const footerLicense = document.querySelector('.main-footer p:nth-child(2)');
        const footerArtist = document.querySelector('.main-footer p:nth-child(3)');
        
        if (footerCopyright) {
            footerCopyright.innerHTML = '&copy; 2025 Catime Project by <a href="https://vladelaina.com/" target="_blank" rel="noopener noreferrer">vladelaina</a>';
        }
        
        if (footerLicense) {
            footerLicense.innerHTML = 'Released under <a href="https://github.com/vladelaina/Catime/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">Apache 2.0</a> license';
        }
        
        if (footerArtist) {
            footerArtist.innerHTML = 'Icon Artist: <a href="https://space.bilibili.com/26087398" target="_blank" rel="noopener noreferrer">猫屋敷梨梨Official</a>';
        }
    }

    // 监听组件加载完成事件
    document.addEventListener('allComponentsLoaded', function() {
        console.log('📄 检测到组件加载完成，执行页脚翻译');
        handleFooterTranslation();
    });

    // 处理CTA区域的波浪字母
    function handleWaveLetters() {
        const ctaTitle = document.getElementById('cta-title');
        if (!ctaTitle) return;
        
        // 清空现有内容
        while (ctaTitle.firstChild) {
            ctaTitle.removeChild(ctaTitle.firstChild);
        }
        
        // 英文文本
        const englishText = 'Ready to manage time with Catime?';
        
        // 遍历每个字符，创建带wave-letter类的span
        for (let i = 0; i < englishText.length; i++) {
            const span = document.createElement('span');
            span.className = 'wave-letter';
            span.textContent = englishText[i];
            ctaTitle.appendChild(span);
        }
    }

    // 处理hero描述区域
    function handleHeroDescription() {
        // 获取当前语言
        const lang = localStorage.getItem('catime-language') || 'zh';
        
        // 如果不是英文，不进行替换
        if (lang !== 'en') return;
        
        const heroDescription = document.querySelector('.hero-description');
        const heroSubtitle = document.querySelector('.hero-subtitle');
        
        if (heroSubtitle) {
            heroSubtitle.innerHTML = 'Time management, just got super kawaii! <span class="cute-emoji">⊂(￣▽￣)⊃</span>';
        }
        
        if (heroDescription) {
            // 清空现有内容
            while (heroDescription.firstChild) {
                heroDescription.removeChild(heroDescription.firstChild);
            }
            
            // 第一行英文内容
            const line1Words = [
                'A', 'transparent', 'timer', '&', 'pomodoro', 'clock', 
                'for', 'Windows', '(Mac', 'coming', 'soon~)', '.'
            ];
            
            // 添加第一行单词
            line1Words.forEach(word => {
                const span = document.createElement('span');
                span.className = 'desc-word';
                span.textContent = word;
                heroDescription.appendChild(span);
            });
            
            // 添加换行
            heroDescription.appendChild(document.createElement('br'));
            
            // 第二行英文内容
            const line2Words = [
                { text: 'Float', class: 'cat-word' },
                { text: 'like', class: 'cat-word' },
                { text: 'a', class: 'cat-word' },
                { text: 'cat', class: 'cat-word' },
                { text: ',', class: 'cat-word' },
                { text: 'stay', class: '' },
                { text: 'focused', class: '' },
                { text: ',', class: '' },
                { text: 'boost', class: 'emp-word' },
                { text: 'your', class: 'emp-word' },
                { text: 'efficiency', class: 'emp-word' },
                { text: '!', class: 'emp-word' }
            ];
            
            // 添加第二行单词并添加特殊类
            line2Words.forEach(item => {
                const span = document.createElement('span');
                span.className = 'desc-word';
                
                // 添加额外的类
                if (item.class) {
                    span.classList.add(item.class);
                }
                
                span.textContent = item.text;
                heroDescription.appendChild(span);
            });
            
            // 添加双语提示(小标签)
            const bilingualHint = document.createElement('div');
            bilingualHint.className = 'bilingual-hint';
            bilingualHint.textContent = 'EN';
            
            heroDescription.appendChild(bilingualHint);
        }
    }
}
