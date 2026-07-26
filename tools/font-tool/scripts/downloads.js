function updateDownloadSectionTitle() {
    const downloadTitle = downloadSection.querySelector('h2');
    
    if (processedFonts.length === 0) {
        downloadTitle.innerHTML = `<i class="fas fa-download"></i> ${translateText('处理后的字体')}`;
        return;
    }
    
    const totalOriginalSize = processedFonts.reduce((sum, font) => sum + font.originalSize, 0);
    const totalNewSize = processedFonts.reduce((sum, font) => sum + font.newSize, 0);
    const totalCompressionRatio = ((totalOriginalSize - totalNewSize) / totalOriginalSize * 100).toFixed(1);
    
    downloadTitle.innerHTML = `
        <i class="fas fa-download"></i> ${translateText('处理后的字体')} 
        <span style="font-size: 14px; color: #666; font-weight: normal;">
            ${formatFileSize(totalOriginalSize)} => ${formatFileSize(totalNewSize)} (${translateText('压缩了')} ${totalCompressionRatio}%)
        </span>
    `;
}

function addSingleDownloadItem(font, index) {
    const downloadItem = document.createElement('div');
    downloadItem.className = 'download-item';
    downloadItem.setAttribute('data-index', index); 
    
    const compressionRatio = ((font.originalSize - font.newSize) / font.originalSize * 100).toFixed(1);
    const safeFontName = escapeHTML(font.name);
    
    downloadItem.innerHTML = `
        <div class="download-info">
            <div class="download-name">${safeFontName}</div>
            <div class="download-size">
                ${formatFileSize(font.originalSize)} => ${formatFileSize(font.newSize)} 
                (${translateText('压缩了')} ${compressionRatio}%)
            </div>
        </div>
        <div class="download-actions">
            <button class="download-remove" onclick="removeProcessedFont(${index})" title="${translateText('删除此处理后的字体')}">
                <i class="fas fa-times"></i>
            </button>
            <button class="btn btn-success" onclick="downloadFont(${index})">
                <i class="fas fa-download"></i> ${translateText('下载')}
            </button>
        </div>
    `;
    
    downloadItems.appendChild(downloadItem);
}

function showDownloadSection() {
    downloadSection.style.display = 'block';
    
    if (processedFonts.length > 0) {
        addBatchDownloadButton();
    }
}

function addBatchDownloadButton() {
    if (processedFonts.length > 0) {
        downloadControls.style.display = 'block';
        
        updateDownloadButtonText();
    }
}

function updateDownloadButtonText() {
    const standaloneCount = fileSourceTracking.standalone.length;
    const folderCount = fileSourceTracking.fromFolders.length;
    const totalCount = standaloneCount + folderCount;
    
    let downloadAllText = '';
    let downloadAllHint = '';
    
    if (totalCount === 0) {
        downloadAllText = `<i class="fas fa-download"></i> ${translateText('下载字体文件')}`;
    } else if (standaloneCount > 0 && folderCount === 0) {
        downloadAllText = `<i class="fas fa-download"></i> ${translateText('下载所有字体文件')}`;
    } else if (standaloneCount === 0 && folderCount > 0) {
        downloadAllText = `<i class="fas fa-archive"></i> ${translateText('下载完整文件夹 (ZIP)')}`;
        downloadAllHint = `<small style="display: block; margin-top: 5px; color: #666;">${translateText('包含目录结构和所有非字体文件')}</small>`;
    } else {
        downloadAllText = `<i class="fas fa-download"></i> ${translateText('下载所有字体文件')}`;
        downloadAllHint = `<small style="display: block; margin-top: 5px; color: #666;">${standaloneCount}${translateText('个单独文件')} + ${folderCount}${translateText('个文件夹文件')} (ZIP)</small>`;
    }
    
    downloadAllBtn.innerHTML = `${downloadAllText}${downloadAllHint}`;
}

