import {
    createNumberedFilename,
    createSequentialEntries,
    formatBytes,
    isImageFile,
    moveItem,
    totalFileSize,
} from './sorter-core.js';
import { createZipBlob } from './zip-store.js';

const DRAG_WHEEL_MULTIPLIER = 2.5;

const COPY = {
    en: {
        pageTitle: 'Catime - Tray Icon Sorter',
        pageDescription: 'Sort tray icon images by dragging, automatically number them from 1, and download the result as a ready-to-use ZIP archive.',
        heroTitle: 'Tray Icon',
        heroTitleAccent: 'Sorter',
        heroDescription: 'Add images, press and drag to sort, then download the ZIP.',
        workspaceTitle: 'Images',
        imagesLabel: 'images',
        imageSingular: 'image',
        clearAll: 'Clear all',
        addImages: 'Add images',
        uploadTitle: 'Add images',
        uploadDescription: 'Drop files here, click to select, or paste with Ctrl+V.',
        browseImages: 'Browse images',
        reorderHint: 'Press and drag the image to reorder. You can also use the arrow buttons or arrow keys.',
        filenameExample: 'ZIP filenames:',
        nothingToExport: 'Add images to prepare your ZIP',
        statusIdle: 'Everything stays in your browser.',
        downloadZip: 'Download ZIP',
        packagingButton: 'Building ZIP…',
        dropOverlayTitle: 'Release to add images',
        dropOverlayDescription: 'They will be appended to the current sequence',
        uploadAriaLabel: 'Select image files or drop them here',
        exportReadyOne: '1 image will be exported as a numbered ZIP',
        exportReadyMany: '{count} images will be exported as a numbered ZIP',
        statusAddedOne: '1 image was added to the sequence.',
        statusAddedMany: '{count} images were added to the sequence.',
        statusIgnoredOne: ' 1 non-image file was ignored.',
        statusIgnoredMany: ' {count} non-image files were ignored.',
        statusOnlyNonImages: 'No images were added. {count} unsupported files were ignored.',
        statusRemoved: 'Removed “{name}”.',
        statusMoved: '“{name}” is now at position {position}.',
        statusCleared: 'All images were cleared.',
        statusPackaging: 'Packing {completed}/{total}: {filename}',
        statusDownloadStarted: 'The numbered ZIP is ready and the download has started.',
        statusExportError: 'The ZIP could not be created. Try using fewer or smaller files.',
        confirmClear: 'Clear every image from this sequence?',
        pastedImage: 'Pasted image',
        previewUnavailable: 'Preview unavailable',
        dragLabel: 'Press and drag image {position}. Use arrow keys to change its position.',
        moveEarlier: 'Move image {position} earlier',
        moveLater: 'Move image {position} later',
        removeImage: 'Remove image {position}',
        previewAlt: 'Preview of {name}',
        duplicateBadge: 'Duplicate',
        duplicateSingular: 'duplicate',
        duplicateMany: 'duplicates',
    },
    zh: {
        pageTitle: 'Catime - 托盘图标排序工具',
        pageDescription: '拖动排列托盘图标图片，从 1 开始自动编号，并下载可直接使用的 ZIP 压缩包。',
        heroTitle: '托盘图标',
        heroTitleAccent: '排序工具',
        heroDescription: '添加图片，按住图片拖动排序，然后下载 ZIP。',
        workspaceTitle: '图片排序',
        imagesLabel: '张图片',
        imageSingular: '张图片',
        clearAll: '全部清空',
        addImages: '添加图片',
        uploadTitle: '添加图片',
        uploadDescription: '拖入文件、点击选择，或按 Ctrl+V 粘贴图片。',
        browseImages: '选择图片',
        reorderHint: '直接按住图片拖动即可排序，也可以使用方向按钮或键盘方向键。',
        filenameExample: 'ZIP 内文件名：',
        nothingToExport: '添加图片后即可生成 ZIP',
        statusIdle: '所有处理都只在你的浏览器中完成。',
        downloadZip: '下载 ZIP',
        packagingButton: '正在生成 ZIP…',
        dropOverlayTitle: '松开即可添加图片',
        dropOverlayDescription: '新图片会接在当前序列末尾',
        uploadAriaLabel: '选择图片或将图片拖到这里',
        exportReadyOne: '已排好 1 张图片，将导出为编号 ZIP',
        exportReadyMany: '已排好 {count} 张图片，将导出为编号 ZIP',
        statusAddedOne: '已向序列加入 1 张图片。',
        statusAddedMany: '已向序列加入 {count} 张图片。',
        statusIgnoredOne: ' 已忽略 1 个非图片文件。',
        statusIgnoredMany: ' 已忽略 {count} 个非图片文件。',
        statusOnlyNonImages: '没有添加图片，已忽略 {count} 个不支持的文件。',
        statusRemoved: '已移除“{name}”。',
        statusMoved: '“{name}”已移动到第 {position} 位。',
        statusCleared: '已清空所有图片。',
        statusPackaging: '正在打包 {completed}/{total}：{filename}',
        statusDownloadStarted: '编号 ZIP 已生成，下载已经开始。',
        statusExportError: '无法生成 ZIP，请尝试减少文件数量或使用更小的图片。',
        confirmClear: '确定要清空当前序列中的所有图片吗？',
        pastedImage: '粘贴的图片',
        previewUnavailable: '无法预览',
        dragLabel: '按住第 {position} 张图片即可拖动；也可以使用方向键调整位置。',
        moveEarlier: '将第 {position} 张图片向前移动',
        moveLater: '将第 {position} 张图片向后移动',
        removeImage: '移除第 {position} 张图片',
        previewAlt: '{name} 的预览图',
        duplicateBadge: '重复',
        duplicateSingular: '张重复',
        duplicateMany: '张重复',
    },
};

