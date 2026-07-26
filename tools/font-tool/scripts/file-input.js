let dragCounter = 0;

function initDragAndDrop() {
    console.log('Initializing drag and drop');
    console.log('dragOverlay:', dragOverlay);
    console.log('uploadArea:', uploadArea);
    
    if (!dragOverlay) {
        console.error('Drag overlay element not found!');
        return;
    }
    
    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        document.addEventListener(eventName, preventDefaults, false);
    });

    document.addEventListener('dragenter', handleDragEnter, false);
    document.addEventListener('dragleave', handleDragLeave, false);
    document.addEventListener('dragover', handleDragOver, false);
    document.addEventListener('drop', handlePageDrop, false);
    
    console.log('Page-wide drag listeners added');

    if (uploadArea) {
        ['dragenter', 'dragover'].forEach(eventName => {
            uploadArea.addEventListener(eventName, highlight, false);
        });

        ['dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, unhighlight, false);
        });
    }

    dragOverlay.addEventListener('click', function(e) {
        if (e.target === dragOverlay) {
            hideDragOverlay();
        }
    });

    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape' && dragOverlay.classList.contains('active')) {
            hideDragOverlay();
        }
    });
    
    console.log('Drag functionality initialized');

    initDebugOverlayToggle();
}

function initDebugOverlayToggle() {
    if (window.location.search.includes('debug=true')) {
        const testBtn = document.createElement('button');
        testBtn.textContent = 'Test Overlay';
        testBtn.style.position = 'fixed';
        testBtn.style.top = '10px';
        testBtn.style.right = '10px';
        testBtn.style.zIndex = '10000';
        testBtn.onclick = () => {
            if (dragOverlay.classList.contains('active')) {
                hideDragOverlay();
            } else {
                showDragOverlay();
            }
        };
        document.body.appendChild(testBtn);
    }
}

function preventDefaults(e) {
    e.preventDefault();
    e.stopPropagation();
}

function handleDragEnter(e) {
    dragCounter++;
    console.log('Drag enter, counter:', dragCounter);
    
    if (e.dataTransfer && e.dataTransfer.types) {
        const hasFiles = e.dataTransfer.types.includes('Files');
        console.log('Drag types:', e.dataTransfer.types, 'Has files:', hasFiles);
        
        if (hasFiles) {
            showDragOverlay();
            console.log('Files detected, showing overlay');
        }
    }
}

function handleDragLeave(e) {
    dragCounter--;
    
    if (dragCounter <= 0) {
        dragCounter = 0;
        hideDragOverlay();
    }
}

function handleDragOver(e) {
    if (e.dataTransfer && e.dataTransfer.types && e.dataTransfer.types.includes('Files')) {
        showDragOverlay();
    }
}

function handlePageDrop(e) {
    dragCounter = 0;
    hideDragOverlay();
    
    handleDrop(e);
}

function showDragOverlay() {
    console.log('Show drag overlay');
    if (dragOverlay) {
        dragOverlay.classList.add('active');
        document.body.style.overflow = 'hidden';
        console.log('Overlay activated');
    } else {
        console.error('dragOverlay element not found');
    }
}

function hideDragOverlay() {
    console.log('Hide drag overlay');
    if (dragOverlay) {
        dragOverlay.classList.remove('active');
        document.body.style.overflow = '';
        console.log('Overlay hidden');
    }
}

function highlight(e) {
    uploadArea.classList.add('drag-over');
}

function unhighlight(e) {
    uploadArea.classList.remove('drag-over');
}

