let SUPPORT_METRICS = { total: 0, totalCNY: 0, count: 0, animated: false };

const CURRENCY_STATE = {
    target: 'USD', 
    symbol: '$',
    rateCnyToUsd: 0, 
    lastUpdated: 0
};

function initCurrency() {
    // Default to USD/English
    const cached = getCachedRateSync();
    if (cached > 0) {
        CURRENCY_STATE.rateCnyToUsd = cached;
    }
    loadExchangeRateCnyToUsd()
        .then((rate) => {
            if (rate > 0) {
                CURRENCY_STATE.rateCnyToUsd = rate;
                CURRENCY_STATE.lastUpdated = Date.now();
                if (SUPPORT_METRICS.animated) {
                    renderSupportTotalImmediate();
                }
            }
        })
        .catch(() => {
        });
}

function getCachedRateSync() {
    try {
        const raw = localStorage.getItem('catime_rate_cny_usd');
        if (!raw) return 0;
        const obj = JSON.parse(raw);
        const ttl = 60 * 60 * 1000;
        if (obj && obj.rate && obj.ts && (Date.now() - obj.ts) < ttl) {
            return Number(obj.rate) || 0;
        }
    } catch (e) {
    }
    return 0;
}

function cacheRate(rate) {
    try {
        localStorage.setItem('catime_rate_cny_usd', JSON.stringify({ rate, ts: Date.now() }));
    } catch (e) {
    }
}