class TrayIconSorter {
    constructor() {
        this.language = resolveLanguage();
        this.copy = COPY[this.language];
        this.items = [];
        this.itemSequence = 0;
        this.exporting = false;
        this.fileDragDepth = 0;
        this.touchDrag = null;
        this.autoScrollFrame = null;
        this.dragPointer = null;
        this.duplicateScanVersion = 0;
        this.downloadProgressTimer = null;

        this.elements = {
            workspace: document.querySelector('.sorter-workspace'),
            imageCount: document.getElementById('imageCount'),
            duplicateStat: document.getElementById('duplicateStat'),
            duplicateCount: document.getElementById('duplicateCount'),
            duplicateLabel: document.getElementById('duplicateLabel'),
            imagesLabel: document.querySelector('[data-i18n="imagesLabel"]'),
            totalSize: document.getElementById('totalSize'),
            addButton: document.getElementById('addButton'),
            clearButton: document.getElementById('clearButton'),
            fileInput: document.getElementById('fileInput'),
            uploadZone: document.getElementById('uploadZone'),
            sorterPanel: document.getElementById('sorterPanel'),
            imageGrid: document.getElementById('imageGrid'),
            exportSummary: document.getElementById('exportSummary'),
            statusMessage: document.getElementById('statusMessage'),
            downloadButton: document.getElementById('downloadButton'),
            downloadButtonText: document.querySelector('#downloadButton span'),
            downloadButtonIcon: document.querySelector('#downloadButton i'),
            exportProgress: document.getElementById('exportProgress'),
            exportProgressFill: document.getElementById('exportProgressFill'),
            fileDragOverlay: document.getElementById('fileDragOverlay'),
        };

        this.applyTranslations();
        this.bindEvents();
        this.render();
    }

    translate(key, replacements = {}) {
        let value = this.copy[key] || COPY.en[key] || key;
        Object.entries(replacements).forEach(([name, replacement]) => {
            value = value.replaceAll(`{${name}}`, String(replacement));
        });
        return value;
    }

    applyTranslations() {
        document.documentElement.lang = this.language === 'zh' ? 'zh-CN' : 'en';
        document.title = this.translate('pageTitle');
        document.querySelector('meta[name="description"]')?.setAttribute('content', this.translate('pageDescription'));

        document.querySelectorAll('[data-i18n]').forEach(element => {
            const key = element.dataset.i18n;
            element.textContent = this.translate(key);
        });

        this.elements.uploadZone.setAttribute('aria-label', this.translate('uploadAriaLabel'));
    }

