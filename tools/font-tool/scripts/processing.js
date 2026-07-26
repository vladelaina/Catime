async function startProcessing() {
    if (selectedFiles.length === 0) {
        showTemporaryMessage(translateText('请先选择要处理的字体文件！'), 'warning');
        scrollToUploadArea();
        return;
    }

    const characters = charactersInput.value.trim();
    if (!characters) {
        showTemporaryMessage(translateText('请输入要保留的字符！'), 'warning');
        return;
    }

    if (!pythonReady && typeof opentype === 'undefined') {
        showTemporaryMessage(translateText('字体处理引擎尚未就绪，请稍候再试'), 'error');
        return;
    }

    processingStartTime = Date.now();
    
    processBtn.disabled = true;
    processBtn.innerHTML = `<i class="fas fa-spinner fa-spin"></i> ${translateText('处理中...')}`;
    progressContainer.style.display = 'block';
    downloadSection.style.display = 'block'; 
    downloadItems.innerHTML = ''; 
    
    createTimingDisplay();
    
    processedFonts = [];
    
    const downloadTitle = downloadSection.querySelector('h2');
    downloadTitle.innerHTML = `<i class="fas fa-download"></i> ${translateText('处理后的字体')} <span style="font-size: 14px; color: #666; font-weight: normal;">(${translateText('处理中...')})</span>`;
    
    const engineType = pythonReady ? '专业处理引擎' : 'JavaScript OpenType.js';
    console.log(`开始使用 ${engineType} (严格清理模式) 处理 ${selectedFiles.length} 个字体文件...`);
    console.log(`保留字符: ${characters}`);
    console.log(`🔧 严格清理模式：将彻底移除复合字形和多余字符`);

    try {
        for (let i = 0; i < selectedFiles.length; i++) {
            const file = selectedFiles[i];
            console.log(`正在处理: ${file.name} (${(file.size / 1024 / 1024).toFixed(1)}MB)`);
            
            updateProgress(i, selectedFiles.length);
            
            try {
                const processedFont = await processFont(file, characters);
                processedFonts.push(processedFont);
                console.log(`✅ 完成: ${file.name}`);
                
                addSingleDownloadItem(processedFont, processedFonts.length - 1);
                updateDownloadSectionTitle(); 
                
                if (processedFonts.length === 1) {
                    addBatchDownloadButton();
                }
                
                if (file.size > 1024 * 1024) { 
                    await new Promise(resolve => setTimeout(resolve, 100));
                }
                
            } catch (error) {
                console.error(`❌ 处理失败 ${file.name}: ${error.message}`);
                console.error('Font processing error:', error);
            }
        }

        updateProgress(selectedFiles.length, selectedFiles.length);
        console.log(`🎉 所有字体处理完成！成功处理 ${processedFonts.length}/${selectedFiles.length} 个文件`);
        
        if (processedFonts.length > 0) {
            showDownloadSection();
            
            scrollToDownloadSection();
            
            const successCount = processedFonts.length;
            const totalCount = selectedFiles.length;
            
            if (successCount === totalCount) {
                showTemporaryMessage(`${translateText('所有字体处理完成！成功处理')} ${successCount}${translateText('个文件')}`, 'success');
            } else {
                showTemporaryMessage(`${translateText('字体处理完成！成功处理')} ${successCount}/${totalCount}${translateText('个文件')}`, 'warning');
            }
        } else {
            showTemporaryMessage(translateText('字体处理失败，没有成功处理任何文件'), 'error');
            downloadSection.style.display = 'none';
            downloadItems.innerHTML = '';
            downloadControls.style.display = 'none';
            console.log('📦 处理失败，已隐藏处理后的字体卡片');
        }

    } catch (error) {
        console.error(`处理过程中发生错误: ${error.message}`);
        console.error('Processing error:', error);
        
        downloadSection.style.display = 'none';
        downloadItems.innerHTML = '';
        downloadControls.style.display = 'none';
        console.log('📦 处理异常，已隐藏处理后的字体卡片');
        
        showTemporaryMessage(translateText('字体处理过程中发生错误，请重试'), 'error');
    } finally {
        stopTimingAndShowResult();
        
        processBtn.disabled = false;
        processBtn.innerHTML = `<i class="fas fa-rocket"></i> ${translateText('开始处理字体')}`;
    }
}