function downloadFont(index) {
    const font = processedFonts[index];
    const blob = new Blob([font.data], { type: 'font/truetype' });
    const url = URL.createObjectURL(blob);
    
    const a = document.createElement('a');
    a.href = url;
    a.download = font.name;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    console.log(`已下载: ${font.name}`);
}

function removeProcessedFont(index) {
    if (index < 0 || index >= processedFonts.length) {
        console.warn('无效的字体索引:', index);
        return;
    }
    
    const font = processedFonts[index];
    console.log(`删除处理后的字体: ${font.name}`);
    
    processedFonts.splice(index, 1);
    
    updateDownloadItemsDisplay();
    
    updateDownloadSectionTitle();
    
    if (downloadAllBtn && typeof updateDownloadButtonText === 'function') {
        updateDownloadButtonText();
    }
    
    if (processedFonts.length === 0) {
        downloadControls.style.display = 'none';
        downloadSection.style.display = 'none';
        console.log('📦 已隐藏处理后的字体卡片');
        
        scrollToUploadArea();
    }
    
    console.log(`已删除字体，剩余 ${processedFonts.length} 个字体`);
    
    showTemporaryMessage(`${translateText('已删除字体: ')}${font.name}`, 'success');
}

function updateDownloadItemsDisplay() {
    downloadItems.innerHTML = '';
    
    processedFonts.forEach((font, index) => {
        addSingleDownloadItem(font, index);
    });
}

async function downloadAllFonts() {
    console.log('=== downloadAllFonts 调试信息 ===');
    console.log('folderMode:', folderMode);
    console.log('folderStructure:', folderStructure);
    console.log('processedFonts.length:', processedFonts.length);
    console.log('fileSourceTracking:', fileSourceTracking);
    console.log('JSZip可用:', typeof JSZip !== 'undefined');
    console.log('================================');
    
    if (processedFonts.length === 1) {
        console.log('🔍 检测到只有一个处理后的字体，直接下载');
        downloadFont(0);
        return;
    }
    
    const standaloneCount = fileSourceTracking.standalone.length;
    const folderCount = fileSourceTracking.fromFolders.length;
    
    console.log(`📊 文件来源分析: ${standaloneCount}个单独文件, ${folderCount}个文件夹文件`);
    
    if (standaloneCount > 0 && folderCount === 0) {
        console.log('🔍 下载模式: 纯单独文件模式');
        console.log('开始下载所有文件...');
        
        for (let i = 0; i < processedFonts.length; i++) {
            await new Promise(resolve => setTimeout(resolve, 500));
            downloadFont(i);
        }
        
        console.log('所有文件下载完成！');
    } else if (standaloneCount === 0 && folderCount > 0) {
        console.log('🔍 下载模式: 纯文件夹ZIP模式');
        showZipProgress();
        await downloadFolderAsZip();
    } else {
        console.log('🔍 下载模式: 混合模式ZIP (单独文件 + 文件夹结构)');
        showZipProgress();
        await downloadMixedModeAsZip();
    }
}