    bindEvents() {
        const {
            addButton,
            clearButton,
            downloadButton,
            fileInput,
            imageGrid,
            uploadZone,
        } = this.elements;

        addButton.addEventListener('click', () => this.openFilePicker());
        clearButton.addEventListener('click', () => this.clearAll());
        downloadButton.addEventListener('click', () => this.downloadZip());

        uploadZone.addEventListener('click', () => this.openFilePicker());
        uploadZone.addEventListener('keydown', event => {
            if (event.key !== 'Enter' && event.key !== ' ') return;
            event.preventDefault();
            this.openFilePicker();
        });

        fileInput.addEventListener('change', event => {
            this.addFiles(event.target.files);
            event.target.value = '';
        });

        imageGrid.addEventListener('click', event => this.handleGridClick(event));
        imageGrid.addEventListener('keydown', event => this.handleGridKeydown(event));
        imageGrid.addEventListener('pointerdown', event => this.handlePointerDown(event));
        window.addEventListener('pointermove', event => this.handlePointerMove(event));
        window.addEventListener('pointerup', event => this.finishTouchDrag(event));
        window.addEventListener('pointercancel', event => this.finishTouchDrag(event));
        window.addEventListener('blur', () => this.handleWindowBlur());
        window.addEventListener('wheel', event => this.handleDragWheel(event), { capture: true, passive: false });
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) this.handleWindowBlur();
        });

        document.addEventListener('paste', event => this.handlePaste(event));
        document.addEventListener('dragenter', event => this.handleFileDragEnter(event));
        document.addEventListener('dragover', event => this.handleFileDragOver(event));
        document.addEventListener('dragleave', event => this.handleFileDragLeave(event));
        document.addEventListener('drop', event => this.handleFileDrop(event));
        window.addEventListener('beforeunload', () => this.revokeAllPreviewUrls());
    }

    openFilePicker() {
        if (!this.exporting) this.elements.fileInput.click();
    }

    addFiles(fileList) {
        if (this.exporting) return;
        this.finishActivePointerDrag();

        const files = Array.from(fileList || []);
        const imageFiles = files.filter(isImageFile);
        const ignoredCount = files.length - imageFiles.length;

        imageFiles.forEach(file => {
            this.itemSequence += 1;
            this.items.push({
                id: `tray-image-${Date.now()}-${this.itemSequence}`,
                file,
                originalName: file.name || `${this.translate('pastedImage')} ${this.itemSequence}`,
                previewUrl: URL.createObjectURL(file),
                fingerprint: null,
                duplicate: false,
            });
        });

        this.render();
        if (imageFiles.length > 0) this.scanForDuplicates();

        if (imageFiles.length > 0) {
            const addedMessage = this.translate(
                imageFiles.length === 1 ? 'statusAddedOne' : 'statusAddedMany',
                { count: imageFiles.length },
            );
            const ignoredMessage = ignoredCount > 0
                ? this.translate(ignoredCount === 1 ? 'statusIgnoredOne' : 'statusIgnoredMany', { count: ignoredCount })
                : '';
            this.setStatus(`${addedMessage}${ignoredMessage}`, ignoredCount > 0 ? 'warning' : 'success');
        } else if (ignoredCount > 0) {
            this.setStatus(this.translate('statusOnlyNonImages', { count: ignoredCount }), 'error');
        }
    }

    render(options = {}) {
        const hasImages = this.items.length > 0;
        this.elements.workspace.classList.toggle('has-images', hasImages);
        this.elements.sorterPanel.hidden = !hasImages;
        this.elements.imageGrid.replaceChildren(...this.items.map((item, index) => this.createImageCard(item, index)));
        this.updateSummary();
        this.updateInteractivity();

        if (options.focusId) {
            requestAnimationFrame(() => {
                this.getCardById(options.focusId)?.querySelector('.card-preview')?.focus();
            });
        }
    }

    createImageCard(item, index) {
        const position = index + 1;
        const card = document.createElement('li');
        card.className = 'image-card';
        card.dataset.id = item.id;
        card.classList.toggle('is-duplicate', item.duplicate);

        const preview = document.createElement('div');
        preview.className = 'card-preview';
        preview.dataset.role = 'drag-surface';
        preview.draggable = false;
        preview.tabIndex = 0;
        preview.setAttribute('aria-describedby', 'reorderHint');
        preview.setAttribute('aria-label', this.translate('dragLabel', { position }));

        const image = document.createElement('img');
        image.src = item.previewUrl;
        image.draggable = false;
        image.alt = this.translate('previewAlt', { name: item.originalName });
        image.loading = 'lazy';
        image.decoding = 'async';
        image.addEventListener('error', () => preview.classList.add('is-unavailable'), { once: true });

        const fallback = document.createElement('span');
        fallback.className = 'preview-fallback';
        fallback.append(this.createIcon('fas fa-file-image'));
        const fallbackText = document.createElement('span');
        fallbackText.textContent = this.translate('previewUnavailable');
        fallback.append(fallbackText);

        const badge = document.createElement('span');
        badge.className = 'sequence-badge';
        badge.dataset.role = 'sequence';
        badge.textContent = String(position);

        const removeButton = document.createElement('button');
        removeButton.className = 'preview-remove';
        removeButton.type = 'button';
        removeButton.dataset.action = 'remove';
        removeButton.draggable = false;
        removeButton.disabled = this.exporting;
        removeButton.setAttribute('aria-label', this.translate('removeImage', { position }));
        removeButton.append(this.createIcon('fas fa-xmark'));

        const duplicateBadge = document.createElement('span');
        duplicateBadge.className = 'duplicate-badge';
        duplicateBadge.textContent = this.translate('duplicateBadge');
        duplicateBadge.hidden = !item.duplicate;

        preview.append(image, fallback, badge, removeButton, duplicateBadge);

        const details = document.createElement('div');
        details.className = 'card-details';

        const outputFilename = document.createElement('span');
        outputFilename.className = 'output-filename';
        outputFilename.dataset.role = 'output-name';
        outputFilename.textContent = createNumberedFilename(item.file, position);

        const originalFilename = document.createElement('span');
        originalFilename.className = 'original-filename';
        originalFilename.title = item.originalName;
        originalFilename.textContent = item.originalName;

        const footer = document.createElement('div');
        footer.className = 'card-footer';

        const fileSize = document.createElement('span');
        fileSize.className = 'file-size';
        fileSize.textContent = formatBytes(item.file.size);

        const actions = document.createElement('div');
        actions.className = 'card-actions';
        actions.append(
            this.createActionButton('move-earlier', 'fas fa-arrow-left', this.translate('moveEarlier', { position }), index === 0),
            this.createActionButton('move-later', 'fas fa-arrow-right', this.translate('moveLater', { position }), index === this.items.length - 1),
        );

        footer.append(fileSize, actions);
        details.append(outputFilename, originalFilename, footer);
        card.append(preview, details);

        return card;
    }

    createActionButton(action, iconClass, label, disabled, extraClass = '') {
        const button = document.createElement('button');
        button.className = `card-action ${extraClass}`.trim();
        button.type = 'button';
        button.dataset.action = action;
        button.disabled = disabled || this.exporting;
        button.setAttribute('aria-label', label);
        button.append(this.createIcon(iconClass));
        return button;
    }

    createIcon(className) {
        const icon = document.createElement('i');
        icon.className = className;
        icon.setAttribute('aria-hidden', 'true');
        return icon;
    }

    updateSummary() {
        const count = this.items.length;
        this.elements.imageCount.textContent = String(count);
        this.elements.imagesLabel.textContent = this.translate(count === 1 ? 'imageSingular' : 'imagesLabel');
        this.elements.totalSize.textContent = formatBytes(totalFileSize(this.items));
        const duplicateCount = this.items.filter(item => item.duplicate).length;
        this.elements.duplicateStat.hidden = duplicateCount === 0;
        this.elements.duplicateCount.textContent = String(duplicateCount);
        this.elements.duplicateLabel.textContent = this.translate(
            duplicateCount === 1 ? 'duplicateSingular' : 'duplicateMany',
        );
        this.elements.exportSummary.textContent = count === 0
            ? this.translate('nothingToExport')
            : this.translate(count === 1 ? 'exportReadyOne' : 'exportReadyMany', { count });
    }

    updateInteractivity() {
        const hasImages = this.items.length > 0;
        this.elements.addButton.disabled = this.exporting;
        this.elements.clearButton.disabled = !hasImages || this.exporting;
        this.elements.downloadButton.disabled = !hasImages || this.exporting;
        this.elements.uploadZone.setAttribute('aria-disabled', String(this.exporting));
        this.refreshDomPositions();

        this.elements.imageGrid.querySelectorAll('[data-role="drag-surface"]').forEach(surface => {
            surface.draggable = false;
            surface.tabIndex = this.exporting ? -1 : 0;
            surface.setAttribute('aria-disabled', String(this.exporting));
        });

        this.elements.imageGrid.querySelectorAll('button').forEach(button => {
            if (button.matches('[data-action="remove"]')) {
                button.disabled = this.exporting;
            }
        });
    }

    handleGridClick(event) {
        if (this.exporting || this.touchDrag) return;
        const actionButton = event.target.closest('[data-action]');
        const card = actionButton?.closest('.image-card');
        if (!actionButton || !card) return;

        const index = this.items.findIndex(item => item.id === card.dataset.id);
        if (index < 0) return;

        switch (actionButton.dataset.action) {
            case 'move-earlier':
                this.moveImage(index, index - 1);
                break;
            case 'move-later':
                this.moveImage(index, index + 1);
                break;
            case 'remove':
                this.removeImage(index);
                break;
            default:
                break;
        }
    }

    handleGridKeydown(event) {
        if (this.exporting || this.touchDrag || !event.target.matches('[data-role="drag-surface"]')) return;
        const card = event.target.closest('.image-card');
        const index = this.items.findIndex(item => item.id === card?.dataset.id);
        if (index < 0) return;

        const earlierKeys = ['ArrowLeft', 'ArrowUp'];
        const laterKeys = ['ArrowRight', 'ArrowDown'];
        let destination = index;

        if (earlierKeys.includes(event.key)) destination = index - 1;
        else if (laterKeys.includes(event.key)) destination = index + 1;
        else if (event.key === 'Home') destination = 0;
        else if (event.key === 'End') destination = this.items.length - 1;
        else return;

        event.preventDefault();
        this.moveImage(index, destination);
    }

    moveImage(fromIndex, toIndex) {
        const destination = Math.max(0, Math.min(toIndex, this.items.length - 1));
        if (fromIndex === destination) return;

        const item = this.items[fromIndex];
        this.items = moveItem(this.items, fromIndex, destination);
        this.render({ focusId: item.id });
        this.setStatus(this.translate('statusMoved', {
            name: item.originalName,
            position: destination + 1,
        }), 'success');
    }

    removeImage(index) {
        const [removed] = this.items.splice(index, 1);
        if (!removed) return;

        URL.revokeObjectURL(removed.previewUrl);
        this.recalculateDuplicateGroups();
        const focusItem = this.items[Math.min(index, this.items.length - 1)];
        this.render({ focusId: focusItem?.id });
        this.setStatus(this.translate('statusRemoved', { name: removed.originalName }), 'success');
    }

    clearAll(options = {}) {
        if (this.exporting || this.items.length === 0) return;
        if (!options.skipConfirmation && !window.confirm(this.translate('confirmClear'))) return;

        this.finishActivePointerDrag();
        this.revokeAllPreviewUrls();
        this.items = [];
        this.duplicateScanVersion += 1;
        this.render();
        this.setStatus(this.translate('statusCleared'), 'success');
    }

    handlePointerDown(event) {
        if (this.exporting || this.touchDrag || event.button !== 0) return;
        if (event.target.closest('[data-action]')) return;
        const surface = event.target.closest('[data-role="drag-surface"]');
        const card = surface?.closest('.image-card');
        if (!surface || !card) return;

        event.preventDefault();
        surface.focus({ preventScroll: true });
        const ghost = card.cloneNode(true);
        ghost.classList.add('drag-ghost');
        ghost.classList.remove('is-dragging');
        ghost.setAttribute('aria-hidden', 'true');
        ghost.querySelectorAll('button').forEach(button => {
            button.disabled = true;
            button.removeAttribute('draggable');
        });
        document.body.append(ghost);

        const cardRect = card.getBoundingClientRect();
        ghost.style.width = `${cardRect.width}px`;

        this.touchDrag = {
            id: card.dataset.id,
            initialPosition: this.getCardPosition(card.dataset.id),
            pointerId: event.pointerId,
            surface,
            ghost,
            ghostWidth: cardRect.width,
            ghostHeight: cardRect.height,
            grabRatioX: cardRect.width > 0 ? (event.clientX - cardRect.left) / cardRect.width : 0.5,
            grabRatioY: cardRect.height > 0 ? (event.clientY - cardRect.top) / cardRect.height : 0.5,
        };
        card.classList.add('is-dragging');
        document.body.classList.add('is-touch-sorting');
        this.positionGhost(event.clientX, event.clientY);
        this.updateDragPointer(event.clientX, event.clientY);
    }

    handlePointerMove(event) {
        if (!this.touchDrag || event.pointerId !== this.touchDrag.pointerId) return;
        if (event.pointerType === 'mouse' && (event.buttons & 1) === 0) {
            this.finishActivePointerDrag();
            return;
        }
        event.preventDefault();
        this.positionGhost(event.clientX, event.clientY);
        this.updateDragPointer(event.clientX, event.clientY);

        const sourceCard = this.getCardById(this.touchDrag.id);
        const targetCard = this.findCardAtPoint(event.clientX, event.clientY, sourceCard);
        if (!sourceCard || !targetCard || sourceCard === targetCard) return;
        this.placeCardNearPointer(sourceCard, targetCard, event.clientX, event.clientY);
    }

    finishTouchDrag(event) {
        if (!this.touchDrag || event.pointerId !== this.touchDrag.pointerId) return;
        event.preventDefault();

        this.finishActivePointerDrag();
    }

    finishActivePointerDrag() {
        if (!this.touchDrag) return;
        const drag = this.touchDrag;
        this.touchDrag = null;
        this.stopAutoScroll();
        drag.ghost.remove();
        document.body.classList.remove('is-touch-sorting');
        this.finishDomReorder(drag.id, drag.initialPosition);
    }

    handleWindowBlur() {
        this.finishActivePointerDrag();
        this.hideFileDragOverlay();
    }

    positionGhost(clientX, clientY) {
        if (!this.touchDrag) return;
        const ghostWidth = this.touchDrag.ghostWidth || 180;
        const ghostHeight = this.touchDrag.ghostHeight || 220;
        const x = clientX - (ghostWidth * this.touchDrag.grabRatioX);
        const y = clientY - (ghostHeight * this.touchDrag.grabRatioY);
        this.touchDrag.ghost.style.transform = `translate3d(${x}px, ${y}px, 0)`;
    }

    updateDragPointer(clientX, clientY) {
        this.dragPointer = { clientX, clientY };
        if (this.autoScrollFrame === null) {
            this.autoScrollFrame = requestAnimationFrame(() => this.runAutoScroll());
        }
    }

    runAutoScroll() {
        this.autoScrollFrame = null;
        if (!this.touchDrag || !this.dragPointer) return;

        const edge = Math.min(100, window.innerHeight * 0.18);
        const { clientY } = this.dragPointer;
        let speed = 0;
        if (clientY < edge) speed = -Math.ceil(18 * (1 - Math.max(0, clientY) / edge));
        else if (clientY > window.innerHeight - edge) {
            speed = Math.ceil(18 * (1 - Math.max(0, window.innerHeight - clientY) / edge));
        }

        if (speed !== 0) {
            window.scrollBy(0, speed);
            this.reorderAtDragPointer();
        }
        this.autoScrollFrame = requestAnimationFrame(() => this.runAutoScroll());
    }

    stopAutoScroll() {
        if (this.autoScrollFrame !== null) cancelAnimationFrame(this.autoScrollFrame);
        this.autoScrollFrame = null;
        this.dragPointer = null;
    }

    handleDragWheel(event) {
        if (!this.touchDrag) return;
        event.preventDefault();
        const unit = event.deltaMode === WheelEvent.DOM_DELTA_LINE
            ? 16
            : event.deltaMode === WheelEvent.DOM_DELTA_PAGE ? window.innerHeight : 1;
        window.scrollBy(0, event.deltaY * unit * DRAG_WHEEL_MULTIPLIER);
        if (this.touchDrag && this.dragPointer) {
            this.positionGhost(this.dragPointer.clientX, this.dragPointer.clientY);
        }
        requestAnimationFrame(() => this.reorderAtDragPointer());
    }

    reorderAtDragPointer() {
        if (!this.dragPointer) return;
        const sourceId = this.touchDrag?.id;
        const sourceCard = this.getCardById(sourceId);
        const targetCard = this.findCardAtPoint(
            this.dragPointer.clientX,
            this.dragPointer.clientY,
            sourceCard,
        );
        if (!sourceCard || !targetCard || sourceCard === targetCard) return;
        this.placeCardNearPointer(
            sourceCard,
            targetCard,
            this.dragPointer.clientX,
            this.dragPointer.clientY,
        );
    }

    findCardAtPoint(clientX, clientY, sourceCard) {
        const element = document.elementFromPoint(clientX, clientY);
        const directCard = element?.closest('.image-card');
        if (directCard === sourceCard) return null;
        if (directCard?.parentElement === this.elements.imageGrid) return directCard;

        const gridRect = this.elements.imageGrid.getBoundingClientRect();
        const insideGrid = clientX >= gridRect.left && clientX <= gridRect.right
            && clientY >= gridRect.top && clientY <= gridRect.bottom;
        if (!insideGrid) return null;

        let closestCard = null;
        let closestDistance = Number.POSITIVE_INFINITY;
        this.elements.imageGrid.querySelectorAll('.image-card').forEach(card => {
            if (card === sourceCard) return;
            const rect = card.getBoundingClientRect();
            const dx = clientX - (rect.left + rect.width / 2);
            const dy = clientY - (rect.top + rect.height / 2);
            const distance = (dx * dx) + (dy * dy);
            if (distance < closestDistance) {
                closestDistance = distance;
                closestCard = card;
            }
        });
        return closestCard;
    }

    placeCardNearPointer(sourceCard, targetCard, clientX, clientY) {
        const rect = targetCard.getBoundingClientRect();
        const columnCount = getComputedStyle(this.elements.imageGrid).gridTemplateColumns.split(' ').length;
        const verticalRatio = (clientY - rect.top) / rect.height;
        let insertBefore;

        if (columnCount <= 1) {
            insertBefore = clientY < rect.top + (rect.height / 2);
        } else if (verticalRatio < 0.24) {
            insertBefore = true;
        } else if (verticalRatio > 0.76) {
            insertBefore = false;
        } else {
            insertBefore = clientX < rect.left + (rect.width / 2);
        }

        const reference = insertBefore ? targetCard : targetCard.nextElementSibling;
        if (reference === sourceCard) return;
        this.elements.imageGrid.insertBefore(sourceCard, reference);
        this.refreshDomPositions();
    }

    refreshDomPositions() {
        const itemMap = new Map(this.items.map(item => [item.id, item]));
        const cards = Array.from(this.elements.imageGrid.children);

        cards.forEach((card, index) => {
            const item = itemMap.get(card.dataset.id);
            const position = index + 1;
            if (!item) return;

            card.querySelector('[data-role="sequence"]').textContent = String(position);
            card.querySelector('[data-role="output-name"]').textContent = createNumberedFilename(item.file, position);

            const surface = card.querySelector('[data-role="drag-surface"]');
            surface.setAttribute('aria-label', this.translate('dragLabel', { position }));

            const earlier = card.querySelector('[data-action="move-earlier"]');
            earlier.disabled = index === 0 || this.exporting;
            earlier.setAttribute('aria-label', this.translate('moveEarlier', { position }));

            const later = card.querySelector('[data-action="move-later"]');
            later.disabled = index === cards.length - 1 || this.exporting;
            later.setAttribute('aria-label', this.translate('moveLater', { position }));

            card.querySelector('[data-action="remove"]')
                .setAttribute('aria-label', this.translate('removeImage', { position }));
        });
    }

    finishDomReorder(id, initialPosition) {
        const itemMap = new Map(this.items.map(item => [item.id, item]));
        this.items = Array.from(this.elements.imageGrid.children)
            .map(card => itemMap.get(card.dataset.id))
            .filter(Boolean);

        const item = this.items.find(candidate => candidate.id === id);
        const position = this.items.findIndex(candidate => candidate.id === id) + 1;
        this.render({ focusId: id });

        if (item && position !== initialPosition) {
            this.setStatus(this.translate('statusMoved', {
                name: item.originalName,
                position,
            }), 'success');
        }
    }

    async scanForDuplicates() {
        const scanVersion = ++this.duplicateScanVersion;
        const pending = this.items.filter(item => !item.fingerprint);

        await Promise.all(pending.map(async item => {
            try {
                item.fingerprint = await createVisualFingerprint(item.file, item.previewUrl);
            } catch (error) {
                console.warn(`Unable to compare image ${item.originalName}:`, error);
                item.fingerprint = { failed: true };
            }
        }));

        if (scanVersion !== this.duplicateScanVersion) return;
        this.recalculateDuplicateGroups();
        this.refreshDuplicatePresentation();
    }

    recalculateDuplicateGroups() {
        this.items.forEach(item => { item.duplicate = false; });
        for (let left = 0; left < this.items.length; left += 1) {
            for (let right = left + 1; right < this.items.length; right += 1) {
                if (!fingerprintsMatch(this.items[left].fingerprint, this.items[right].fingerprint)) continue;
                this.items[left].duplicate = true;
                this.items[right].duplicate = true;
            }
        }
    }

    refreshDuplicatePresentation() {
        const itemMap = new Map(this.items.map(item => [item.id, item]));
        this.elements.imageGrid.querySelectorAll('.image-card').forEach(card => {
            const item = itemMap.get(card.dataset.id);
            if (!item) return;
            card.classList.toggle('is-duplicate', item.duplicate);
            const badge = card.querySelector('.duplicate-badge');
            if (badge) badge.hidden = !item.duplicate;
        });
        this.updateSummary();
    }

    getCardById(id) {
        return Array.from(this.elements.imageGrid.children).find(card => card.dataset.id === id) || null;
    }

    getCardPosition(id) {
        return Array.from(this.elements.imageGrid.children).findIndex(card => card.dataset.id === id) + 1;
    }

    handlePaste(event) {
        if (this.exporting) return;
        const clipboardItems = Array.from(event.clipboardData?.items || []);
        const imageFiles = clipboardItems
            .filter(item => item.kind === 'file' && item.type.startsWith('image/'))
            .map(item => item.getAsFile())
            .filter(Boolean);

        if (imageFiles.length === 0) return;
        event.preventDefault();
        this.addFiles(imageFiles);
    }

    isFileDrag(event) {
        return Array.from(event.dataTransfer?.types || []).includes('Files');
    }

    handleFileDragEnter(event) {
        if (!this.isFileDrag(event) || this.exporting) return;
        event.preventDefault();
        this.fileDragDepth += 1;
        this.elements.fileDragOverlay.hidden = false;
        this.elements.uploadZone.classList.add('is-file-over');
    }

    handleFileDragOver(event) {
        if (!this.isFileDrag(event) || this.exporting) return;
        event.preventDefault();
        event.dataTransfer.dropEffect = 'copy';
    }

    handleFileDragLeave(event) {
        if (this.fileDragDepth === 0) return;
        if (event.relatedTarget === null) {
            this.hideFileDragOverlay();
            return;
        }
        this.fileDragDepth -= 1;
        if (this.fileDragDepth === 0) this.hideFileDragOverlay();
    }

    handleFileDrop(event) {
        if (!this.isFileDrag(event) || this.exporting) return;
        event.preventDefault();
        const files = event.dataTransfer.files;
        this.hideFileDragOverlay();
        this.addFiles(files);
    }

    hideFileDragOverlay() {
        this.fileDragDepth = 0;
        this.elements.fileDragOverlay.hidden = true;
        this.elements.uploadZone.classList.remove('is-file-over');
    }

    async downloadZip() {
        if (this.exporting || this.items.length === 0) return;

        this.finishActivePointerDrag();
        const snapshot = [...this.items];
        this.setExporting(true);

        try {
            const zip = await createZipBlob(createSequentialEntries(snapshot), {
                onProgress: progress => {
                    this.elements.exportProgressFill.style.width = `${progress.percent}%`;
                    if (progress.filename) {
                        this.setStatus(this.translate('statusPackaging', progress));
                    }
                },
            });
            this.triggerDownload(zip, 'catime-tray-icons.zip');
            this.setStatus(this.translate('statusDownloadStarted'), 'success');
        } catch (error) {
            console.error('Unable to create tray icon ZIP:', error);
            this.setStatus(this.translate('statusExportError'), 'error');
        } finally {
            this.setExporting(false);
        }
    }

    setExporting(exporting) {
        this.exporting = exporting;
        clearTimeout(this.downloadProgressTimer);
        this.elements.downloadButton.classList.toggle('is-busy', exporting);
        this.elements.downloadButtonText.textContent = this.translate(exporting ? 'packagingButton' : 'downloadZip');
        this.elements.downloadButtonIcon.className = exporting ? 'fas fa-spinner' : 'fas fa-download';
        this.elements.exportProgress.hidden = !exporting;
        if (exporting) this.elements.exportProgressFill.style.width = '0%';
        this.updateInteractivity();

        if (!exporting) {
            this.elements.exportProgressFill.style.width = '100%';
            this.downloadProgressTimer = setTimeout(() => {
                this.elements.exportProgress.hidden = true;
                this.elements.exportProgressFill.style.width = '0%';
            }, 900);
        }
    }

    triggerDownload(blob, filename) {
        const url = URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url;
        link.download = filename;
        document.body.append(link);
        link.click();
        link.remove();
        setTimeout(() => URL.revokeObjectURL(url), 30000);
    }

    setStatus(message, type = '') {
        this.elements.statusMessage.textContent = message;
        this.elements.statusMessage.classList.toggle('is-success', type === 'success');
        this.elements.statusMessage.classList.toggle('is-error', type === 'error');
        this.elements.statusMessage.classList.toggle('is-warning', type === 'warning');
    }

    revokeAllPreviewUrls() {
        this.items.forEach(item => URL.revokeObjectURL(item.previewUrl));
    }
}

