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

let catimeDownloadInfoPromise = null;
let catimeDownloadClickInitialized = false;

function getConfiguredDownloadInfo() {
    return {
        source: 'local',
        url: CATIME_CONFIG.DOWNLOAD_URL,
        file: CATIME_CONFIG.DOWNLOAD_FILE,
        version: CATIME_CONFIG.DOWNLOAD_VERSION || CATIME_CONFIG.VERSION,
    };
}

function getGithubDownloadUrl(file, version) {
    if (file && version && CATIME_CONFIG.GITHUB_URL) {
        return `${CATIME_CONFIG.GITHUB_URL}/releases/download/v${version}/${file}`;
    }

    return CATIME_CONFIG.GITHUB_DOWNLOAD_URL || CATIME_CONFIG.GITHUB_RELEASES_URL || CATIME_CONFIG.GITHUB_URL;
}

function getDownloadInfoFromManifest(manifest) {
    const latest = manifest && (manifest.latest || (Array.isArray(manifest.files) ? manifest.files[0] : null));
    if (!latest || !latest.file) return null;

    const manifestUrl = CATIME_CONFIG.DOWNLOAD_MANIFEST_URL || CATIME_CONFIG.DOWNLOAD_URL;

    return {
        source: 'local',
        url: new URL(latest.url || latest.file, manifestUrl).href,
        file: latest.file,
        version: latest.version,
        sha256: latest.sha256,
        size: latest.size,
    };
}

async function fetchDownloadManifest() {
    if (!CATIME_CONFIG.DOWNLOAD_MANIFEST_URL || window.location.protocol === 'file:') {
        return null;
    }

    try {
        const response = await fetch(CATIME_CONFIG.DOWNLOAD_MANIFEST_URL, { cache: 'no-store' });
        if (!response.ok) return null;

        return await response.json();
    } catch (error) {
        console.warn('Download manifest unavailable, using configured installer.', error);
        return null;
    }
}

async function isDownloadReachable(url) {
    if (!url) return false;

    const parsedUrl = new URL(url, window.location.href);
    if (parsedUrl.protocol === 'file:') return true;

    try {
        const response = await fetch(parsedUrl.href, { method: 'HEAD', cache: 'no-store' });
        if (response.ok) return true;

        if (response.status !== 405 && response.status !== 403) {
            return false;
        }
    } catch (error) {
        console.warn('Download HEAD check failed, retrying with GET.', error);
    }

    try {
        const response = await fetch(parsedUrl.href, {
            cache: 'no-store',
            headers: { Range: 'bytes=0-0' },
        });

        return response.ok || response.status === 206;
    } catch (error) {
        console.warn('Download file unavailable.', error);
        return false;
    }
}

async function resolveCatimeDownloadInfo() {
    if (catimeDownloadInfoPromise) {
        return catimeDownloadInfoPromise;
    }

    catimeDownloadInfoPromise = (async () => {
        const configuredInfo = getConfiguredDownloadInfo();
        const manifest = await fetchDownloadManifest();
        const localInfo = getDownloadInfoFromManifest(manifest) || configuredInfo;

        if (await isDownloadReachable(localInfo.url)) {
            return localInfo;
        }

        return {
            ...localInfo,
            source: 'github',
            url: getGithubDownloadUrl(localInfo.file, localInfo.version),
            fallback: true,
        };
    })();

    return catimeDownloadInfoPromise;
}

function applyDownloadLink(link, info) {
    if (!link || !info || !info.url) return;

    link.setAttribute('href', info.url);

    if (info.source === 'github') {
        link.removeAttribute('download');
        link.setAttribute('target', '_blank');
        link.setAttribute('rel', 'noopener noreferrer');
        return;
    }

    if (info.file) {
        link.setAttribute('download', info.file);
    }

    link.removeAttribute('target');
    link.removeAttribute('rel');
}

function getDownloadLinks() {
    const downloadButtons = [
        'hero-download-btn',
        'cta-download-btn'
    ];

    const links = downloadButtons
        .map(id => document.getElementById(id))
        .filter(Boolean);

    document.querySelectorAll('a[data-download], a.download-link').forEach(link => {
        if (!links.includes(link)) {
            links.push(link);
        }
    });

    return links;
}

function triggerDownload(info) {
    if (!info || !info.url) return;

    if (info.source === 'github') {
        window.location.assign(info.url);
        return;
    }

    const link = document.createElement('a');
    link.href = info.url;

    if (info.file) {
        link.download = info.file;
    }

    document.body.appendChild(link);
    link.click();
    link.remove();
}

function initDownloadClickFallback() {
    if (catimeDownloadClickInitialized) return;
    catimeDownloadClickInitialized = true;

    document.addEventListener('click', async (event) => {
        const target = event.target instanceof Element ? event.target : event.target.parentElement;
        const link = target ? target.closest('a[data-download], a.download-link') : null;
        if (!link) return;

        event.preventDefault();

        const info = await resolveCatimeDownloadInfo();
        applyDownloadLink(link, info);
        triggerDownload(info);
    });
}

function setDownloadUrls() {
    if (typeof CATIME_CONFIG === 'undefined') {
        console.error('Global config not loaded');
        return;
    }

    const configuredInfo = getConfiguredDownloadInfo();
    getDownloadLinks().forEach(link => applyDownloadLink(link, configuredInfo));
    initDownloadClickFallback();

    resolveCatimeDownloadInfo().then(info => {
        getDownloadLinks().forEach(link => applyDownloadLink(link, info));
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


