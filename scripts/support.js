// 支持项目的3D交互效果
// State for support metrics animation
let SUPPORT_METRICS = { total: 0, totalCNY: 0, count: 0, animated: false };

// Currency state for dynamic conversion
const CURRENCY_STATE = {
    target: 'CNY', // 'CNY' | 'USD'
    symbol: '¥',
    rateCnyToUsd: 0, // exchange rate CNY -> USD
    lastUpdated: 0
};

// Initialize currency based on current language and fetch rate if needed
function initCurrency() {
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    const isEnglish = currentLang === 'en';
    CURRENCY_STATE.target = isEnglish ? 'USD' : 'CNY';
    CURRENCY_STATE.symbol = isEnglish ? '$' : '¥';

    if (isEnglish) {
        // Load cached rate first (if fresh), then fetch latest in background
        const cached = getCachedRateSync();
        if (cached > 0) {
            CURRENCY_STATE.rateCnyToUsd = cached;
        }
        loadExchangeRateCnyToUsd()
            .then((rate) => {
                if (rate > 0) {
                    CURRENCY_STATE.rateCnyToUsd = rate;
                    CURRENCY_STATE.lastUpdated = Date.now();
                    // Update display only after animation played, otherwise keep 0->target animation intact
                    if (SUPPORT_METRICS.animated) {
                        renderSupportTotalImmediate();
                    }
                }
            })
            .catch(() => {
                // Keep cached rate if available; otherwise do nothing
            });
    }
}

// Read cached rate from localStorage synchronously
function getCachedRateSync() {
    try {
        const raw = localStorage.getItem('catime_rate_cny_usd');
        if (!raw) return 0;
        const obj = JSON.parse(raw);
        // TTL: 1 hour
        const ttl = 60 * 60 * 1000;
        if (obj && obj.rate && obj.ts && (Date.now() - obj.ts) < ttl) {
            return Number(obj.rate) || 0;
        }
    } catch (e) {
        // ignore
    }
    return 0;
}

// Persist rate to localStorage
function cacheRate(rate) {
    try {
        localStorage.setItem('catime_rate_cny_usd', JSON.stringify({ rate, ts: Date.now() }));
    } catch (e) {
        // ignore
    }
}

// Fetch CNY->USD exchange rate with fallbacks
function loadExchangeRateCnyToUsd() {
    // Try cache freshness first; if fresh, still fetch latest in background
    const endpoints = [
        'https://api.exchangerate.host/latest?base=CNY&symbols=USD',
        'https://open.er-api.com/v6/latest/CNY',
        'https://api.frankfurter.app/latest?from=CNY&to=USD'
    ];

    function tryNext(index) {
        if (index >= endpoints.length) {
            return Promise.reject(new Error('No exchange endpoint available'));
        }
        const url = endpoints[index];
        return fetch(url, { cache: 'no-store' })
            .then(r => r.json())
            .then(j => {
                let rate = 0;
                if (j && j.rates && typeof j.rates.USD === 'number') {
                    rate = j.rates.USD;
                } else if (j && j.result === 'success' && j.rates && typeof j.rates.USD === 'number') {
                    rate = j.rates.USD;
                }
                // Frankfurter format also uses j.rates.USD
                if (rate > 0) {
                    cacheRate(rate);
                    return rate;
                }
                return tryNext(index + 1);
            })
            .catch(() => tryNext(index + 1));
    }

    return tryNext(0);
}

// Compute display total based on current currency
function getDisplayTotal(totalCNY) {
    if (CURRENCY_STATE.target === 'USD') {
        const rate = CURRENCY_STATE.rateCnyToUsd || getCachedRateSync();
        if (rate > 0) return totalCNY * rate;
        // Fallback approximate if no rate yet; will be corrected once fetched
        return totalCNY * 0.14;
    }
    return totalCNY;
}