async function handleDrop(e) {
    const dt = e.dataTransfer;
    
    let currentDropFolderMode = false;
    let currentDropFolderStructure = {
        name: '',
        files: [],
        fontFiles: [],
        directories: new Set()
    };
    
    if (dt.items && dt.items.length > 0) {
        console.log('Scanning dropped content...');
        console.log('Dropped items count:', dt.items.length);
        
        const files = [];
        const scanPromises = [];
        
        let mainFolderEntry = null;
        for (let i = 0; i < dt.items.length; i++) {
            const item = dt.items[i];
            console.log(`Item ${i}:`, item.kind, item.type);
            
            if (item.kind === 'file') {
                const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : item.getAsEntry();
                if (entry) {
                    console.log(`Entry ${i}:`, entry.name, entry.isDirectory ? 'Directory' : 'File');
                    
                    if (entry.isDirectory) {
                        currentDropFolderMode = true;
                        currentDropFolderStructure.name = entry.name;
                        mainFolderEntry = entry;
                        console.log(`📁 Folder mode detected: ${entry.name}`);
                        console.log('Main folder entry:', entry.name);
                        break; 
                    }
                }
            }
        }
        
        if (mainFolderEntry) {
            console.log('Scanning main folder:', mainFolderEntry.name);
            scanPromises.push(scanEntryForCurrentDrop(mainFolderEntry, files, currentDropFolderStructure));
        } else {
            for (let i = 0; i < dt.items.length; i++) {
                const item = dt.items[i];
                if (item.kind === 'file') {
                    const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : item.getAsEntry();
                    if (entry && entry.isFile) {
                        scanPromises.push(scanEntryForCurrentDrop(entry, files, currentDropFolderStructure));
                    } else {
                        const file = item.getAsFile();
                        if (file) files.push(file);
                    }
                }
            }
        }
        
        await Promise.all(scanPromises);
        
        if (files.length > 0) {
            if (currentDropFolderMode) {
                folderMode = true;
                if (!folderStructure.name) {
                    folderStructure.name = currentDropFolderStructure.name;
                }
                if (!folderStructure.folderNames.includes(currentDropFolderStructure.name)) {
                    folderStructure.folderNames.push(currentDropFolderStructure.name);
                }
                folderStructure.files.push(...currentDropFolderStructure.files);
                folderStructure.fontFiles.push(...currentDropFolderStructure.fontFiles);
                currentDropFolderStructure.directories.forEach(dir => folderStructure.directories.add(dir));
                
                files.forEach(file => {
                    if (!fileSourceTracking.fromFolders.some(f => f.name === file.name && f.size === file.size)) {
                        fileSourceTracking.fromFolders.push(file);
                    }
                });
            } else {
                files.forEach(file => {
                    if (!fileSourceTracking.standalone.some(f => f.name === file.name && f.size === file.size)) {
                        fileSourceTracking.standalone.push(file);
                    }
                });
            }
            
            const totalFiles = currentDropFolderMode ? currentDropFolderStructure.files.length : files.length;
            const nonFontFiles = totalFiles - files.length;
            
            updateScanInfo(totalFiles, files.length, nonFontFiles, currentDropFolderMode);
            
            console.log(`📁 Scan complete, found ${totalFiles} files (${files.length} font files, ${nonFontFiles} other files)`);
            
            if (currentDropFolderMode) {
                console.log(`📁 Folder mode active: directory structure will be preserved`);
                console.log(`🔍 Debug: Dirs=${currentDropFolderStructure.directories.size}, Files=${currentDropFolderStructure.files.length}`);
            }
            
            handleFiles(files);
        } else {
            console.warn('No files found in dropped content');
        }
    } else {
        const files = dt.files;
        handleFiles(files);
    }
}

function initFileInput() {
    fileInput.addEventListener('change', function(e) {
        handleFiles(e.target.files);
    });
    
    if (uploadArea) {
        uploadArea.addEventListener('click', function(e) {
            if (!e.target.closest('button')) {
                fileInput.click();
            }
        });
        console.log('Upload area click event bound');
    } else {
        console.error('Upload area element not found!');
    }
}

