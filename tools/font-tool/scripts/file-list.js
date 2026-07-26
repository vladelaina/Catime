function hideScanInfo() {
    if (scanInfo) {
        scanInfo.style.display = 'none';
    }
}

function updateFileList() {
    if (selectedFiles.length === 0) {
        fileList.style.display = 'none';
        hideScanInfo(); 
        return;
    }

    fileList.style.display = 'block';
    fileItems.innerHTML = '';

    selectedFiles.forEach((file, index) => {
        const fileItem = document.createElement('div');
        fileItem.className = 'file-item';
        const safeFileName = escapeHTML(file.name);
        
        fileItem.innerHTML = `
            <div class="file-info">
                <div class="file-name">${safeFileName}</div>
                <div class="file-size">${formatFileSize(file.size)}</div>
            </div>
            <button class="file-remove" onclick="removeFile(${index})">
                <i class="fas fa-times"></i>
            </button>
        `;
        
        fileItems.appendChild(fileItem);
    });

    scrollToFileList();
}

function scrollToElement(targetElement, elementName = '目标区域') {
    if (!targetElement || targetElement.style.display === 'none') {
        console.log(`❌ ${elementName}不存在或不可见，跳过滚动`);
        return;
    }
    
    setTimeout(() => {
        try {
            const header = document.querySelector('.main-header');
            let headerHeight = 0;
            
            if (header) {
                const headerRect = header.getBoundingClientRect();
                headerHeight = headerRect.height;
                const headerStyle = window.getComputedStyle(header);
                if (headerStyle.position === 'fixed' || headerStyle.position === 'sticky') {
                    headerHeight = headerRect.height;
                }
            }
            
            console.log(`📏 导航栏高度: ${headerHeight}px`);
            
            const targetRect = targetElement.getBoundingClientRect();
            const currentScrollY = window.scrollY;
            
            const highlightOffsetY = 3; 
            const safetyMargin = 8; 
            const totalOffsetY = highlightOffsetY + safetyMargin; 
            
            let targetScrollY = currentScrollY + targetRect.top - headerHeight - totalOffsetY;
            
            const maxScrollY = document.documentElement.scrollHeight - window.innerHeight;
            targetScrollY = Math.max(0, Math.min(targetScrollY, maxScrollY));
            
            const scrollDifference = Math.abs(targetScrollY - currentScrollY);
            const minScrollThreshold = 5; 
            
            console.log(`🎯 滚动到${elementName} - 当前位置: ${currentScrollY}px, 目标位置: ${targetScrollY}px, 需要滚动: ${targetScrollY - currentScrollY}px`);
            console.log(`🔄 总偏移量: ${totalOffsetY}px (高亮动画${highlightOffsetY}px + 安全边距${safetyMargin}px), 确保不被导航栏覆盖`);
            
            if (scrollDifference > minScrollThreshold) {
                window.scrollTo({
                    top: targetScrollY,
                    behavior: 'smooth'
                });
                
                console.log(`📍 已精确滚动到${elementName}（含安全边距，避免被导航栏覆盖）`);
            } else {
                console.log(`📍 ${elementName}已在合适位置（含安全边距），无需滚动`);
            }
            
        } catch (error) {
            console.warn(`精确滚动到${elementName}失败，使用备用方案:`, error);
            
            try {
                targetElement.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start',
                    inline: 'nearest'
                });
                
                setTimeout(() => {
                    const header = document.querySelector('.main-header');
                    if (header) {
                        const headerHeight = header.getBoundingClientRect().height;
                        const targetRect = targetElement.getBoundingClientRect();
                        const totalOffsetY = 11; 
                        
                        if (targetRect.top < headerHeight + totalOffsetY) {
                            window.scrollBy({
                                top: targetRect.top - headerHeight - totalOffsetY,
                                behavior: 'smooth'
                            });
                        }
                    }
                }, 300);
                
            } catch (fallbackError) {
                console.warn(`所有滚动方案失败，使用基础滚动:`, fallbackError);
                try {
                    const rect = targetElement.getBoundingClientRect();
                    const header = document.querySelector('.main-header');
                    const headerHeight = header ? header.getBoundingClientRect().height : 0;
                    const totalOffsetY = 11; 
                    window.scrollTo(0, window.scrollY + rect.top - headerHeight - totalOffsetY);
                } catch {
                    targetElement.scrollIntoView();
                }
            }
        }
    }, 150); 
}

