export function resolveMotionPolicy({ reducedMotion = false, saveData = false, effectiveType = '' } = {}) {
    if (reducedMotion) return { automatic: false, manual: false, concurrency: 1 };

    const networkType = String(effectiveType).toLowerCase();
    if (saveData || networkType === 'slow-2g' || networkType === '2g') {
        return { automatic: false, manual: true, concurrency: 1 };
    }
    if (networkType === '3g') return { automatic: true, manual: true, concurrency: 1 };
    return { automatic: true, manual: true, concurrency: 2 };
}

export function createPreviewLoader({ concurrency = 2, loadPreview }) {
    if (!Number.isInteger(concurrency) || concurrency < 1) {
        throw new RangeError('Preview concurrency must be a positive integer');
    }
    if (typeof loadPreview !== 'function') throw new TypeError('A preview loader function is required');

    const records = new Map();
    const queue = [];
    let active = 0;

    function request(url, { priority = false } = {}) {
        const existing = records.get(url);
        if (existing) {
            if (priority && existing.status === 'queued') {
                const index = queue.indexOf(existing);
                if (index >= 0) queue.splice(index, 1);
                queue.unshift(existing);
            }
            return existing.promise;
        }

        let settle;
        const promise = new Promise(resolve => { settle = resolve; });
        const record = { url, status: 'queued', promise, settle };
        records.set(url, record);
        if (priority) queue.unshift(record);
        else queue.push(record);
        pump();
        return promise;
    }

    function pump() {
        while (active < concurrency && queue.length > 0) {
            const record = queue.shift();
            record.status = 'loading';
            active += 1;
            Promise.resolve(loadPreview(record.url)).then(loaded => {
                const succeeded = loaded !== false;
                record.status = succeeded ? 'loaded' : 'failed';
                record.settle(succeeded);
                if (!succeeded) records.delete(record.url);
            }, () => {
                record.status = 'failed';
                record.settle(false);
                records.delete(record.url);
            }).finally(() => {
                active -= 1;
                pump();
            });
        }
    }

    return { request };
}
