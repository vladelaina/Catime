function clearAllProcessedFiles() {
    if (processedFonts.length === 0) {
        return;
    }
    
    if (confirm('Are you sure you want to clear all processed results? This cannot be undone.')) {
        console.log('Clearing all processed files...');
        processedFonts = [];
        
        fileSourceTracking.standalone = [];
        fileSourceTracking.fromFolders = [];
        
        folderMode = false;
        folderStructure = {
            name: '',
            folderNames: [],
            files: [],
            fontFiles: [],
            directories: new Set()
        };
        
        updateDownloadItemsDisplay();
        
        downloadControls.style.display = 'none';
        downloadSection.style.display = 'none';
        
        console.log('📦 All data cleared');
        showTemporaryMessage('All processed files cleared', 'success');
        
        scrollToUploadArea();
    }
}

function updateScanInfo(total, fontCount, otherCount, isFolder = false) {
    if (!scanInfo || !scanInfoText) return;
    
    scanInfo.style.display = 'flex';
    
    let message = '';
    if (isFolder) {
        message = `Folder scan: Found ${total} files (${fontCount} font files, ${otherCount} other files)`;
    } else {
        message = `Found ${total} files (${fontCount} font files, ${otherCount} other files)`;
    }
    
    scanInfoText.textContent = message;
    
    scanInfo.classList.remove('fade-in');
    void scanInfo.offsetWidth; 
    scanInfo.classList.add('fade-in');
}

function updateZipProgress(percent, text, details) {
    if (zipProgressContainer) {
        zipProgressContainer.style.display = 'block';
        zipProgressFill.style.width = `${percent}%`;
        if (zipProgressText) zipProgressText.textContent = text;
        if (zipProgressDetails) zipProgressDetails.textContent = details;
    }
}

function showZipProgress() {
    if (zipProgressContainer) {
        zipProgressContainer.style.display = 'block';
        zipProgressFill.style.width = '0%';
        if (zipProgressText) zipProgressText.textContent = 'Preparing...';
        if (zipProgressDetails) zipProgressDetails.textContent = 'Initializing...';
    }
}

function hideZipProgress() {
    if (zipProgressContainer) {
        setTimeout(() => {
            zipProgressContainer.style.display = 'none';
        }, 2000); 
    }
}

function readFileAsArrayBuffer(file) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = (e) => resolve(e.target.result);
        reader.onerror = (e) => reject(e);
        reader.readAsArrayBuffer(file);
    });
}

function scrollToUploadArea() {
    if (uploadSection) {
        uploadSection.scrollIntoView({ behavior: 'smooth', block: 'start' });
        progressContainer.style.display = 'none';
        progressFill.style.width = '0%';
        progressText.textContent = '0%';
    }
}

function showTemporaryMessage(message, type = 'info') {
    let iconClass = 'info-circle';
    switch (type) {
        case 'success':
            iconClass = 'check-circle';
            break;
        case 'warning':
            iconClass = 'exclamation-triangle';
            break;
        case 'error':
            iconClass = 'times-circle';
            break;
        case 'info':
        default:
            iconClass = 'info-circle';
            break;
    }
    
    const messageDiv = document.createElement('div');
    messageDiv.className = `temporary-message ${type}`;
    const icon = document.createElement('i');
    icon.className = `fas fa-${iconClass}`;
    const text = document.createElement('span');
    text.textContent = message;
    messageDiv.append(icon, text);
    
    document.body.insertBefore(messageDiv, document.body.firstChild);
    
    setTimeout(() => {
        messageDiv.classList.add('show');
    }, 100);
    
    setTimeout(() => {
        messageDiv.classList.remove('show');
        setTimeout(() => {
            messageDiv.remove();
        }, 300);
    }, 3000);
}

function updateFileScrollProgress() {
    if (!fileItems || fileItems.children.length === 0) {
        fileScrollFill.style.width = '0%';
        return;
    }
    
    const scrollTop = fileItems.scrollTop;
    const scrollHeight = fileItems.scrollHeight;
    const clientHeight = fileItems.clientHeight;
    
    if (scrollHeight <= clientHeight) {
        fileScrollFill.style.width = '100%';
        return;
    }
    
    const scrollPercentage = (scrollTop / (scrollHeight - clientHeight)) * 100;
    fileScrollFill.style.width = Math.min(100, Math.max(0, scrollPercentage)) + '%';
}

function initFileScrollProgress() {
    if (fileItems) {
        fileItems.addEventListener('scroll', updateFileScrollProgress);
        const observer = new MutationObserver(updateFileScrollProgress);
        observer.observe(fileItems, { childList: true, subtree: true });
    }
}

window.addEventListener('error', function(e) {
    console.error(`Error occurred: ${e.message}`);
});

window.addEventListener('unhandledrejection', function(e) {
    console.error(`Promise error: ${e.reason}`);
    e.preventDefault();
});
