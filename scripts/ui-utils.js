window.CatimeUI = window.CatimeUI || {};

(function(CatimeUI) {
    function escapeRegExp(value) {
        return String(value).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }

    function loadExternalScriptOnce(src, marker) {
        const existingScript = document.querySelector(`script[data-loader="${marker}"]`);
        if (existingScript) {
            if (existingScript.dataset.loaded === 'true') {
                return Promise.resolve();
            }

            return new Promise((resolve, reject) => {
                existingScript.addEventListener('load', () => resolve(), { once: true });
                existingScript.addEventListener('error', () => reject(new Error(`Failed to load ${src}`)), { once: true });
            });
        }

        return new Promise((resolve, reject) => {
            const script = document.createElement('script');
            script.src = src;
            script.async = true;
            script.dataset.loader = marker;
            script.onload = () => {
                script.dataset.loaded = 'true';
                resolve();
            };
            script.onerror = () => reject(new Error(`Failed to load ${src}`));
            document.head.appendChild(script);
        });
    }

    function initAOSOnce() {
        if (!window.AOS || document.documentElement.dataset.aosInitialized) return;

        AOS.init({
            duration: 800,
            once: true,
            offset: 50,
        });
        document.documentElement.dataset.aosInitialized = 'true';
    }

    function initScrollProgressIndicator() {
        const scrollProgressContainer = document.getElementById('scrollProgressContainer');
        if (!scrollProgressContainer) return;

        const scrollProgressCircle = document.querySelector('.scroll-progress-circle-fill');
        const scrollProgressPercentage = document.querySelector('.scroll-progress-percentage');

        if (!scrollProgressCircle || !scrollProgressPercentage) return;

        window.addEventListener('scroll', function() {
            updateScrollProgress();
        }, { passive: true });

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

    function setupVideoCoverPlayer(options = {}) {
        const videoContainer = document.getElementById(options.containerId || 'videoContainer');
        if (!videoContainer) return;

        const videoCover = videoContainer.querySelector('.video-cover');
        const videoFrameContainer = videoContainer.querySelector('.video-frame-container');
        const autoplayIframe = videoContainer.querySelector('#autoplayIframe');

        if (!videoCover || !videoFrameContainer || !autoplayIframe) return;

        if (options.preloadSrc) {
            const preloadImg = new Image();
            preloadImg.src = options.preloadSrc;
        }

        videoCover.addEventListener('click', function() {
            videoCover.style.opacity = '0';
            videoFrameContainer.style.display = 'block';

            const realSrc = autoplayIframe.getAttribute('data-src');
            autoplayIframe.src = realSrc;

            setTimeout(function() {
                videoCover.style.display = 'none';
                autoplayIframe.style.opacity = '1';
            }, 50);
        });
    }

    function translateNavLinks(options = {}) {
        const {
            selector = '.nav-links li a',
            linkTranslations = {},
            spanTranslations = {},
            trimText = false,
        } = options;

        document.querySelectorAll(selector).forEach(link => {
            const linkText = trimText ? link.textContent.trim() : link.textContent;
            const translatedLinkText = linkTranslations[linkText];
            if (translatedLinkText) {
                link.textContent = translatedLinkText;
            }

            const span = link.querySelector('span');
            if (!span) return;

            const translatedSpanText = spanTranslations[span.textContent];
            if (translatedSpanText) {
                span.textContent = translatedSpanText;
            }
        });
    }

    function applyInnerHTMLTranslations(selector, translations) {
        const elements = document.querySelectorAll(selector);

        for (const [key, value] of Object.entries(translations)) {
            elements.forEach(el => {
                if (el.innerHTML === key) {
                    el.innerHTML = value;
                } else if (el.innerHTML && el.innerHTML.includes(key)) {
                    el.innerHTML = el.innerHTML.replace(new RegExp(escapeRegExp(key), 'g'), value);
                }
            });
        }
    }

    function setInnerHTMLWhenIncludes(selector, translations) {
        document.querySelectorAll(selector).forEach(el => {
            for (const [sourceText, translatedHtml] of Object.entries(translations)) {
                if (el.innerHTML.includes(sourceText)) {
                    el.innerHTML = translatedHtml;
                }
            }
        });
    }

    function initTiltBreatheImages(options = {}) {
        const {
            imageSelector,
            containerSelector,
            containerClosest,
            maxRotateX = 10,
            maxRotateY = 15,
            scaleBase = 1.02,
            scaleAmplitude = 0.015,
            phaseStep = 0.05,
            intervalMs = 30,
            enablePress = false,
        } = options;

        if (!imageSelector) return;

        const images = document.querySelectorAll(imageSelector);
        const containers = containerSelector ? document.querySelectorAll(containerSelector) : null;

        if (containerSelector && (images.length === 0 || containers.length === 0)) return;

        const targets = containers
            ? Array.from(containers, (container, index) => ({ container, img: images[index] }))
            : Array.from(images, img => ({ container: img.closest(containerClosest), img }));

        targets.forEach(({ container, img }) => {
            if (!container || !img) return;

            let breatheTimer = null;
            let currentRotateX = 0;
            let currentRotateY = 0;

            function startBreatheEffect() {
                if (breatheTimer) return;

                let phase = 0;
                breatheTimer = setInterval(() => {
                    const scale = scaleBase + Math.sin(phase) * scaleAmplitude;
                    img.style.transform = `scale(${scale}) perspective(1000px) rotateY(${currentRotateY}deg) rotateX(${currentRotateX}deg)`;
                    phase += phaseStep;
                }, intervalMs);
            }

            function stopBreatheEffect() {
                if (!breatheTimer) return;

                clearInterval(breatheTimer);
                breatheTimer = null;
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

            if (!enablePress) return;

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
    }

    CatimeUI.escapeRegExp = escapeRegExp;
    CatimeUI.loadExternalScriptOnce = loadExternalScriptOnce;
    CatimeUI.initAOSOnce = initAOSOnce;
    CatimeUI.initScrollProgressIndicator = initScrollProgressIndicator;
    CatimeUI.setupVideoCoverPlayer = setupVideoCoverPlayer;
    CatimeUI.translateNavLinks = translateNavLinks;
    CatimeUI.applyInnerHTMLTranslations = applyInnerHTMLTranslations;
    CatimeUI.setInnerHTMLWhenIncludes = setInnerHTMLWhenIncludes;
    CatimeUI.initTiltBreatheImages = initTiltBreatheImages;

    window.escapeRegExp = escapeRegExp;
    window.loadExternalScriptOnce = loadExternalScriptOnce;
    window.initAOSOnce = initAOSOnce;
    window.initScrollProgressIndicator = initScrollProgressIndicator;
})(window.CatimeUI);