async function createVisualFingerprint(file, previewUrl) {
    const size = 32;
    const canvas = document.createElement('canvas');
    canvas.width = size;
    canvas.height = size;
    const context = canvas.getContext('2d', { willReadFrequently: true });
    context.imageSmoothingEnabled = true;
    context.imageSmoothingQuality = 'high';
    context.fillStyle = '#f3f5fb';
    context.fillRect(0, 0, size, size);

    const decoded = await decodeImage(file, previewUrl);
    const sourceWidth = decoded.width || decoded.naturalWidth;
    const sourceHeight = decoded.height || decoded.naturalHeight;
    const scale = Math.min(size / sourceWidth, size / sourceHeight);
    const width = sourceWidth * scale;
    const height = sourceHeight * scale;
    context.drawImage(decoded, (size - width) / 2, (size - height) / 2, width, height);
    decoded.close?.();

    const rgba = context.getImageData(0, 0, size, size).data;
    const pixels = new Uint8Array(size * size * 3);
    for (let source = 0, target = 0; source < rgba.length; source += 4) {
        pixels[target++] = rgba[source];
        pixels[target++] = rgba[source + 1];
        pixels[target++] = rgba[source + 2];
    }

    return { hash: createDifferenceHash(pixels, size), pixels };
}

async function decodeImage(file, previewUrl) {
    if ('createImageBitmap' in window) {
        try {
            return await createImageBitmap(file, { imageOrientation: 'from-image' });
        } catch {
            // The regular image decoder supports a few formats createImageBitmap may reject.
        }
    }

    return new Promise((resolve, reject) => {
        const image = new Image();
        image.onload = () => resolve(image);
        image.onerror = () => reject(new Error('Image decoding failed'));
        image.src = previewUrl;
    });
}