// Format currency string by current target
function formatCurrency(value) {
    const lang = CURRENCY_STATE.target === 'USD' ? 'en-US' : 'zh-CN';
    const amount = Number(value || 0);
    const formatted = amount.toLocaleString(lang, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    return `${CURRENCY_STATE.symbol}${formatted}`;
}

// Re-render total immediately with current settings (no animation)
function renderSupportTotalImmediate() {
    const totalEl = document.getElementById('support-total-value');
    if (!totalEl) return;
    const cny = SUPPORT_METRICS.totalCNY || SUPPORT_METRICS.total || 0;
    totalEl.textContent = formatCurrency(getDisplayTotal(cny));
}

document.addEventListener('DOMContentLoaded', function() {
    // AOS (滚动时动画) 库初始化
    AOS.init({
        duration: 800, // 动画持续时间
        once: true, // 动画是否只发生一次 - 在向下滚动时
        offset: 50, // 从原始触发点偏移的距离 (以像素为单位)
    });

    // 初始化滚动进度指示器
    initScrollProgressIndicator();

    // 为支持项目卡添加3D交互效果
    const supportMethods = document.querySelectorAll('.support-method');
    const isMobile = window.innerWidth <= 768; // 检测是否是移动设备
    
    supportMethods.forEach(card => {
        let rect = card.getBoundingClientRect();
        let centerX = (rect.left + rect.right) / 2;
        let centerY = (rect.top + rect.bottom) / 2;
        
        // 鼠标移动事件处理
        card.addEventListener('mousemove', function(e) {
            if (isMobile) return; // 在移动设备上不执行这些特效
            
            rect = card.getBoundingClientRect();
            centerX = (rect.left + rect.right) / 2;
            centerY = (rect.top + rect.bottom) / 2;
            
            // 计算鼠标与卡片中心的相对距离（-1到1的范围）
            const relativeX = (e.clientX - centerX) / (rect.width / 2);
            const relativeY = (e.clientY - centerY) / (rect.height / 2);
            
            // 应用旋转效果，最大±10度
            card.style.transform = `translateY(-20px) scale(1.03) rotateX(${-relativeY * 10}deg) rotateY(${relativeX * 10}deg)`;
            
            // 获取QR码和标签元素
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                // QR码沿Z轴移动更多，创造深度感
                qrCode.style.transform = `translateZ(40px) scale(1.08) rotate(${relativeX * 2}deg)`;
            }
            
            if (label) {
                // 标签轻微偏移跟随鼠标
                label.style.transform = `translateZ(25px) translateY(-5px) translateX(${relativeX * 5}px) scale(1.05)`;
            }
        });
        
        // 鼠标离开时重置效果
        card.addEventListener('mouseleave', function() {
            if (isMobile) return; // 在移动设备上不执行这些特效
            
            card.style.transform = '';
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transform = 'translateZ(20px)';
            }
            
            if (label) {
                label.style.transform = 'translateZ(10px)';
            }
            
            // 使用setTimeout添加过渡回原位的动画
            setTimeout(() => {
                card.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                if (qrCode) qrCode.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                if (label) label.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
            }, 50);
        });
        
        // 鼠标进入时移除过渡效果，使移动更流畅
        card.addEventListener('mouseenter', function() {
            if (isMobile) return; // 在移动设备上不执行这些特效
            
            card.style.transition = 'none';
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transition = 'none';
            }
            
            if (label) {
                label.style.transition = 'none';
            }
        });
    });

    // 为其他支持卡片添加悬停效果
    const supportCards = document.querySelectorAll('.support-card');
    supportCards.forEach(card => {
        card.addEventListener('mouseenter', function() {
            const icon = this.querySelector('.support-icon i');
            if (icon) {
                icon.style.transition = 'transform 0.3s ease';
                icon.style.transform = 'scale(1.2)';
            }
        });
        
        card.addEventListener('mouseleave', function() {
            const icon = this.querySelector('.support-icon i');
            if (icon) {
                icon.style.transform = 'scale(1)';
            }
        });
    });

    // 添加心形动画交互
    const heartAnimation = document.querySelector('.heart-animation');
    if (heartAnimation) {
        heartAnimation.addEventListener('click', function() {
            const heart = this.querySelector('i');
            heart.style.animation = 'none';
            setTimeout(() => {
                heart.style.animation = 'heartbeat 1.5s infinite';
                this.classList.add('clicked');
                setTimeout(() => {
                    this.classList.remove('clicked');
                }, 1000);
            }, 10);
        });
    }

    // 初始化咖啡粒子效果
    initCoffeeParticles();
    
    // 添加支持页面的翻译
    addSupportTranslations();

    // 初始化货币（根据语言设置）并预取汇率
    initCurrency();

    // 计算并更新累计赞助
    updateSupportTotal();
    // 统计支持者人数
    updateSupportCount();
    // 为胶囊添加亮片绽放交互
    initCapsuleSparkles();
    // 为胶囊添加点击彩带效果
    initCapsuleConfetti();
    // 在胶囊可见时再触发数字动画
    initCapsuleNumberObserver();

    // 语言切换按钮功能
    const languageToggle = document.getElementById('language-toggle');
    if (languageToggle) {
        const currentLang = localStorage.getItem('catime-language') || 'zh';
        
        // 根据当前语言设置按钮文本
        if (currentLang === 'zh') {
            languageToggle.innerHTML = '<i class="fas fa-language"></i> English';
        } else {
            languageToggle.innerHTML = '<i class="fas fa-language"></i> 中文';
        }
        
        // 点击切换语言
        languageToggle.addEventListener('click', function(e) {
            e.preventDefault();
            
            const newLang = currentLang === 'zh' ? 'en' : 'zh';
            localStorage.setItem('catime-language', newLang);
            
            // 重新加载页面应用新的语言
            window.location.reload();
        });
    }
});