function initPasteSupport() {
    document.addEventListener('paste', async function(e) {
        console.log('Paste event detected');
        
        const clipboardData = e.clipboardData || window.clipboardData;
        if (!clipboardData) {
            console.log('Cannot access clipboard data');
            return;
        }
        
        folderMode = false;
        folderStructure = {
            name: '',
            folderNames: [],
            files: [],
            fontFiles: [],
            directories: new Set()
        };
        
        let files = [];
        let foundFolderStructure = false;
        
        if (clipboardData.items && clipboardData.items.length > 0) {
            console.log(`Found ${clipboardData.items.length} items in clipboard`);
            
            for (let i = 0; i < clipboardData.items.length; i++) {
                const item = clipboardData.items[i];
                console.log(`Item ${i}:`, item.kind, item.type);
                
                if (item.kind === 'file') {
                    const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
                    if (entry) {
                        console.log(`Entry ${i}:`, entry.name, entry.isDirectory ? 'Directory' : 'File');
                        
                        if (entry.isDirectory) {
                            console.log(`📁 Folder detected: ${entry.name}`);
                            folderMode = true;
                            folderStructure.name = entry.name;
                            foundFolderStructure = true;
                            
                            e.preventDefault();
                            
                            try {
                                await scanEntry(entry, files);
                                
                                if (files.length > 0) {
                                    const totalFiles = folderStructure.files.length;
                                    const nonFontFiles = totalFiles - files.length;
                                    
                                    console.log(`📁 Folder scan complete: ${totalFiles} files (${files.length} font files)`);
                                    
                                    updateScanInfo(totalFiles, files.length, nonFontFiles, folderMode);
                                    
                                    showTemporaryMessage(`Added folder via paste "${entry.name}", contains ${files.length} font files`, 'success');
                                    
                                    handleFiles(files);
                                } else {
                                    showTemporaryMessage(`No font files found in folder "${entry.name}"`, 'warning');
                                }
                            } catch (error) {
                                console.error('Folder scan failed:', error);
                                showTemporaryMessage('Folder processing failed, please try dragging the folder', 'error');
                            }
                            return; 
                        } else if (entry.isFile) {
                            try {
                                await scanEntry(entry, files);
                            } catch (error) {
                                console.log('File processing failed, trying fallback');
                            }
                        }
                    }
                }
            }
        }
        
        if (!foundFolderStructure) {
            const clipboardFiles = clipboardData.files;
            if (!clipboardFiles || clipboardFiles.length === 0) {
                console.log('No files in clipboard');
                return;
            }
            
            console.log(`Found ${clipboardFiles.length} files in clipboard`);
            
            const fontFiles = Array.from(clipboardFiles).filter(file => {
                const extension = file.name.toLowerCase().split('.').pop();
                return ['ttf', 'otf', 'woff', 'woff2'].includes(extension);
            });
            
            if (fontFiles.length > 0) {
                console.log(`Detected ${fontFiles.length} font files, starting processing`);
                
                e.preventDefault();
                
                showTemporaryMessage(`Added ${fontFiles.length} font files via paste`, 'success');
                
                handleFiles(fontFiles);
            } else {
                console.log('No font files in clipboard');
                if (clipboardFiles.length > 0) {
                    showTemporaryMessage('Files in clipboard are not supported font formats', 'warning');
                }
            }
        }
    });

    
    console.log('Global paste support initialized (with folder support)');
}

async function scanEntry(entry, files, basePath = '') {
    console.log(`Scanning entry: ${entry.name}, Type: ${entry.isDirectory ? 'Directory' : 'File'}, Base Path: ${basePath}`);
    
    if (entry.isFile) {
        return new Promise((resolve) => {
            entry.file((file) => {
                const extension = file.name.toLowerCase().split('.').pop();
                if (['ttf', 'otf', 'woff', 'woff2'].includes(extension)) {
                    files.push(file);
                    
                    if (folderMode) {
                        folderStructure.files.push(file);
                        folderStructure.fontFiles.push(file);
                        
                        const fileFullPath = basePath ? `${basePath}/${file.name}` : file.name;
                        console.log(`Found font file: ${fileFullPath}`);
                        
                        file.fullPath = fileFullPath;
                        
                        if (basePath) {
                            folderStructure.directories.add(basePath);
                        }
                        
                        if (!fileSourceTracking.fromFolders.some(f => f.name === file.name && f.size === file.size)) {
                            fileSourceTracking.fromFolders.push(file);
                        }
                    }
                } else {
                    if (folderMode) {
                        folderStructure.files.push(file);
                        
                        const fileFullPath = basePath ? `${basePath}/${file.name}` : file.name;
                        file.fullPath = fileFullPath;
                        
                        if (basePath) {
                            folderStructure.directories.add(basePath);
                        }
                    }
                }
                resolve();
            }, (error) => {
                console.error(`Failed to read file ${entry.name}:`, error);
                resolve();
            });
        });
    } else if (entry.isDirectory) {
        const dirReader = entry.createReader();
        
        const currentPath = basePath ? `${basePath}/${entry.name}` : entry.name;
        if (folderMode) {
            folderStructure.directories.add(currentPath);
        }
        
        const readEntries = async () => {
            const entries = await new Promise((resolve) => {
                dirReader.readEntries((results) => resolve(results), (error) => {
                    console.error(`Failed to read directory ${entry.name}:`, error);
                    resolve([]);
                });
            });
            
            if (entries.length > 0) {
                await Promise.all(entries.map(e => scanEntry(e, files, currentPath)));
                await readEntries(); 
            }
        };
        
        await readEntries();
    }
}