function createDifferenceHash(pixels, size) {
    let hash = 0n;
    for (let row = 0; row < 8; row += 1) {
        const y = Math.round((row + 0.5) * size / 8 - 0.5);
        for (let column = 0; column < 8; column += 1) {
            const leftX = Math.round(column * (size - 1) / 8);
            const rightX = Math.round((column + 1) * (size - 1) / 8);
            const left = pixelLuminance(pixels, size, leftX, y);
            const right = pixelLuminance(pixels, size, rightX, y);
            if (left > right) hash |= 1n << BigInt((row * 8) + column);
        }
    }
    return hash;
}

function pixelLuminance(pixels, size, x, y) {
    const offset = ((y * size) + x) * 3;
    return (pixels[offset] * 0.299) + (pixels[offset + 1] * 0.587) + (pixels[offset + 2] * 0.114);
}

function fingerprintsMatch(left, right) {
    if (!left || !right || left.failed || right.failed) return false;
    if (countHashDifferences(left.hash, right.hash) > 6) return false;

    let absoluteDifference = 0;
    let squaredDifference = 0;
    for (let index = 0; index < left.pixels.length; index += 1) {
        const difference = left.pixels[index] - right.pixels[index];
        absoluteDifference += Math.abs(difference);
        squaredDifference += difference * difference;
    }

    const channelCount = left.pixels.length;
    return absoluteDifference / channelCount <= 4
        && Math.sqrt(squaredDifference / channelCount) <= 8;
}

function countHashDifferences(left, right) {
    let difference = left ^ right;
    let count = 0;
    while (difference !== 0n) {
        difference &= difference - 1n;
        count += 1;
    }
    return count;
}

function resolveLanguage() {
    const configuredLanguage = window.CatimeLocale?.getLanguage?.();
    if (configuredLanguage === 'zh' || configuredLanguage === 'en') return configuredLanguage;

    try {
        const savedLanguage = localStorage.getItem('catime-language');
        if (savedLanguage === 'zh' || savedLanguage === 'en') return savedLanguage;
    } catch {
        // Local storage may be unavailable in private browsing contexts.
    }

    return /^zh\b/i.test(navigator.language || '') ? 'zh' : 'en';
}

const trayIconSorter = new TrayIconSorter();
window.trayIconSorter = trayIconSorter;
window.clearImages = () => trayIconSorter.clearAll({ skipConfirmation: true });
window.downloadSortedImages = () => trayIconSorter.downloadZip();
