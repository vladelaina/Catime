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
    
    // 初始化 Hero 区域的交互效果 (Catime 2.0)
    initHeroInteractions();
    
    // 初始化导航栏滚动效果 (Liquid Glass Toggle)
    initHeaderScroll();

    // 监听组件加载完成事件
    document.addEventListener('allComponentsLoaded', function() {
        console.log('📄 检测到组件加载完成');
    });

    // 处理CTA区域的波浪字母
    handleWaveLetters();
});

// 导航栏滚动效果
function initHeaderScroll() {
    const header = document.querySelector('.main-header');
    if (!header) return;

    // 检查初始位置
    if (window.scrollY > 50) {
        header.classList.add('scrolled');
    }

    window.addEventListener('scroll', () => {
        if (window.scrollY > 50) {
            header.classList.add('scrolled');
        } else {
            header.classList.remove('scrolled');
        }
    });
}

// Catime 2.0 - Hero 区域交互效果
function initHeroInteractions() {
    const hero = document.querySelector('.hero');
    if (!hero) return;

    const heroVisual = document.querySelector('.hero-visual-wrapper');
    const spotlight = document.querySelector('.hero-spotlight');
    const parallaxElements = document.querySelectorAll('[data-parallax-speed]');
    const magneticBtns = document.querySelectorAll('.btn-magnetic');

    // 鼠标移动事件监听
    hero.addEventListener('mousemove', (e) => {
        const rect = hero.getBoundingClientRect();
        const x = e.clientX - rect.left; // 鼠标在 hero 内的 x 坐标
        const y = e.clientY - rect.top;  // 鼠标在 hero 内的 y 坐标
        
        // 1. 聚光灯效果 (Spotlight)
        // 更新 CSS 变量，让径向渐变跟随鼠标
        hero.style.setProperty('--mouse-x', `${x}px`);
        hero.style.setProperty('--mouse-y', `${y}px`);

        // 计算相对中心的坐标 (-1 到 1)
        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const relativeX = (x - centerX) / centerX;
        const relativeY = (y - centerY) / centerY;

        // 2. 3D 窗口倾斜 (Window Tilt)
        if (heroVisual) {
            // 限制最大旋转角度
            const rotateY = relativeX * 5; // 左右移动导致绕 Y 轴旋转
            const rotateX = relativeY * -5; // 上下移动导致绕 X 轴旋转 (反向)
            
            // 应用变换
            heroVisual.style.transform = `
                perspective(1000px)
                rotateX(${rotateX}deg)
                rotateY(${rotateY}deg)
                scale(1.02)
            `;
        }

        // 3. 视差滚动 (Parallax)
        parallaxElements.forEach(el => {
            const speed = parseFloat(el.getAttribute('data-parallax-speed')) || 0.05;
            const moveX = -relativeX * speed * 100; // 移动距离
            const moveY = -relativeY * speed * 100;
            
            el.style.transform = `translate3d(${moveX}px, ${moveY}px, 0)`;
        });
    });

    // 鼠标离开时复位
    hero.addEventListener('mouseleave', () => {
        if (heroVisual) {
            heroVisual.style.transform = 'perspective(1000px) rotateX(5deg) rotateY(0deg) scale(1)';
        }
        
        parallaxElements.forEach(el => {
            el.style.transform = 'translate3d(0, 0, 0)';
        });
    });

    // 4. 磁吸按钮效果 (Magnetic Buttons)
    magneticBtns.forEach(btn => {
        btn.addEventListener('mousemove', (e) => {
            const rect = btn.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const y = e.clientY - rect.top;
            
            // 计算鼠标相对于按钮中心的偏移
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            
            // 磁吸强度
            const strength = 0.3;
            const deltaX = (x - centerX) * strength;
            const deltaY = (y - centerY) * strength;
            
            // 移动按钮内容 (Content) 和 背景 (Glow)
            // 按钮整体移动
            btn.style.transform = `translate(${deltaX}px, ${deltaY}px)`;
            
            // 让 Glow 跟随鼠标位置移动，创造光影效果
            const glow = btn.querySelector('.btn-glow');
            if (glow) {
                glow.style.transform = `translate(${deltaX * 0.5}px, ${deltaY * 0.5}px)`;
            }
        });

        btn.addEventListener('mouseleave', () => {
            // 复位
            btn.style.transform = 'translate(0, 0)';
            const glow = btn.querySelector('.btn-glow');
            if (glow) {
                glow.style.transform = 'translate(0, 0)';
            }
        });
    });
}

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

    // 统一给标记了 data-download 或 download-link 类名的链接赋值（兜底）
    document.querySelectorAll('a[data-download], a.download-link').forEach(a => {
        a.setAttribute('href', CATIME_CONFIG.DOWNLOAD_URL);
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

// 监听组件加载完成事件

    document.addEventListener('allComponentsLoaded', function() {
        console.log('📄 检测到组件加载完成');
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

// 处理hero描述区域 - 已移除 (Moved to static HTML)
// function handleHeroDescription() { ... }