// 初始化咖啡粒子效果
function initCoffeeParticles() {
    const coffeeParticlesContainer = document.querySelector('.coffee-particles');
    if (!coffeeParticlesContainer) return;

    const coffeeParticles = coffeeParticlesContainer.querySelectorAll('.coffee-particle');
    
    // 为每个咖啡粒子分配随机的动画延迟和位置
    coffeeParticles.forEach(particle => {
        const delay = Math.random() * 5; // 0-5秒的随机延迟
        const duration = 15 + Math.random() * 10; // 15-25秒的随机持续时间
        const left = Math.random() * 100; // 0-100%的随机水平位置

        particle.style.animationDelay = `${delay}s`;
        particle.style.animationDuration = `${duration}s`;
        particle.style.left = `${left}%`;
    });
}

// 初始化滚动进度指示器
function initScrollProgressIndicator() {
    const scrollProgressContainer = document.getElementById('scrollProgressContainer');
    const scrollProgressCircle = document.querySelector('.scroll-progress-circle-fill');
    const scrollProgressPercentage = document.querySelector('.scroll-progress-percentage');

    if (!scrollProgressContainer || !scrollProgressCircle || !scrollProgressPercentage) return;

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

// 添加支持页面的翻译
function addSupportTranslations() {
    // 检查当前语言设置
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    // 应用按钮样式，无论是哪种语言
    fixButtonPositions();
    
    // 如果当前语言是英文，则添加英文翻译
    if (currentLang === 'en') {
        // 翻译页面标题
        const pageTitle = document.getElementById('page-title');
        if (pageTitle) {
            pageTitle.textContent = 'Catime - Support the Project';
        }
        
        // 翻译meta描述
        const metaDescription = document.getElementById('meta-description');
        if (metaDescription) {
            metaDescription.setAttribute('content', 'Support Catime Project - A minimalist, modern, efficient transparent timer and pomodoro clock for Windows, with a cute style.');
        }
        
        // 翻译导航链接
        document.querySelectorAll('.nav-links li a').forEach(link => {
            if (link.textContent === '首页') link.textContent = 'Home';
            if (link.textContent === '指南') link.textContent = 'Guide';
            if (link.textContent === '关于') link.textContent = 'About';
            if (link.querySelector('span') && link.querySelector('span').textContent === '下载') {
                link.querySelector('span').textContent = 'Download';
            }
        });
        
        // 翻译页面标题区域
        const pageHeader = document.querySelector('.page-header h1');
        if (pageHeader) {
            pageHeader.textContent = 'Support the Project';
        }
        
        const pageHeaderSubtitle = document.querySelector('.page-header p');
        if (pageHeaderSubtitle) {
            pageHeaderSubtitle.textContent = 'Your support is our motivation to continuously develop and improve Catime';
        }
        
        // 翻译主要内容
        translateSupportElements();
        
        // 确保所有按钮可见
        fixButtonVisibility();
        

    }
}

// 确保所有支持按钮位置固定，适用于中英文界面
function fixButtonPositions() {
    setTimeout(() => {
        // 固定按钮位置
        document.querySelectorAll('.support-card .support-btn').forEach(btn => {
            btn.style.position = 'absolute';
            btn.style.bottom = window.innerWidth <= 480 ? '2rem' : '2.5rem';
            btn.style.left = '50%';
            btn.style.transform = 'translateX(-50%)';
            btn.style.width = '200px';
            btn.style.margin = '0';
        });
    }, 100);
}

// 确保所有支持按钮可见
function fixButtonVisibility() {
    // 确保所有按钮都是可见的
    document.querySelectorAll('.support-card .support-btn').forEach(btn => {
        btn.style.display = 'flex';
        btn.style.visibility = 'visible';
        btn.style.opacity = '1';
    });
    
    // 特别检查提交Issues按钮
    const issuesBtn = document.querySelector('.support-card:nth-child(2) .support-btn');
    if (issuesBtn) {
        issuesBtn.style.display = 'flex';
        issuesBtn.style.visibility = 'visible';
        issuesBtn.style.opacity = '1';
        
        // 确保按钮内容正确显示
        if (issuesBtn.querySelector('i')) {
            issuesBtn.querySelector('i').style.display = 'inline-block';
        }
    }
}

// 翻译support页面内容元素
function translateSupportElements() {
    // 翻译section标题和内容
    document.querySelectorAll('.section-title').forEach(title => {
        if (title.innerHTML.includes('支持项目')) {
            title.innerHTML = 'Support the Project <i class="fas fa-mug-hot"></i>';
        }
        if (title.innerHTML.includes('其他支持方式')) {
            title.innerHTML = 'Other Ways to Support <i class="fas fa-gift"></i>';
        }
        if (title.innerHTML.includes('感谢支持者')) {
            title.innerHTML = 'Thanks to Supporters';
        }
    });
    
    // 翻译支持项目说明
    const projectDesc = document.querySelector('.support-project .section-subtitle');
    if (projectDesc) {
        projectDesc.innerHTML = 'Catime will continue to be open-source and free to use forever.<br>' +
            'But its development and maintenance require a lot of time and energy.<br>' +
            'If you find Catime helpful, consider buying the author a coffee,<br>' +
            'to fuel this passion ❤️‍🔥<br>' +
            'Every bit of your support is a powerful drive to keep it moving forward!';
    }
    
    // 翻译支付方式标签
    document.querySelectorAll('.support-label').forEach(label => {
        if (label.textContent.includes('微信')) {
            label.innerHTML = '<i class="fab fa-weixin"></i> WeChat';
        }
        if (label.textContent.includes('支付宝')) {
            label.innerHTML = '<i class="fab fa-alipay"></i> Alipay';
        }
    });
    
    // 翻译其他支持卡片内容
    const supportCards = document.querySelectorAll('.support-card');
    supportCards.forEach(card => {
        const title = card.querySelector('h3');
        const desc = card.querySelector('p');
        const btn = card.querySelector('.support-btn');
        
        if (title && title.textContent === 'Star 项目') {
            title.textContent = 'Star Project';
            desc.textContent = 'If you like Catime, please give us a Star on GitHub. It\'s the best encouragement for us!';
            if (btn) btn.innerHTML = '<i class="fab fa-github"></i> Star Project';
        }
        
        if (title && title.textContent === '提交Issues') {
            title.textContent = 'Submit Issues';
            desc.textContent = 'Found a bug or have feature suggestions? Welcome to submit Issues on GitHub to help us continuously improve Catime!';
            if (btn) {
                btn.innerHTML = '<i class="fas fa-exclamation-circle"></i> Submit Issues';
                btn.style.display = 'flex';
                btn.style.visibility = 'visible';
                btn.style.opacity = '1';
            }
        }
        
        if (title && title.textContent === '分享推广') {
            title.textContent = 'Share & Promote';
            desc.textContent = 'Share Catime with your friends, colleagues, or on social media to help more people discover this tool!';
            if (btn) {
                btn.innerHTML = '<i class="fas fa-users"></i> Join Discord';
                btn.href = 'https://discord.com/invite/W3tW2gtp6g';
            }
        }
    });
    
        // 翻译感谢支持者部分
    const supportersDesc = document.querySelector('.supporters .section-subtitle');
    if (supportersDesc) {
        supportersDesc.textContent = 'Special thanks to those who have supported the Catime project! Your encouragement is our motivation to move forward.';
    }

        // 翻译累计赞助标签
        const totalLabel = document.querySelector('.support-total-label');
        if (totalLabel) {
            totalLabel.innerHTML = '<i class="fas fa-coins"></i> Total Donations';
        }

        const countLabel = document.querySelector('.support-count-label');
        if (countLabel) {
            countLabel.innerHTML = '<i class="fas fa-user-friends"></i> Supporters';
        }
    
    // 翻译表格头部
    const tableHeaders = document.querySelectorAll('.supporters-table th');
    if (tableHeaders.length >= 4) {
        tableHeaders[0].textContent = 'Time';
        tableHeaders[1].textContent = 'Username';
        tableHeaders[2].textContent = 'Amount';
        tableHeaders[3].textContent = 'Message';
    }
    
    // 翻译表格内容中的留言（这里只翻译重复出现的内容）
    document.querySelectorAll('.supporters-table td').forEach(td => {
        if (td.textContent === '温州市宇波机车部件有限公司') {
            td.textContent = 'Wenzhou Yubo Locomotive Parts CO.,LTD.';
        }
        if (td.textContent === '加油') {
            td.textContent = 'Keep going';
        }
        if (td.textContent === '加油啊，你可以的') {
            td.textContent = 'Come on, you can do it!';
        }
        if (td.textContent === '加油，你可以的🫡') {
            td.textContent = 'Keep going, you can do it! 🫡';
        }
        if (td.textContent === '好用，爱用，希望增加个鼠标悬停时隐藏时钟的功能') {
            td.textContent = 'Love it! Hope to add a feature to hide the clock when hovering with mouse';
        }
        if (td.textContent === '催更催更😏') {
            td.textContent = 'Push for updates😏';
        }
        if (td.textContent === '番茄钟超赞，期待继续优化') {
            td.textContent = 'Pomodoro timer is great, looking forward to further optimization';
        }
        if (td.textContent === '很棒的项目！') {
            td.textContent = 'Great project!';
        }
        if (td.textContent === '恭喜') {
            td.textContent = 'Congratulations';
        }
        if (td.textContent === '感谢Catime，希望你也能多爱自己，未来可期') {
            td.textContent = 'Thank you Catime, hope you also love yourself more, the future is promising';
        }
        if (td.textContent === '建议catime加个倒计时列表功能') {
            td.textContent = 'Suggest adding a countdown list feature to catime';
        }
        if (td.textContent === '赞助了一年的域名 vladelaina.com') {
            td.textContent = 'Sponsored one year of domain vladelaina.com';
        }
        if (td.textContent === '打赏catime') {
            td.textContent = 'Tipping catime';
        }
        if (td.textContent === 'catime打赏') {
            td.textContent = 'Tipping catime';
        }
        if (td.textContent === '软件好用，赞赞赞') {
            td.textContent = 'The software is great, praise!';
        }
        if (td.textContent === '极简，可爱，好用，喜欢') {
            td.textContent = 'Minimalist, cute, useful, love it';
        }
        if (td.textContent === '不错不错，实用鼓励一下') {
            td.textContent = 'Not bad, very practical, a little encouragement';
        }
        if (td.textContent === '在任务栏里吃灰叭（bushi）好用👍') {
            td.textContent = 'Let it collect dust in the taskbar (just kidding) It\'s great👍';
        }
        if (td.textContent === '学生党，1块冲你和我一样喜欢蕾娜和伊蕾娜，1块冲你的产品确实挺好') {
            td.textContent = 'As a student, ¥1 because you like Laina and Elaina like me, another ¥1 because your product is really good';
        }
        if (td.textContent === '我是最早提出来让你弄这个二维码，我们粉丝可以赞助') {
            td.textContent = 'I was the first to suggest setting up these QR codes so we fans could support you';
        }
        if (td.textContent === '不多感谢你的catime，让我下班有了倒计时盼头🤧') {
            td.textContent = 'Just wanted to thank you for catime, it gives me something to look forward to counting down to when getting off work🤧';
        }
        if (td.textContent === '为爱发电') {
            td.textContent = 'Powered by love';
        }
        if (td.textContent === 'catime小小支持') {
            td.textContent = 'A little support for catime';
        }
        if (td.textContent === '支持一下') {
            td.textContent = 'Just a little support';
        }
        if (td.textContent === '茉莉蜜茶行不行') {
            td.textContent = 'How about jasmine honey tea';
        }
        if (td.textContent === '好喜欢catime，请你喝奶茶') {
            td.textContent = 'I really like catime, please have some milk tea';
        }
        if (td.textContent === '时钟很不错，帮助页面也很漂亮 支持~*.。(๑･∀･๑)*.。') {
            td.textContent = 'The timer is great, and the help page is beautiful. Support~*.。(๑･∀･๑)*.。';
        }
        if (td.textContent === 'bro好用狒狒防沉迷组件了已经是') {
            td.textContent = 'Bro, it\'s so useful! Already has anti-addiction component';
        }
        if (td.textContent === '非常好的工具，谢谢') {
            td.textContent = 'Very good tool, thank you';
        }
        if (td.textContent === '功能简洁明了，谢谢你，cat') {
            td.textContent = 'Simple and clear features, thank you, cat';
        }
        if (td.textContent === '好用，respect') {
            td.textContent = 'Useful, respect';
        }
        if (td.textContent === '感谢 catime，好用。') {
            td.textContent = 'Thanks to Catime, easy to use.';
        }
        if (td.textContent === '入股入股，付费使用来自律') {
            td.textContent = 'Investing, paying for self-discipline.';
        }
        if (td.textContent === '非常好小程序') {
            td.textContent = 'Very good app';
        }
        
        // 为所有留言单元格添加特殊样式，增强可读性
        if (td.parentElement && td.cellIndex === 3) { // 第四列是留言列
            td.style.maxWidth = '250px';
            td.style.wordWrap = 'break-word';
            td.style.whiteSpace = 'normal';
            
            // 检查是否为英文模式，如果是，增加字体大小提高可读性
            if (localStorage.getItem('catime-language') === 'en') {
                td.style.fontSize = '0.9rem';
                td.style.lineHeight = '1.4';
            }
        }
    });
}

// 汇总表格金额并更新到累计赞助展示
function updateSupportTotal() {
    const table = document.querySelector('.supporters-table');
    const totalEl = document.getElementById('support-total-value');
    if (!table || !totalEl) return;

    let sum = 0;
    // 遍历表体的每一行的“金额”列（第3列）
    table.querySelectorAll('tbody tr').forEach(row => {
        const amountCell = row.cells && row.cells[2];
        if (!amountCell) return;
        const text = amountCell.textContent.trim();
        // 支持形如 ¥66.66、¥2.8、¥1、含空格等
        const match = text.replace(/[,\s]/g, '').match(/([\-\+]?)¥?([0-9]+(?:\.[0-9]+)?)/);
        if (match) {
            const sign = match[1] === '-' ? -1 : 1;
            const value = parseFloat(match[2]);
            if (!isNaN(value)) {
                sum += sign * value;
            }
        }
    });

    // Cache in CNY and compute display total for current currency
    SUPPORT_METRICS.totalCNY = sum;
    SUPPORT_METRICS.total = getDisplayTotal(sum);
    // If already animated once, update text immediately in current currency
    if (SUPPORT_METRICS.animated) {
        totalEl.textContent = formatCurrency(SUPPORT_METRICS.total);
    }
}

// 统计支持者人数
function updateSupportCount() {
    const tbody = document.querySelector('.supporters-table tbody');
    const countEl = document.getElementById('support-count-value');
    if (!tbody || !countEl) return;

    // 按用户名去重统计，忽略空白与大小写
    const normalize = (s) => (s || '').replace(/\s+/g, '').toLowerCase();
    const nameSet = new Set();
    Array.from(tbody.querySelectorAll('tr')).forEach(tr => {
        if (!tr.cells || tr.cells.length < 2) return;
        const nameText = tr.cells[1].textContent.trim();
        if (!nameText) return;
        nameSet.add(normalize(nameText));
    });
    const newCount = nameSet.size;
    SUPPORT_METRICS.count = newCount;
    if (SUPPORT_METRICS.animated) {
        countEl.textContent = String(newCount);
    }
}

// 胶囊悬停时的亮片粒子效果
function initCapsuleSparkles() {
    const capsule = document.querySelector('.support-total-top');
    if (!capsule) return;

    // 如果用户偏好减少动效，直接返回
    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    if (media.matches) return;

    capsule.addEventListener('mouseenter', spawnSparkles);
    capsule.addEventListener('focus', spawnSparkles);

    function spawnSparkles() {
        for (let i = 0; i < 10; i++) {
            const s = document.createElement('span');
            s.className = 'sparkle';
            // 随机起点在胶囊中部
            s.style.left = `${50 + (Math.random() * 40 - 20)}%`;
            s.style.top = `${45 + (Math.random() * 20 - 10)}%`;
            // 随机延迟/大小
            s.style.setProperty('--d', `${Math.random() * 0.2 + 0.05}s`);
            s.style.setProperty('--tx', `${(Math.random() * 140 - 70)}%`);
            s.style.setProperty('--ty', `${(Math.random() * -120 - 20)}%`);
            capsule.appendChild(s);
            // 清理
            setTimeout(() => s.remove(), 700);
        }
    }
}

// 胶囊悬停/触发的彩带效果（带冷却）
function initCapsuleConfetti() {
    const capsule = document.querySelector('.support-total-top');
    if (!capsule) return;
    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    if (media.matches) return;

    const trigger = () => {
        launchOverlayConfetti(capsule);
    };

    capsule.addEventListener('mouseenter', trigger);
    capsule.addEventListener('focus', trigger);

    function randomConfettiColor() {
        const colors = ['#7aa2f7', '#f799b8', '#ffd45e', '#9ae6b4', '#fbd38d'];
        return colors[Math.floor(Math.random() * colors.length)];
    }
}

// 数字动画工具函数
function animateNumber(element, from, to, duration, formatter) {
    const start = performance.now();
    const ease = (t) => 1 - Math.pow(1 - t, 3); // easeOutCubic
    function frame(now) {
        const progress = Math.min(1, (now - start) / duration);
        const eased = ease(progress);
        const value = from + (to - from) * eased;
        element.textContent = formatter(value);
        if (progress < 1) {
            requestAnimationFrame(frame);
        }
    }
    requestAnimationFrame(frame);
}

// 观察胶囊是否进入视口，进入后再播放数字动画（只播放一次）
function initCapsuleNumberObserver() {
    const capsule = document.querySelector('.support-total-top');
    const totalEl = document.getElementById('support-total-value');
    const countEl = document.getElementById('support-count-value');
    if (!capsule || !totalEl || !countEl) return;

    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    const oncePlay = () => {
        if (SUPPORT_METRICS.animated) return;
        SUPPORT_METRICS.animated = true;
        // 播放累计赞助（按当前货币），强制从0开始
        const currentTotal = 0;
        const duration = media.matches ? 0 : 1800; // keep both in sync
        animateNumber(totalEl, currentTotal, SUPPORT_METRICS.total || 0, duration, (v) => formatCurrency(v));
        // 播放支持者人数
        const currentCount = parseFloat(countEl.textContent || '0') || 0;
        animateNumber(countEl, currentCount, SUPPORT_METRICS.count || 0, duration, (v) => `${Math.round(v)}`);
    };

    if ('IntersectionObserver' in window) {
        const io = new IntersectionObserver((entries) => {
            entries.forEach(entry => {
                if (entry.isIntersecting && entry.intersectionRatio > 0.25) {
                    oncePlay();
                    io.disconnect();
                }
            });
        }, { threshold: [0, 0.25, 0.5, 1] });
        io.observe(capsule);
    } else {
        // 回退：滚动时检测
        const onScroll = () => {
            const rect = capsule.getBoundingClientRect();
            if (rect.top < window.innerHeight * 0.75 && rect.bottom > 0) {
                oncePlay();
                window.removeEventListener('scroll', onScroll);
            }
        };
        window.addEventListener('scroll', onScroll);
        onScroll();
    }
}

// 更大面积的全屏彩带
function launchOverlayConfetti(anchor) {
    const overlay = document.createElement('div');
    overlay.className = 'confetti-overlay';
    document.body.appendChild(overlay);

    const rect = anchor.getBoundingClientRect();
    const originX = rect.left + rect.width / 2;
    const originY = rect.top + rect.height / 2;

    const colors = ['#7aa2f7', '#f799b8', '#ffd45e', '#9ae6b4', '#fbd38d'];
    const totalPieces = 40; // 更有庆祝感
    for (let i = 0; i < totalPieces; i++) {
        const piece = document.createElement('span');
        const isStreamer = Math.random() < 0.25;
        piece.className = isStreamer ? 'streamer' : 'confetti';
        piece.style.left = `${originX + (Math.random() * 80 - 40)}px`;
        piece.style.top = `${originY + (Math.random() * 20 - 10)}px`;
        piece.style.setProperty('--rot', `${Math.random() * 360}deg`);
        piece.style.setProperty('--dx', `${(Math.random() * 800 - 400)}px`);
        piece.style.setProperty('--dy', `${(Math.random() * 600 + 200)}px`);
        piece.style.setProperty('--dur', `${1.1 + Math.random() * 0.5}s`);
        piece.style.setProperty('--c', colors[Math.floor(Math.random() * colors.length)]);
        if (isStreamer) {
            piece.style.setProperty('--h', `${20 + Math.floor(Math.random() * 30)}px`);
        } else {
            piece.style.setProperty('--br', Math.random() < 0.5 ? '50%' : '2px');
        }
        overlay.appendChild(piece);
    }

    // 自动清理
    setTimeout(() => overlay.remove(), 1800);
}

 