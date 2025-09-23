// 支持项目的3D交互效果
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
                qrCode.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                label.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
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

 