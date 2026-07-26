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

 
