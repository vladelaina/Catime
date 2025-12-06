document.addEventListener('DOMContentLoaded', function() {
    AOS.init({
        duration: 800,
        once: true,
        offset: 50,
    });

    setDownloadUrls();

    document.querySelectorAll('.animated-image img').forEach(img => {
        const container = img.closest('.animated-image');
        if (!container) return;
        
        const maxRotateX = 10;
        const maxRotateY = 15;
        
        let breatheTimer = null;
        
        let currentRotateX = 0;
        let currentRotateY = 0;
        
        function startBreatheEffect() {
            if (breatheTimer) return;
            
            let phase = 0;
            breatheTimer = setInterval(() => {
                const scale = 1.02 + Math.sin(phase) * 0.015;
                
                img.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                
                phase += 0.05;
            }, 30);
        }

        function stopBreatheEffect() {
            if (breatheTimer) {
                clearInterval(breatheTimer);
                breatheTimer = null;
            }
        }
        
        container.addEventListener('mousemove', function(e) {
            const rect = container.getBoundingClientRect();
            const x = (e.clientX - rect.left) / rect.width;
            const y = (e.clientY - rect.top) / rect.height;
            
            currentRotateY = (x - 0.5) * 2 * maxRotateY;
            currentRotateX = (y - 0.5) * -2 * maxRotateX;
        });
        
        container.addEventListener('mouseleave', function() {
            stopBreatheEffect();
            currentRotateX = 0;
            currentRotateY = 0;
            img.style.transform = 'scale(1) perspective(1000px)';
        });
        
        container.addEventListener('mouseenter', function() {
            img.style.transition = 'transform 0.2s ease-out';
            startBreatheEffect();
        });
        
        img.addEventListener('mousedown', function() {
            stopBreatheEffect();
            img.style.transform = `scale(0.98) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg) translateZ(-10px)`;
        });
        
        document.addEventListener('mouseup', function(event) {
            if (container.matches(':hover')) {
                if (event.target === img || img.contains(event.target)) {
                    img.style.transition = 'transform 0.15s cubic-bezier(0.34, 1.2, 0.64, 1)';
                    
                    setTimeout(() => {
                        startBreatheEffect();
                    }, 150);
                }
            }
        });
    });
    
    initHeroInteractions();
    
    initHeaderScroll();

    document.addEventListener('allComponentsLoaded', function() {
        console.log('📄 检测到组件加载完成');
    });

    handleWaveLetters();
});

function initHeaderScroll() {
    const header = document.querySelector('.main-header');
    if (!header) return;

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

function initHeroInteractions() {
    const hero = document.querySelector('.hero');
    if (!hero) return;

    const heroVisual = document.querySelector('.hero-visual-wrapper');
    const spotlight = document.querySelector('.hero-spotlight');
    const parallaxElements = document.querySelectorAll('[data-parallax-speed]');
    const magneticBtns = document.querySelectorAll('.btn-magnetic');

    hero.addEventListener('mousemove', (e) => {
        const rect = hero.getBoundingClientRect();
        const x = e.clientX - rect.left;
        const y = e.clientY - rect.top;
        
        hero.style.setProperty('--mouse-x', `${x}px`);
        hero.style.setProperty('--mouse-y', `${y}px`);

        const centerX = rect.width / 2;
        const centerY = rect.height / 2;
        const relativeX = (x - centerX) / centerX;
        const relativeY = (y - centerY) / centerY;

        if (heroVisual) {
            const rotateY = relativeX * 5;
            const rotateX = relativeY * -5;
            
            heroVisual.style.transform = `
                perspective(1000px)
                rotateX(${rotateX}deg)
                rotateY(${rotateY}deg)
                scale(1.02)
            `;
        }

        parallaxElements.forEach(el => {
            const speed = parseFloat(el.getAttribute('data-parallax-speed')) || 0.05;
            const moveX = -relativeX * speed * 100;
            const moveY = -relativeY * speed * 100;
            
            el.style.transform = `translate3d(${moveX}px, ${moveY}px, 0)`;
        });
    });

    hero.addEventListener('mouseleave', () => {
        if (heroVisual) {
            heroVisual.style.transform = 'perspective(1000px) rotateX(5deg) rotateY(0deg) scale(1)';
        }
        
        parallaxElements.forEach(el => {
            el.style.transform = 'translate3d(0, 0, 0)';
        });
    });

    magneticBtns.forEach(btn => {
        btn.addEventListener('mousemove', (e) => {
            const rect = btn.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const y = e.clientY - rect.top;
            
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
        });

        btn.addEventListener('mouseleave', () => {
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
        console.error('全局配置未加载');
        return;
    }

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

    document.querySelectorAll('a[data-download], a.download-link').forEach(a => {
        a.setAttribute('href', CATIME_CONFIG.DOWNLOAD_URL);
    });
}

function initScrollProgressIndicator() {
    const scrollProgressContainer = document.getElementById('scrollProgressContainer');
    if (!scrollProgressContainer) return;

    const scrollProgressCircle = document.querySelector('.scroll-progress-circle-fill');
    const scrollProgressPercentage = document.querySelector('.scroll-progress-percentage');

    if (!scrollProgressCircle || !scrollProgressPercentage) return;

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