async function processFont(file, characters) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        
        reader.onload = async function(e) {
            try {
                const arrayBuffer = e.target.result;
                
                let subsetFont;
                
                if (pythonReady && pyodide) {
                    subsetFont = await createPythonSubset(arrayBuffer, characters);
                } else if (typeof opentype !== 'undefined') {
                    subsetFont = await createOpenTypeSubset(arrayBuffer, characters);
                } else {
                    throw new Error('没有可用的字体处理引擎');
                }
                
                resolve({
                    name: file.name,  
                    data: subsetFont.buffer,
                    originalSize: file.size,
                    newSize: subsetFont.buffer.byteLength
                });
                
            } catch (error) {
                reject(error);
            }
        };
        
        reader.onerror = function() {
            reject(new Error('文件读取失败'));
        };
        
        reader.readAsArrayBuffer(file);
    });
}

async function createPythonSubset(fontBuffer, characters) {
    try {
        const uint8Array = new Uint8Array(fontBuffer);
        
        let base64Data;
        try {
            const binaryString = String.fromCharCode.apply(null, uint8Array);
            base64Data = btoa(binaryString);
        } catch (rangeError) {
            console.log('文件较大，使用分块处理...');
            
            let binaryString = '';
            const chunkSize = 8192; 
            
            for (let i = 0; i < uint8Array.length; i += chunkSize) {
                const chunk = uint8Array.slice(i, i + chunkSize);
                for (let j = 0; j < chunk.length; j++) {
                    binaryString += String.fromCharCode(chunk[j]);
                }
            }
            
            base64Data = btoa(binaryString);
        }
        
        if (!base64Data || base64Data.length === 0) {
            throw new Error('Base64编码失败');
        }
        
        try {
            const decoded = atob(base64Data);
            const expectedLength = uint8Array.length;
            if (decoded.length !== expectedLength) {
                throw new Error(`Base64编码验证失败：期望长度${expectedLength}，实际长度${decoded.length}`);
            }
            console.log('✅ Base64编码验证通过');
        } catch (validationError) {
            console.error('❌ Base64编码验证失败:', validationError);
            throw new Error(`Base64编码验证失败：${validationError.message}`);
        }
        
        console.log(`设置处理变量: font_data_b64(${base64Data.length}字符), chars_to_keep(${characters})`);
        
        try {
            pyodide.globals.set('font_data_b64', base64Data);
            pyodide.globals.set('chars_to_keep', characters);
        } catch (error) {
            if (error.message.includes('out of memory') || error.message.includes('stack')) {
                throw new Error(`字体文件过大(${(fontBuffer.byteLength / 1024 / 1024).toFixed(1)}MB)，建议处理较小的文件`);
            }
            throw error;
        }
        
        const var_check = pyodide.runPython(`
f"处理引擎收到的变量: font_data_b64长度={len(font_data_b64)}, chars_to_keep='{chars_to_keep}'"
        `);
        console.log('处理引擎变量验证:', var_check);
        
        const originalConsole = pyodide.runPython(`
import sys
from io import StringIO

capture_output = StringIO()
original_stdout = sys.stdout
sys.stdout = capture_output
        `);
        
        let result;
        try {
            result = pyodide.runPython(`
result = subset_font(font_data_b64, chars_to_keep)

sys.stdout = original_stdout
captured_output = capture_output.getvalue()
capture_output.close()

result['debug_output'] = captured_output
result
            `);
        } catch (processingError) {
            console.error('处理引擎代码执行失败:', processingError);
            throw new Error(`处理引擎代码执行失败: ${processingError.message}`);
        }
        
        if (!result) {
            console.error('处理引擎返回的结果无效:', result);
            throw new Error('处理引擎返回了无效的结果');
        }
        
        console.log('处理引擎结果对象类型:', typeof result);
        console.log('处理引擎结果对象:', result);
        
        let success, debug_output, error_detail, error, message, data, size;
        
        try {
            success = result.get ? result.get('success') : result.success;
            debug_output = result.get ? result.get('debug_output') : result.debug_output;
            error_detail = result.get ? result.get('error_detail') : result.error_detail;
            error = result.get ? result.get('error') : result.error;
            message = result.get ? result.get('message') : result.message;
            data = result.get ? result.get('data') : result.data;
            size = result.get ? result.get('size') : result.size;
            
            console.log('解析的属性:', { success, message, error, hasData: !!data, hasDebugOutput: !!debug_output });
            
        } catch (accessError) {
            console.error('访问Proxy属性失败:', accessError);
            
            try {
                const jsResult = result.toJs ? result.toJs() : result;
                console.log('转换后的JS对象:', jsResult);
                success = jsResult.success;
                debug_output = jsResult.debug_output;
                error_detail = jsResult.error_detail;
                error = jsResult.error;
                message = jsResult.message;
                data = jsResult.data;
                size = jsResult.size;
            } catch (convertError) {
                console.error('转换Proxy失败:', convertError);
                throw new Error('无法解析处理引擎返回的结果');
            }
        }
        
        if (debug_output) {
            console.log('=== 处理引擎调试输出 ===');
            console.log(debug_output);
            console.log('=== 调试输出结束 ===');
            
            const debugLines = debug_output.split('\n');
            debugLines.forEach(line => {
                if (line.includes('[DEBUG]') || line.includes('[ERROR]') || line.includes('[WARNING]')) {
                    const cleanLine = line.replace(/^\[.*?\]\s*/, ''); 
                    console.log(`🔍 ${cleanLine}`);
                }
            });
        } else {
            console.warn('没有收到处理引擎调试输出');
        }
        
        if (!success) {
            console.error('处理引擎处理失败，详细信息:', { success, message, error, error_detail });
            
            if (error_detail) {
                console.error('处理引擎详细错误:', error_detail);
                
                if (error_detail.includes('AssertionError')) {
                    console.error('❌ 字体文件数据损坏或格式不兼容');
                    if (error_detail.includes('assert len(data) == self.length')) {
                        console.warn('💡 建议：这可能是Base64编码问题，已自动修复，请重试');
                    }
                } else if (error_detail.includes('cmap')) {
                    console.error('❌ 字体字符映射表(cmap)读取失败');
                    console.warn('💡 建议：请检查字体文件是否完整或选择其他字体');
                } else if (error_detail.includes('Memory')) {
                    console.error('❌ 内存不足，文件过大');
                    console.warn('💡 建议：请处理较小的字体文件（<5MB）');
                } else if (error_detail.includes('base64')) {
                    console.error('❌ Base64编码解码失败');
                    console.warn('💡 建议：文件可能损坏，请重新选择文件');
                }
            }
            
            if (error) {
                console.error('处理引擎错误:', error);
            }
            
            const errorMsg = message || error || '字体处理失败，请查看详细日志';
            throw new Error(errorMsg);
        }
        
        result = { success, debug_output, error_detail, error, message, data, size };
        
        const binaryString = atob(result.data);
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }
        
        console.log(`JavaScript收到的字体数据大小: ${bytes.length} 字节`);
        
        if (bytes.length < 100) {
            throw new Error(`生成的字体文件过小(${bytes.length}字节)，可能损坏`);
        }
        
        const header = new DataView(bytes.buffer, 0, Math.min(12, bytes.length));
        const signature = header.getUint32(0, false);
        
        const headerBytes = new Uint8Array(bytes.buffer, 0, Math.min(12, bytes.length));
        const headerHex = Array.from(headerBytes).map(b => b.toString(16).padStart(2, '0')).join(' ');
        console.log(`JavaScript验证文件头: ${headerHex}`);
        
        if (signature === 0x00010000) {
            console.log('  ✅ JavaScript验证：有效的TTF格式字体');
        } else if (signature === 0x4F54544F) {
            console.log('  ✅ JavaScript验证：有效的OTF格式字体');
        } else {
            const hex = signature.toString(16).padStart(8, '0');
            console.warn(`  ⚠️ JavaScript验证：意外的文件签名: 0x${hex}`);
            console.error('文件头详情:', {
                signature: `0x${hex}`,
                expectedTTF: '0x00010000',
                expectedOTF: '0x4f54544f',
                headerHex: headerHex
            });
        }
        
        if (bytes.length >= 12) {
            const numTables = header.getUint16(4, false);
            console.log(`字体表数量: ${numTables}`);
            
            if (numTables === 0 || numTables > 50) {
                console.warn(`  ⚠️ 字体表数量异常: ${numTables}`);
            } else {
                console.log(`  ✅ 字体表数量正常: ${numTables}`);
            }
        }
        
        console.log(`  ✅ 专业引擎处理成功: ${result.message}`);
        
        return { buffer: bytes.buffer };
        
    } catch (error) {
        console.error(`  ❌ 专业引擎处理失败: ${error.message}`);
        console.error('字体处理引擎错误:', error);
        throw error;
    }
}