async function scanEntryForCurrentDrop(entry, files, targetFolderStructure, basePath = '') {
    console.log(`Scanning drop entry: ${entry.name}, Type: ${entry.isDirectory ? 'Directory' : 'File'}, Base Path: ${basePath}`);
    
    if (entry.isFile) {
        return new Promise((resolve) => {
            entry.file((file) => {
                const extension = file.name.toLowerCase().split('.').pop();
                
                const relativePath = basePath ? `${basePath}/${file.name}` : file.name;
                file.fullPath = relativePath; 
                
                const fileInfo = file; 
                
                if (['ttf', 'otf', 'woff', 'woff2'].includes(extension)) {
                    files.push(file);
                    targetFolderStructure.fontFiles.push(fileInfo);
                    console.log(`✅ Font file: ${relativePath}`);
                } else {
                    console.log(`📄 Normal file: ${relativePath}`);
                }
                
                targetFolderStructure.files.push(fileInfo);
                
                if (basePath) {
                    targetFolderStructure.directories.add(basePath);
                }
                
                resolve();
            }, () => resolve()); 
        });
    } else if (entry.isDirectory) {
        const currentPath = basePath ? `${basePath}/${entry.name}` : entry.name;
        console.log(`Entering directory: ${entry.name}, Full path: ${currentPath}`);
        targetFolderStructure.directories.add(currentPath);
        
        return new Promise((resolve) => {
            const reader = entry.createReader();
            const readEntries = async () => {
                reader.readEntries(async (entries) => {
                    if (entries.length === 0) {
                        resolve();
                        return;
                    }
                    
                    console.log(`Directory ${entry.name} contains ${entries.length} items`);
                    const subPromises = entries.map(subEntry => scanEntryForCurrentDrop(subEntry, files, targetFolderStructure, currentPath));
                    await Promise.all(subPromises);
                    
                    await readEntries();
                }, () => resolve()); 
            };
            readEntries();
        });
    }
}

function handleFiles(files) {
    const fontFiles = Array.from(files).filter(file => {
        const extension = file.name.toLowerCase().split('.').pop();
        return ['ttf', 'otf', 'woff', 'woff2'].includes(extension);
    });

    if (fontFiles.length === 0) {
        console.warn('未检测到有效的字体文件，请选择 .ttf、.otf、.woff 或 .woff2 格式的文件。💡 提示：可以直接拖拽包含字体文件的文件夹！');
        return;
    }

    let addedCount = 0;
    fontFiles.forEach(file => {
        if (!selectedFiles.some(f => f.name === file.name && f.size === file.size)) {
            selectedFiles.push(file);
            addedCount++;
        }
    });

    updateFileList();
    
    if (!folderMode && selectedFiles.length > 0) {
        updateScanInfo(selectedFiles.length, selectedFiles.length, 0, false);
    }
    
    if (addedCount > 0) {
        console.log(`✅ 成功添加 ${addedCount} 个字体文件，总计 ${selectedFiles.length} 个文件待处理。`);
        
        if (fontFiles.length > addedCount) {
            console.log(`📁 文件夹模式：已自动扫描并添加字体文件（与本地版本保持一致）`);
        }
    } else {
        console.log(`ℹ️ 所有字体文件都已存在，未添加新文件。`);
    }
}