async function downloadFolderAsZip() {
    console.log('=== downloadFolderAsZip 调试信息 ===');
    console.log('JSZip类型:', typeof JSZip);
    console.log('folderStructure:', folderStructure);
    console.log('folderStructure.files长度:', folderStructure.files ? folderStructure.files.length : 'undefined');
    console.log('================================');

    if (typeof JSZip === 'undefined') {
        console.error('❌ JSZip library not loaded, cannot create ZIP file');
        showTemporaryMessage('Please refresh page and try again, or check network connection', 'error');
        return;
    }

    if (!folderStructure.files || folderStructure.files.length === 0) {
        console.error('❌ No folder structure data found, cannot create ZIP');
        console.error(`🔍 Debug: folderStructure.files=${folderStructure.files ? folderStructure.files.length : 'null'}, folderMode=${folderMode}`);
        showTemporaryMessage('Please drag folder again and retry', 'warning');
        return;
    }

    console.log('📦 Creating ZIP file, preserving directory structure...');
    
    try {
        const zip = new JSZip();
        const outputFolderName = folderStructure.folderNames.length > 1 
            ? folderStructure.folderNames.join('_') 
            : folderStructure.name;
        console.log('Output folder name:', outputFolderName);
        console.log('Folder list:', folderStructure.folderNames);
        
        updateZipProgress(10, 'Creating directory structure...', `Creating ${folderStructure.directories.size} directories`);
        console.log('Starting directory creation, total:', folderStructure.directories.size);
        let dirCount = 0;
        folderStructure.directories.forEach(dirPath => {
            let fullPath;
            if (folderStructure.folderNames.length > 1) {
                fullPath = `${dirPath}/`;
            } else {
                const relativePath = dirPath.replace(new RegExp(`^${escapeRegExp(folderStructure.name)}/?`), '');
                if (relativePath) {
                    fullPath = `${relativePath}/`;
                } else {
                    return; 
                }
            }
            zip.folder(fullPath);
            console.log('Creating directory:', fullPath);
            dirCount++;
        });
        console.log(`✅ Created ${dirCount} directories (${folderStructure.folderNames.length > 1 ? 'Multi-folder structure' : 'Single-folder flattened'})`);
        
        updateZipProgress(20, 'Preparing font files...', `Mapping ${processedFonts.length} processed fonts`);
        const processedFontMap = new Map();
        processedFonts.forEach(font => {
            processedFontMap.set(font.name, font.data);
            console.log(`Mapped font: ${font.name} -> ${font.data ? font.data.byteLength + ' bytes' : 'null'}`);
        });
        console.log(`✅ Font mapping complete, total ${processedFontMap.size} fonts`);
        
        console.log('Adding files to ZIP, total:', folderStructure.files.length);
        let addedFiles = 0;
        let skippedFiles = 0;
        const totalFiles = folderStructure.files.length;
        
        for (let i = 0; i < folderStructure.files.length; i++) {
            const fileInfo = folderStructure.files[i];
            const { file, relativePath, isFont } = fileInfo;
            
            let finalPath;
            if (folderStructure.folderNames.length > 1) {
                finalPath = relativePath;
            } else {
                const flattenedPath = relativePath.replace(new RegExp(`^${escapeRegExp(folderStructure.name)}/?`), '');
                finalPath = flattenedPath || file.name; 
            }
            
            const fileProgress = 20 + (i / totalFiles) * 60;
            updateZipProgress(fileProgress, 'Adding files...', `Processing ${finalPath} (${i + 1}/${totalFiles})`);
            
            try {
                if (isFont) {
                    const processedData = processedFontMap.get(file.name);
                    if (processedData) {
                        zip.file(finalPath, processedData);
                        console.log(`✅ Added processed font: ${finalPath} (${processedData.byteLength} bytes)`);
                        addedFiles++;
                    } else {
                        console.log(`❌ Processed font data not found: ${file.name}`);
                        skippedFiles++;
                    }
                } else {
                    const fileData = await readFileAsArrayBuffer(file);
                    zip.file(finalPath, fileData);
                    console.log(`✅ Copied original file: ${finalPath} (${fileData.byteLength} bytes)`);
                    addedFiles++;
                }
            } catch (error) {
                console.error(`❌ Failed to process file ${finalPath}:`, error);
                skippedFiles++;
            }
        }
        
        console.log(`✅ File addition complete: Success ${addedFiles}, Skipped ${skippedFiles}`);
        console.log(`📦 Added ${addedFiles} files to ZIP`);
        
        updateZipProgress(80, 'Generating ZIP file...', 'Compressing data, please wait...');
        console.log('📦 Generating ZIP file...');
        console.log('Starting ZIP generation...');
        
        const zipBlob = await zip.generateAsync({
            type: 'blob',
            compression: 'DEFLATE',
            compressionOptions: {
                level: 6
            }
        });
        
        console.log(`✅ ZIP file generated, size: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log(`📦 ZIP file size: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        
        updateZipProgress(95, 'Preparing download...', `File size: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log('Starting ZIP download...');
        const url = URL.createObjectURL(zipBlob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${outputFolderName}.zip`;
        
        console.log('Download link:', url);
        console.log('Download filename:', `${outputFolderName}.zip`);
        
        updateZipProgress(100, 'Download Complete!', `${outputFolderName}.zip download started`);
        
        document.body.appendChild(a);
        console.log('Triggering download...');
        a.click();
        console.log('Download triggered');
        
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        const fontFiles = folderStructure.fontFiles.length;
        const nonFontFiles = totalFiles - fontFiles;
        
        console.log(`🎉 ZIP file download complete!`);
        console.log(`📊 Includes: ${fontFiles} processed font files, ${nonFontFiles} original files`);
        console.log(`📁 Single folder mode: Flattened structure, ready to use after unzip`);
        console.log('ZIP download process finished');
        
        hideZipProgress();
        
    } catch (error) {
        console.error(`❌ Failed to create ZIP file: ${error.message}`);
        console.error('ZIP creation error:', error);
        
        hideZipProgress();
    }
}

async function downloadMixedModeAsZip() {
    console.log('=== downloadMixedModeAsZip Debug Info ===');
    console.log('JSZip type:', typeof JSZip);
    console.log('fileSourceTracking:', fileSourceTracking);
    console.log('folderStructure:', folderStructure);
    console.log('processedFonts.length:', processedFonts.length);
    console.log('================================');

    if (typeof JSZip === 'undefined') {
        console.error('❌ JSZip library not loaded, cannot create ZIP file');
        showTemporaryMessage('Please refresh page and try again, or check network connection', 'error');
        return;
    }

    console.log('📦 Creating mixed mode ZIP file...');
    
    try {
        const zip = new JSZip();
        
        let nameComponents = [];
        
        if (fileSourceTracking.standalone.length > 0) {
            const standaloneNames = fileSourceTracking.standalone.map(file => {
                const nameWithoutExt = file.name.replace(/\.[^/.]+$/, ''); 
                return nameWithoutExt;
            });
            nameComponents.push(...standaloneNames);
        }
        
        if (folderStructure.folderNames.length > 0) {
            nameComponents.push(...folderStructure.folderNames);
        }
        
        const outputFolderName = nameComponents.length > 0 
            ? nameComponents.join('_')
            : 'processed_fonts';
            
        console.log('Output folder name:', outputFolderName);
        console.log('Standalone files:', fileSourceTracking.standalone.map(f => f.name));
        console.log('Folder list:', folderStructure.folderNames);
        console.log('Name components:', nameComponents);
        
        updateZipProgress(10, 'Creating directory structure...', `Creating ${folderStructure.directories.size} directories`);
        console.log('Starting directory creation, total:', folderStructure.directories.size);
        let dirCount = 0;
        folderStructure.directories.forEach(dirPath => {
            const fullPath = `${dirPath}/`;
            zip.folder(fullPath);
            dirCount++;
            if (dirCount <= 5) { 
                console.log('Creating directory:', fullPath);
            }
        });
        console.log(`✅ Created ${dirCount} directories`);
        
        updateZipProgress(20, 'Preparing font files...', `Mapping ${processedFonts.length} processed fonts`);
        const processedFontMap = new Map();
        processedFonts.forEach(font => {
            processedFontMap.set(font.name, font.data);
            console.log(`Mapped font: ${font.name} -> ${font.data ? font.data.byteLength + ' bytes' : 'null'}`);
        });
        console.log(`✅ Font mapping complete, total ${processedFontMap.size} fonts`);

        console.log('Adding standalone files to ZIP root, total:', fileSourceTracking.standalone.length);
        let addedStandaloneFiles = 0;
        
        for (let i = 0; i < fileSourceTracking.standalone.length; i++) {
            const file = fileSourceTracking.standalone[i];
            
            const fileProgress = 20 + (i / fileSourceTracking.standalone.length) * 20;
            updateZipProgress(fileProgress, 'Adding standalone files...', `Processing ${file.name} (${i + 1}/${fileSourceTracking.standalone.length})`);
            
            try {
                const processedData = processedFontMap.get(file.name);
                if (processedData) {
                    zip.file(file.name, processedData);
                    console.log(`✅ Added standalone file to root: ${file.name} (${processedData.byteLength} bytes)`);
                    addedStandaloneFiles++;
                } else {
                    console.log(`❌ Processed data not found for standalone file: ${file.name}`);
                }
            } catch (error) {
                console.error(`❌ Failed to process standalone file ${file.name}:`, error);
            }
        }
        console.log(`✅ Standalone files added: Success ${addedStandaloneFiles}`);
        
        console.log('Adding folder files to ZIP, total:', folderStructure.files.length);
        let addedFolderFiles = 0;
        let skippedFiles = 0;
        const totalFolderFiles = folderStructure.files.length;
        
        for (let i = 0; i < folderStructure.files.length; i++) {
            const fileInfo = folderStructure.files[i];
            const { file, relativePath, isFont } = fileInfo;
            
            const fileProgress = 40 + (i / totalFolderFiles) * 40;
            updateZipProgress(fileProgress, 'Adding folder files...', `Processing ${relativePath} (${i + 1}/${totalFolderFiles})`);
            
            try {
                if (isFont) {
                    const processedData = processedFontMap.get(file.name);
                    if (processedData) {
                        zip.file(relativePath, processedData);
                        console.log(`✅ Added processed font to folder: ${relativePath} (${processedData.byteLength} bytes)`);
                        addedFolderFiles++;
                    } else {
                        console.log(`❌ Processed data not found for folder file: ${file.name}`);
                        skippedFiles++;
                    }
                } else {
                    const fileData = await readFileAsArrayBuffer(file);
                    zip.file(relativePath, fileData);
                    console.log(`✅ Copied original file to folder: ${relativePath} (${fileData.byteLength} bytes)`);
                    addedFolderFiles++;
                }
            } catch (error) {
                console.error(`❌ Failed to process file ${relativePath}:`, error);
                skippedFiles++;
            }
        }
        console.log(`✅ Folder files added: Success ${addedFolderFiles}, Skipped ${skippedFiles}`);
        
        updateZipProgress(80, 'Generating ZIP file...', 'Compressing data, please wait...');
        console.log('📦 Generating ZIP file...');
        console.log('Starting ZIP generation...');
        
        const zipBlob = await zip.generateAsync({
            type: 'blob',
            compression: 'DEFLATE',
            compressionOptions: {
                level: 6
            }
        });
        
        console.log(`✅ ZIP file generated, size: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log(`📦 ZIP file size: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        
        updateZipProgress(95, 'Preparing download...', `File size: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log('Starting ZIP download...');
        const url = URL.createObjectURL(zipBlob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${outputFolderName}.zip`;
        
        console.log('Download link:', url);
        console.log('Download filename:', `${outputFolderName}.zip`);
        
        updateZipProgress(100, 'Download Complete!', `${outputFolderName}.zip download started`);
        
        document.body.appendChild(a);
        console.log('Triggering download...');
        a.click();
        console.log('Download triggered');
        
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        const totalAdded = addedStandaloneFiles + addedFolderFiles;
        console.log(`🎉 Mixed mode ZIP download complete!`);
        console.log(`📊 Includes: ${totalAdded} total files (${addedStandaloneFiles} standalone, ${addedFolderFiles} from folders)`);
        console.log('ZIP download process finished');
        
        hideZipProgress();
        
    } catch (error) {
        console.error(`❌ Failed to create ZIP file: ${error.message}`);
        console.error('ZIP creation error:', error);
        
        hideZipProgress();
    }
}