async function createOpenTypeSubset(fontBuffer, characters) {
    try {
        const font = opentype.parse(fontBuffer);
        
        if (!font || !font.glyphs) {
            throw new Error('无法解析字体文件');
        }
        
        const glyphsToKeep = [];
        const charToGlyph = {};
        
        if (font.glyphs.glyphs[0]) {
            glyphsToKeep.push(font.glyphs.glyphs[0]);
        }
        
        let foundChars = 0;
        for (const char of characters) {
            const charCode = char.charCodeAt(0);
            const glyph = font.charToGlyph(char);
            
            if (glyph && glyph.index > 0) {
                if (!glyphsToKeep.find(g => g.index === glyph.index)) {
                    glyphsToKeep.push(glyph);
                    foundChars++;
                }
                charToGlyph[charCode] = glyph;
            }
        }
        
        if (foundChars === 0) {
            throw new Error('在字体中未找到任何指定字符');
        }
        
        const newFont = new opentype.Font({
            familyName: (font.names?.fontFamily?.en || 'SimplifiedFont'),
            styleName: (font.names?.fontSubfamily?.en || 'Regular'),
            unitsPerEm: font.unitsPerEm || 1000,
            ascender: font.ascender || 800,
            descender: font.descender || -200,
            glyphs: glyphsToKeep
        });
        
        if (!newFont.encoding) newFont.encoding = {};
        if (!newFont.encoding.cmap) newFont.encoding.cmap = {};
        if (!newFont.encoding.cmap.glyphIndexMap) newFont.encoding.cmap.glyphIndexMap = {};
        
        Object.keys(charToGlyph).forEach(charCode => {
            const glyph = charToGlyph[charCode];
            if (glyph) {
                newFont.encoding.cmap.glyphIndexMap[parseInt(charCode)] = glyph.index;
            }
        });
        
        const buffer = newFont.toArrayBuffer();
        
        if (!buffer || buffer.byteLength === 0) {
            throw new Error('生成的字体文件为空');
        }
        
        console.log(`  📋 JavaScript备用处理完成，包含 ${foundChars} 个字符`);
        
        return { buffer };
        
    } catch (error) {
        console.error(`  ❌ JavaScript处理失败: ${error.message}`);
        throw error;
    }
}

