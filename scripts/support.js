let SUPPORT_METRICS = { total: 0, totalCNY: 0, count: 0, animated: false };

const CURRENCY_STATE = {
    target: 'USD', 
    symbol: '$',
    rateCnyToUsd: 0, 
    lastUpdated: 0
};

function getCurrentLanguage() {
    const saved = localStorage.getItem('catime-language');
    if (saved === 'en' || saved === 'zh') return saved;

    const browserLang = (navigator.languages && navigator.languages[0]) || navigator.language || 'zh-CN';
    return /^en\b/i.test(browserLang) ? 'en' : 'zh';
}

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
    initAOSOnce();

    initScrollProgressIndicator();

    initSupportMethodCards();
    initSupportCardIcons();
    initHeartAnimation();
    initCoffeeParticles();
    addSupportTranslations();

    initCurrency();

    updateSupportTotal();
    updateSupportCount();
    initCapsuleSparkles();
    initCapsuleConfetti();
    initCapsuleNumberObserver();
});

document.addEventListener('allComponentsLoaded', function() {
    addSupportTranslations();
});

function initSupportMethodCards() {
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
}

function initSupportCardIcons() {
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
}

function initHeartAnimation() {
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
}

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

function addSupportTranslations() {
    const currentLang = getCurrentLanguage();
    document.documentElement.lang = currentLang === 'en' ? 'en' : 'zh-CN';
    
    fixButtonPositions();
    translateSharedChrome(currentLang);
    
    if (currentLang === 'en') {
        const pageTitle = document.getElementById('page-title');
        if (pageTitle) {
            pageTitle.textContent = 'Catime - Support the Project';
        }
        
        const metaDescription = document.getElementById('meta-description');
        if (metaDescription) {
            metaDescription.setAttribute('content', 'Support Catime Project - A minimalist, modern, efficient transparent timer and pomodoro clock for Windows, with a cute style.');
        }
        
        CatimeUI.translateNavLinks({
            linkTranslations: {
                '首页': 'Home',
                '指南': 'Guide',
                '关于': 'About',
                '插件': 'Plugins',
            },
            spanTranslations: {
                '下载': 'Download',
            },
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

function translateSharedChrome(currentLang) {
    const navTranslations = currentLang === 'zh'
        ? {
            Home: '首页',
            Guide: '指南',
            About: '关于',
            GitHub: 'GitHub',
        }
        : {
            '首页': 'Home',
            '指南': 'Guide',
            '关于': 'About',
            '插件': 'Plugins',
        };

    CatimeUI.translateNavLinks({
        linkTranslations: navTranslations,
        trimText: true,
    });

    const dropdownToggle = document.querySelector('.dropdown-toggle');
    if (dropdownToggle) {
        dropdownToggle.innerHTML = currentLang === 'zh'
            ? '工具 <i class="fas fa-chevron-down"></i>'
            : 'Tools <i class="fas fa-chevron-down"></i>';
    }

    const fontToolLink = document.querySelector('.dropdown-menu a');
    if (fontToolLink && fontToolLink.textContent.includes('Font Simplifier')) {
        fontToolLink.innerHTML = currentLang === 'zh'
            ? '<i class="fas fa-font"></i> 字体简化工具'
            : '<i class="fas fa-font"></i> Font Simplifier';
    }
    if (fontToolLink && fontToolLink.textContent.includes('字体简化工具') && currentLang === 'en') {
        fontToolLink.innerHTML = '<i class="fas fa-font"></i> Font Simplifier';
    }

    const actionTranslations = currentLang === 'zh'
        ? {
            Download: '下载',
            Support: '支持项目',
        }
        : {
            '下载': 'Download',
            '支持项目': 'Support',
        };

    document.querySelectorAll('.nav-actions .nav-button span').forEach(span => {
        const translatedText = actionTranslations[span.textContent];
        if (translatedText) span.textContent = translatedText;
    });

    const footer = document.querySelector('.main-footer');
    if (!footer) return;

    const footerParagraphs = footer.querySelectorAll('p');
    if (footerParagraphs.length >= 3) {
        const footerParagraphTranslations = currentLang === 'zh'
            ? [
                '&copy; 2025-2026 Catime 项目，由 <a href="https://vladelaina.com/" target="_blank" rel="noopener noreferrer">vladelaina</a> 开发',
                '基于 <a href="https://github.com/vladelaina/Catime/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">Apache 2.0</a> 协议开源',
                '图标画师：<a href="https://space.bilibili.com/26087398" target="_blank" rel="noopener noreferrer">猫屋敷梨梨Official</a>',
            ]
            : [
                '&copy; 2025-2026 Catime Project by <a href="https://vladelaina.com/" target="_blank" rel="noopener noreferrer">vladelaina</a>',
                'Open sourced under <a href="https://github.com/vladelaina/Catime/blob/main/LICENSE" target="_blank" rel="noopener noreferrer">Apache 2.0</a> License',
                'Icon Artist: <a href="https://space.bilibili.com/26087398" target="_blank" rel="noopener noreferrer">猫屋敷梨梨Official</a>',
            ];

        footerParagraphTranslations.forEach((html, index) => {
            footerParagraphs[index].innerHTML = html;
        });
    }

    const footerLinkTranslations = {
        'message.bilibili.com': currentLang === 'zh' ? '反馈' : 'Feedback',
        'PRIVACY.md': currentLang === 'zh' ? '隐私政策' : 'Privacy Policy',
    };

    footer.querySelectorAll('.footer-links a').forEach(link => {
        const href = link.getAttribute('href') || '';
        for (const [hrefPart, text] of Object.entries(footerLinkTranslations)) {
            if (href.includes(hrefPart)) {
                link.textContent = text;
            }
        }
    });
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
        setSupportButtonVisible(btn);
    });
    
    const issuesBtn = document.querySelector('.support-card:nth-child(2) .support-btn');
    if (issuesBtn) {
        setSupportButtonVisible(issuesBtn);
        
        if (issuesBtn.querySelector('i')) {
            issuesBtn.querySelector('i').style.display = 'inline-block';
        }
    }
}

function setSupportButtonVisible(btn) {
    btn.style.display = 'flex';
    btn.style.visibility = 'visible';
    btn.style.opacity = '1';
}

function translateSupportElements() {
    const sectionTitleTranslations = {
        '支持项目': 'Support the Project <i class="fas fa-mug-hot"></i>',
        '其他支持方式': 'Other Ways to Support <i class="fas fa-gift"></i>',
        '感谢支持者': 'Thanks to Supporters',
    };

    CatimeUI.setInnerHTMLWhenIncludes('.section-title', sectionTitleTranslations);
    
    const projectDesc = document.querySelector('.support-project .section-subtitle');
    if (projectDesc) {
        projectDesc.innerHTML = 'Catime will continue to be open-source and free to use forever.<br>' +
            'But its development and maintenance require a lot of time and energy.<br>' +
            'If you find Catime helpful, consider buying the author a coffee,<br>' +
            'to fuel this passion ❤️‍🔥<br>' +
            'Every bit of your support is a powerful drive to keep it moving forward!';
    }
    
    const supportLabelTranslations = {
        '微信': '<i class="fab fa-weixin"></i> WeChat',
        '支付宝': '<i class="fab fa-alipay"></i> Alipay',
    };

    CatimeUI.setInnerHTMLWhenIncludes('.support-label', supportLabelTranslations);

    const wechatQr = document.querySelector('.wechat-qr');
    if (wechatQr) {
        wechatQr.alt = 'WeChat Pay';
    }

    const alipayQr = document.querySelector('.alipay-qr');
    if (alipayQr) {
        alipayQr.alt = 'Alipay';
    }

    const kofiButton = document.querySelector('.kofi-official-button');
    if (kofiButton) {
        kofiButton.alt = 'Support me on Ko-fi';
    }

    const starCardTranslation = {
        title: 'Star Project',
        description: 'If you like Catime, please give us a Star on GitHub. It\'s the best encouragement for us!',
        buttonHtml: '<i class="fab fa-github"></i> Star Project',
    };
    const issueCardTranslation = {
        title: 'Submit Issues',
        description: 'Found a bug or have feature suggestions? Welcome to submit Issues on GitHub to help us continuously improve Catime!',
        buttonHtml: '<i class="fas fa-exclamation-circle"></i> Submit Issues',
        forceVisible: true,
    };
    const cardTranslations = {
        '点亮 Star': starCardTranslation,
        'Star 项目': starCardTranslation,
        '提交反馈': issueCardTranslation,
        '提交Issues': issueCardTranslation,
        '分享推广': {
            title: 'Share & Promote',
            description: 'Share Catime with your friends, colleagues, or on social media to help more people discover this tool!',
            buttonHtml: '<i class="fas fa-users"></i> Join Discord',
            href: 'https://discord.com/invite/W3tW2gtp6g',
        },
    };

    const supportCards = document.querySelectorAll('.support-card');
    supportCards.forEach(card => {
        const title = card.querySelector('h3');
        const desc = card.querySelector('p');
        const btn = card.querySelector('.support-btn');

        const translation = title ? cardTranslations[title.textContent] : null;
        if (!translation) return;

        title.textContent = translation.title;
        desc.textContent = translation.description;

        if (btn) {
            btn.innerHTML = translation.buttonHtml;
            if (translation.href) {
                btn.href = translation.href;
            }
        }

        if (translation.forceVisible && btn) {
            setSupportButtonVisible(btn);
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
        ['Time', 'Username', 'Amount', 'Message'].forEach((text, index) => {
            tableHeaders[index].textContent = text;
        });
    }
    
    const supporterCellTranslations = {
        '温州市宇波机车部件有限公司': 'Wenzhou Yubo Locomotive Parts CO.,LTD.',
        '1.4版本太好了': 'Version 1.4 is great',
        '坚持就是胜利': 'Perseverance leads to victory',
        '加油': 'Keep going',
        '加油啊，你可以的': 'Come on, you can do it!',
        '加油，你可以的🫡': 'Keep going, you can do it! 🫡',
        '好用，爱用，希望增加个鼠标悬停时隐藏时钟的功能': 'Love it! Hope to add a feature to hide the clock when hovering with mouse',
        '催更催更😏': 'Push for updates😏',
        '番茄钟超赞，期待继续优化': 'Pomodoro timer is great, looking forward to further optimization',
        '很棒的项目！': 'Great project!',
        '恭喜': 'Congratulations',
        '感谢Catime，希望你也能多爱自己，未来可期': 'Thank you Catime, hope you also love yourself more, the future is promising',
        '建议catime加个倒计时列表功能': 'Suggest adding a countdown list feature to catime',
        '赞助了一年的域名 vladelaina.com': 'Sponsored one year of domain vladelaina.com',
        '打赏catime': 'Tipping catime',
        'catime打赏': 'Tipping catime',
        '支持catime': 'Support catime',
        '软件好用，赞赞赞': 'The software is great, praise!',
        '软件很好用，感谢你的坚持[爱心]': 'The software is very useful, thank you for your persistence [heart]',
        '极简，可爱，好用，喜欢': 'Minimalist, cute, useful, love it',
        '不错不错，实用鼓励一下': 'Not bad, very practical, a little encouragement',
        '在任务栏里吃灰叭（bushi）好用👍': "Let it collect dust in the taskbar (just kidding) It's great👍",
        '学生党，1块冲你和我一样喜欢蕾娜和伊蕾娜，1块冲你的产品确实挺好': 'As a student, ¥1 because you like Laina and Elaina like me, another ¥1 because your product is really good',
        '我是最早提出来让你弄这个二维码，我们粉丝可以赞助': 'I was the first to suggest setting up these QR codes so we fans could support you',
        '不多感谢你的catime，让我下班有了倒计时盼头🤧': 'Just wanted to thank you for catime, it gives me something to look forward to counting down to when getting off work🤧',
        '为爱发电': 'Powered by love',
        'catime小小支持': 'A little support for catime',
        '支持一下': 'Just a little support',
        '茉莉蜜茶行不行': 'How about jasmine honey tea',
        '好喜欢catime，请你喝奶茶': 'I really like catime, please have some milk tea',
        '时钟很不错，帮助页面也很漂亮 支持~*.。(๑･∀･๑)*.。': 'The timer is great, and the help page is beautiful. Support~*.。(๑･∀･๑)*.。',
        'bro好用狒狒防沉迷组件了已经是': "Bro, it's so useful! Already has anti-addiction component",
        '非常好的工具，谢谢': 'Very good tool, thank you',
        '功能简洁明了，谢谢你，cat': 'Simple and clear features, thank you, cat',
        '好用，respect': 'Useful, respect',
        '感谢 catime，好用。': 'Thanks to Catime, easy to use.',
        '入股入股，付费使用来自律': 'Investing, paying for self-discipline.',
        '非常好小程序': 'Very good app',
        '我是葱葱哦，想给你加个油，祝你的软件越做越好！': 'I am Congcong, just wanted to cheer you on. Hope your software keeps getting better and better!'
    };

    document.querySelectorAll('.supporters-table td').forEach(td => {
        const translatedText = supporterCellTranslations[td.textContent];
        if (translatedText) {
            td.textContent = translatedText;
        }
        
        if (td.parentElement && td.cellIndex === 3) { 
            td.style.maxWidth = '250px';
            td.style.wordWrap = 'break-word';
            td.style.whiteSpace = 'normal';
            
            if (getCurrentLanguage() === 'en') {
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
        window.addEventListener('scroll', onScroll, { passive: true });
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

 