function scrollToFileList() {
    scrollToElement(fileList, '文件列表区域');
    
    if (fileList && fileList.style.display === 'block') {
        setTimeout(() => {
            fileList.style.animation = 'highlightFileList 1.5s ease-in-out';
            
            setTimeout(() => {
                fileList.style.animation = '';
            }, 1500);
        }, 150);
    }
}

function scrollToDownloadSection() {
    scrollToElement(downloadSection, '处理后的字体区域');
    
    if (downloadSection && downloadSection.style.display === 'block') {
        setTimeout(() => {
            downloadSection.style.animation = 'highlightFileList 1.5s ease-in-out';
            
            setTimeout(() => {
                downloadSection.style.animation = '';
            }, 1500);
        }, 150);
    }
}

function removeFile(index) {
    selectedFiles.splice(index, 1);
    updateFileList();
    console.log('文件已移除。');
}

function clearFiles() {
    selectedFiles = [];
    folderMode = false;
    folderStructure = {
        name: '',
        folderNames: [],
        files: [],
        fontFiles: [],
        directories: new Set()
    };
    fileSourceTracking = {
        standalone: [],
        fromFolders: []
    };
    updateFileList();
    hideScanInfo();
    console.log('已清除所有文件。');
}

function setCharacters(chars) {
    charactersInput.value = chars;
    console.log(`已设置要保留的字符: ${chars}`);
}

function formatFileSize(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}



function updateProgress(current, total) {
    const percentage = (current / total) * 100;
    progressFill.style.width = `${percentage}%`;
    progressText.textContent = `${Math.round(percentage)}% (${current}/${total})`;
}

function createTimingDisplay() {
    if (timingText) {
        timingText.remove();
    }
    
    timingText = document.createElement('div');
    timingText.className = 'timing-text';
    timingText.innerHTML = `<i class="fas fa-clock"></i> ${translateText('已耗时: ')}0${translateText('秒')}`;
    
    progressContainer.appendChild(timingText);
    
    startTimingUpdate();
}

function startTimingUpdate() {
    if (timingInterval) {
        clearInterval(timingInterval);
    }
    
    timingInterval = setInterval(() => {
        if (processingStartTime) {
            const elapsedTime = Date.now() - processingStartTime;
            updateTimingDisplay(elapsedTime);
        }
    }, 1000);
    
    updateTimingDisplay(0);
}

function updateTimingDisplay(elapsedTime) {
    if (!timingText) return;
    
    const seconds = Math.floor(elapsedTime / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    
    let timeString;
    if (hours > 0) {
        timeString = `${hours}${translateText('小时')}${minutes % 60}${translateText('分钟')}${seconds % 60}${translateText('秒')}`;
    } else if (minutes > 0) {
        timeString = `${minutes}${translateText('分钟')}${seconds % 60}${translateText('秒')}`;
    } else {
        timeString = `${seconds}${translateText('秒')}`;
    }
    
    timingText.innerHTML = `<i class="fas fa-clock"></i> ${translateText('已耗时: ')}${timeString}`;
}

function stopTimingAndShowResult() {
    if (timingInterval) {
        clearInterval(timingInterval);
        timingInterval = null;
    }
    
    if (processingStartTime && timingText) {
        const totalTime = Date.now() - processingStartTime;
        const seconds = Math.floor(totalTime / 1000);
        const minutes = Math.floor(seconds / 60);
        const hours = Math.floor(minutes / 60);
        
        let timeString;
        if (hours > 0) {
            timeString = `${hours}${translateText('小时')}${minutes % 60}${translateText('分钟')}${seconds % 60}${translateText('秒')}`;
        } else if (minutes > 0) {
            timeString = `${minutes}${translateText('分钟')}${seconds % 60}${translateText('秒')}`;
        } else {
            timeString = `${seconds}${translateText('秒')}`;
        }
        
        timingText.innerHTML = `<i class="fas fa-check-circle"></i> ${translateText('处理完成，总耗时: ')}${timeString}`;
        timingText.classList.add('timing-completed');
    }
}

