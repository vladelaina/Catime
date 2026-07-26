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