function loadExchangeRateCnyToUsd() {
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

function getDisplayTotal(totalCNY) {
    // Always convert to USD as target is USD
    const rate = CURRENCY_STATE.rateCnyToUsd || getCachedRateSync();
    if (rate > 0) return totalCNY * rate;
    return totalCNY * 0.14;
}

function formatCurrency(value) {
    const lang = 'en-US';
    const amount = Number(value || 0);
    const formatted = amount.toLocaleString(lang, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
    return `${CURRENCY_STATE.symbol}${formatted}`;
}

function renderSupportTotalImmediate() {
    const totalEl = document.getElementById('support-total-value');
    if (!totalEl) return;
    const cny = SUPPORT_METRICS.totalCNY || SUPPORT_METRICS.total || 0;
    totalEl.textContent = formatCurrency(getDisplayTotal(cny));
}

document.addEventListener('DOMContentLoaded', function() {
    AOS.init({
        duration: 800, 
        once: true, 
        offset: 50, 
    });

    initScrollProgressIndicator();

    const supportMethods = document.querySelectorAll('.support-method');
    const isMobile = window.innerWidth <= 768; 
    
    supportMethods.forEach(card => {
        let rect = card.getBoundingClientRect();
        let centerX = (rect.left + rect.right) / 2;
        let centerY = (rect.top + rect.bottom) / 2;
        
        card.addEventListener('mousemove', function(e) {
            if (isMobile) return; 
            
            rect = card.getBoundingClientRect();
            centerX = (rect.left + rect.right) / 2;
            centerY = (rect.top + rect.bottom) / 2;
            
            const relativeX = (e.clientX - centerX) / (rect.width / 2);
            const relativeY = (e.clientY - centerY) / (rect.height / 2);
            
            card.style.transform = `translateY(-20px) scale(1.03) rotateX(${-relativeY * 10}deg) rotateY(${relativeX * 10}deg)`;
            
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transform = `translateZ(40px) scale(1.08) rotate(${relativeX * 2}deg)`;
            }
            
            if (label) {
                label.style.transform = `translateZ(25px) translateY(-5px) translateX(${relativeX * 5}px) scale(1.05)`;
            }
        });
        
        card.addEventListener('mouseleave', function() {
            if (isMobile) return; 
            
            card.style.transform = '';
            const qrCode = card.querySelector('.support-qr');
            const label = card.querySelector('.support-label');
            
            if (qrCode) {
                qrCode.style.transform = 'translateZ(20px)';
            }
            
            if (label) {
                label.style.transform = 'translateZ(10px)';
            }
            
            setTimeout(() => {
                card.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                if (qrCode) qrCode.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                if (label) label.style.transition = 'all 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
            }, 50);
        });
        
        card.addEventListener('mouseenter', function() {
            if (isMobile) return; 
            
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

    initCoffeeParticles();
    
    initCurrency();

    updateSupportTotal();
    updateSupportCount();
    initCapsuleSparkles();
    initCapsuleConfetti();
    initCapsuleNumberObserver();
});

function initCoffeeParticles() {
    const coffeeParticlesContainer = document.querySelector('.coffee-particles');
    if (!coffeeParticlesContainer) return;

    const coffeeParticles = coffeeParticlesContainer.querySelectorAll('.coffee-particle');
    
    coffeeParticles.forEach(particle => {
        const delay = Math.random() * 5; 
        const duration = 15 + Math.random() * 10; 
        const left = Math.random() * 100; 

        particle.style.animationDelay = `${delay}s`;
        particle.style.animationDuration = `${duration}s`;
        particle.style.left = `${left}%`;
    });
}

function initScrollProgressIndicator() {
    const scrollProgressContainer = document.getElementById('scrollProgressContainer');
    const scrollProgressCircle = document.querySelector('.scroll-progress-circle-fill');
    const scrollProgressPercentage = document.querySelector('.scroll-progress-percentage');

    if (!scrollProgressContainer || !scrollProgressCircle || !scrollProgressPercentage) return;

    window.addEventListener('scroll', function() {
        updateScrollProgress();
    });

    scrollProgressContainer.addEventListener('click', function() {
        this.classList.add('clicked');
        
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
        
        setTimeout(() => {
            this.classList.remove('clicked');
        }, 500);
    });

    updateScrollProgress();

    function updateScrollProgress() {
        const scrollTop = window.scrollY;
        const scrollHeight = document.documentElement.scrollHeight - window.innerHeight;
        const scrollPercentage = (scrollTop / scrollHeight) * 100;
        
        const perimeter = Math.PI * 2 * 45; 
        const strokeDashoffset = perimeter * (1 - scrollPercentage / 100);
        scrollProgressCircle.style.strokeDashoffset = strokeDashoffset;
        
        scrollProgressPercentage.textContent = `${Math.round(scrollPercentage)}%`;
        
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

function addSupportTranslations() {
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    fixButtonPositions();
    
    if (currentLang === 'en') {
        const pageTitle = document.getElementById('page-title');
        if (pageTitle) {
            pageTitle.textContent = 'Catime - Support the Project';
        }
        
        const metaDescription = document.getElementById('meta-description');
        if (metaDescription) {
            metaDescription.setAttribute('content', 'Support Catime Project - A minimalist, modern, efficient transparent timer and pomodoro clock for Windows, with a cute style.');
        }
        
        document.querySelectorAll('.nav-links li a').forEach(link => {
            if (link.textContent === '首页') link.textContent = 'Home';
            if (link.textContent === '指南') link.textContent = 'Guide';
            if (link.textContent === '关于') link.textContent = 'About';
            if (link.querySelector('span') && link.querySelector('span').textContent === '下载') {
                link.querySelector('span').textContent = 'Download';
            }
        });
        
        const pageHeader = document.querySelector('.page-header h1');
        if (pageHeader) {
            pageHeader.textContent = 'Support the Project';
        }
        
        const pageHeaderSubtitle = document.querySelector('.page-header p');
        if (pageHeaderSubtitle) {
            pageHeaderSubtitle.textContent = 'Your support is our motivation to continuously develop and improve Catime';
        }
        
        translateSupportElements();
        
        fixButtonVisibility();
    }
}

function fixButtonPositions() {
    setTimeout(() => {
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

function fixButtonVisibility() {
    document.querySelectorAll('.support-card .support-btn').forEach(btn => {
        btn.style.display = 'flex';
        btn.style.visibility = 'visible';
        btn.style.opacity = '1';
    });
    
    const issuesBtn = document.querySelector('.support-card:nth-child(2) .support-btn');
    if (issuesBtn) {
        issuesBtn.style.display = 'flex';
        issuesBtn.style.visibility = 'visible';
        issuesBtn.style.opacity = '1';
        
        if (issuesBtn.querySelector('i')) {
            issuesBtn.querySelector('i').style.display = 'inline-block';
        }
    }
}

function translateSupportElements() {
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
    
    const projectDesc = document.querySelector('.support-project .section-subtitle');
    if (projectDesc) {
        projectDesc.innerHTML = 'Catime will continue to be open-source and free to use forever.<br>' +
            'But its development and maintenance require a lot of time and energy.<br>' +
            'If you find Catime helpful, consider buying the author a coffee,<br>' +
            'to fuel this passion ❤️‍🔥<br>' +
            'Every bit of your support is a powerful drive to keep it moving forward!';
    }
    
    document.querySelectorAll('.support-label').forEach(label => {
        if (label.textContent.includes('微信')) {
            label.innerHTML = '<i class="fab fa-weixin"></i> WeChat';
        }
        if (label.textContent.includes('支付宝')) {
            label.innerHTML = '<i class="fab fa-alipay"></i> Alipay';
        }
    });
    
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
    
    const supportersDesc = document.querySelector('.supporters .section-subtitle');
    if (supportersDesc) {
        supportersDesc.textContent = 'Special thanks to those who have supported the Catime project! Your encouragement is our motivation to move forward.';
    }

    const totalLabel = document.querySelector('.support-total-label');
    if (totalLabel) {
        totalLabel.innerHTML = '<i class="fas fa-coins"></i> Total Donations';
    }

    const countLabel = document.querySelector('.support-count-label');
    if (countLabel) {
        countLabel.innerHTML = '<i class="fas fa-user-friends"></i> Supporters';
    }
    
    const tableHeaders = document.querySelectorAll('.supporters-table th');
    if (tableHeaders.length >= 4) {
        tableHeaders[0].textContent = 'Time';
        tableHeaders[1].textContent = 'Username';
        tableHeaders[2].textContent = 'Amount';
        tableHeaders[3].textContent = 'Message';
    }
    
    document.querySelectorAll('.supporters-table td').forEach(td => {
        if (td.textContent === '温州市宇波机车部件有限公司') {
            td.textContent = 'Wenzhou Yubo Locomotive Parts CO.,LTD.';
        }
        if (td.textContent === '1.4版本太好了') {
            td.textContent = 'Version 1.4 is great';
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
        
        if (td.parentElement && td.cellIndex === 3) { 
            td.style.maxWidth = '250px';
            td.style.wordWrap = 'break-word';
            td.style.whiteSpace = 'normal';
            
            if (localStorage.getItem('catime-language') === 'en') {
                td.style.fontSize = '0.9rem';
                td.style.lineHeight = '1.4';
            }
        }
    });
}

function updateSupportTotal() {
    const table = document.querySelector('.supporters-table');
    const totalEl = document.getElementById('support-total-value');
    if (!table || !totalEl) return;

    let sum = 0;
    table.querySelectorAll('tbody tr').forEach(row => {
        const amountCell = row.cells && row.cells[2];
        if (!amountCell) return;
        const text = amountCell.textContent.trim();
        const match = text.replace(/[,\s]/g, '').match(/([\-\+]?)¥?([0-9]+(?:\.[0-9]+)?)/);
        if (match) {
            const sign = match[1] === '-' ? -1 : 1;
            const value = parseFloat(match[2]);
            if (!isNaN(value)) {
                sum += sign * value;
            }
        }
    });

    SUPPORT_METRICS.totalCNY = sum;
    SUPPORT_METRICS.total = getDisplayTotal(sum);
    if (SUPPORT_METRICS.animated) {
        totalEl.textContent = formatCurrency(SUPPORT_METRICS.total);
    }
}

function updateSupportCount() {
    const tbody = document.querySelector('.supporters-table tbody');
    const countEl = document.getElementById('support-count-value');
    if (!tbody || !countEl) return;

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

function initCapsuleSparkles() {
    const capsule = document.querySelector('.support-total-top');
    if (!capsule) return;

    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    if (media.matches) return;

    capsule.addEventListener('mouseenter', spawnSparkles);
    capsule.addEventListener('focus', spawnSparkles);

    function spawnSparkles() {
        for (let i = 0; i < 10; i++) {
            const s = document.createElement('span');
            s.className = 'sparkle';
            s.style.left = `${50 + (Math.random() * 40 - 20)}%`;
            s.style.top = `${45 + (Math.random() * 20 - 10)}%`;
            s.style.setProperty('--d', `${Math.random() * 0.2 + 0.05}s`);
            s.style.setProperty('--tx', `${(Math.random() * 140 - 70)}%`);
            s.style.setProperty('--ty', `${(Math.random() * -120 - 20)}%`);
            capsule.appendChild(s);
            setTimeout(() => s.remove(), 700);
        }
    }
}

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

function animateNumber(element, from, to, duration, formatter) {
    const start = performance.now();
    const ease = (t) => 1 - Math.pow(1 - t, 3); 
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

function initCapsuleNumberObserver() {
    const capsule = document.querySelector('.support-total-top');
    const totalEl = document.getElementById('support-total-value');
    const countEl = document.getElementById('support-count-value');
    if (!capsule || !totalEl || !countEl) return;

    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    const oncePlay = () => {
        if (SUPPORT_METRICS.animated) return;
        SUPPORT_METRICS.animated = true;
        const currentTotal = 0;
        const duration = media.matches ? 0 : 1800; 
        animateNumber(totalEl, currentTotal, SUPPORT_METRICS.total || 0, duration, (v) => formatCurrency(v));
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

function launchOverlayConfetti(anchor) {
    const overlay = document.createElement('div');
    overlay.className = 'confetti-overlay';
    document.body.appendChild(overlay);

    const rect = anchor.getBoundingClientRect();
    const originX = rect.left + rect.width / 2;
    const originY = rect.top + rect.height / 2;

    const colors = ['#7aa2f7', '#f799b8', '#ffd45e', '#9ae6b4', '#fbd38d'];
    const totalPieces = 40; 
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

    setTimeout(() => overlay.remove(), 1800);
}

 