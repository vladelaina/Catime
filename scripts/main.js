document.addEventListener('DOMContentLoaded', function() {
    initAOSOnce();

    setDownloadUrls();

    initAnimatedImageInteractions();

    initHeroInteractions();

    initHeaderScroll();

    document.addEventListener('allComponentsLoaded', function() {
        console.log('📄 Components loaded');
    });

    handleWaveLetters();
});

function initAnimatedImageInteractions() {
    CatimeUI.initTiltBreatheImages({
        imageSelector: '.animated-image img',
        containerClosest: '.animated-image',
        maxRotateX: 10,
        maxRotateY: 15,
        scaleAmplitude: 0.015,
        enablePress: true,
    });
}

function initHeaderScroll() {
    const header = document.querySelector('.main-header');
    if (!header) return;

    const updateHeaderState = () => {
        if (window.scrollY > 50) {
            header.classList.add('scrolled');
        } else {
            header.classList.remove('scrolled');
        }
    };

    let ticking = false;
    window.addEventListener('scroll', () => {
        if (ticking) return;

        ticking = true;
        requestAnimationFrame(() => {
            updateHeaderState();
            ticking = false;
        });
    }, { passive: true });

    updateHeaderState();
}

function initHeroInteractions() {
    const hero = document.querySelector('.hero');
    if (!hero) return;

    const parallaxElements = document.querySelectorAll('[data-parallax-speed]');
    const magneticBtns = document.querySelectorAll('.btn-magnetic');

    let heroPointer = null;
    let heroRafId = null;

    const updateHeroPointer = () => {
        if (!heroPointer) return;

        const rect = hero.getBoundingClientRect();
        const x = heroPointer.clientX - rect.left;
        const y = heroPointer.clientY - rect.top;

        hero.style.setProperty('--mouse-x', `${x}px`);
        hero.style.setProperty('--mouse-y', `${y}px`);

        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const relativeX = (x - centerX) / centerX;
        const relativeY = (y - centerY) / centerY;

        parallaxElements.forEach(el => {
            const speed = parseFloat(el.getAttribute('data-parallax-speed')) || 0.05;
            const moveX = -relativeX * speed * 100;
            const moveY = -relativeY * speed * 100;
            
            el.style.transform = `translate3d(${moveX}px, ${moveY}px, 0)`;
        });
    };

    hero.addEventListener('mousemove', (e) => {
        heroPointer = {
            clientX: e.clientX,
            clientY: e.clientY
        };

        if (heroRafId) return;

        heroRafId = requestAnimationFrame(() => {
            heroRafId = null;
            updateHeroPointer();
        });
    }, { passive: true });

    hero.addEventListener('mouseleave', () => {
        heroPointer = null;
        if (heroRafId) {
            cancelAnimationFrame(heroRafId);
            heroRafId = null;
        }

        parallaxElements.forEach(el => {
            el.style.transform = 'translate3d(0, 0, 0)';
        });
    });

    magneticBtns.forEach(btn => {
        let pointer = null;
        let rafId = null;

        const updateButtonPointer = () => {
            if (!pointer) return;

            const rect = btn.getBoundingClientRect();
            const x = pointer.clientX - rect.left;
            const y = pointer.clientY - rect.top;
            
            const centerX = rect.width / 2;
            const centerY = rect.height / 2;
            
            const strength = 0.3;
            const deltaX = (x - centerX) * strength;
            const deltaY = (y - centerY) * strength;
            
            btn.style.transform = `translate(${deltaX}px, ${deltaY}px)`;
            
            const glow = btn.querySelector('.btn-glow');
            if (glow) {
                glow.style.transform = `translate(${deltaX * 0.5}px, ${deltaY * 0.5}px)`;
            }
        };

        btn.addEventListener('mousemove', (e) => {
            pointer = {
                clientX: e.clientX,
                clientY: e.clientY
            };

            if (rafId) return;

            rafId = requestAnimationFrame(() => {
                rafId = null;
                updateButtonPointer();
            });
        }, { passive: true });

        btn.addEventListener('mouseleave', () => {
            pointer = null;
            if (rafId) {
                cancelAnimationFrame(rafId);
                rafId = null;
            }

            btn.style.transform = 'translate(0, 0)';
            const glow = btn.querySelector('.btn-glow');
            if (glow) {
                glow.style.transform = 'translate(0, 0)';
            }
        });
    });
}

function setDownloadUrls() {
    if (typeof CATIME_CONFIG === 'undefined') {
        console.error('Global config not loaded');
        return;
    }

    const downloadButtons = [
        'hero-download-btn',
        'cta-download-btn'
    ];

    downloadButtons.forEach(id => {
        const btn = document.getElementById(id);
        if (btn) {
            btn.href = CATIME_CONFIG.DOWNLOAD_URL;
        }
    });

    document.querySelectorAll('a[data-download], a.download-link').forEach(a => {
        a.setAttribute('href', CATIME_CONFIG.DOWNLOAD_URL);
    });
}

function handleWaveLetters() {
    const ctaTitle = document.getElementById('cta-title');
    if (!ctaTitle) return;

    while (ctaTitle.firstChild) {
        ctaTitle.removeChild(ctaTitle.firstChild);
    }

    const englishText = 'Ready to manage time with Catime?';

    for (let i = 0; i < englishText.length; i++) {
        const span = document.createElement('span');
        span.className = 'wave-letter';
        span.textContent = englishText[i];
        ctaTitle.appendChild(span);
    }
}


