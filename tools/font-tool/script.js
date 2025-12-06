let selectedFiles = [];
let processedFonts = [];
let pyodide = null;
let pythonReady = false;

let folderMode = false;
let folderStructure = {
    name: '',
    folderNames: [],
    files: [],
    fontFiles: [],
    directories: new Set()
};

let fileSourceTracking = {
    standalone: [],
    fromFolders: []
};

let processingStartTime = null;
let timingInterval = null;

const uploadArea = document.getElementById('uploadArea');
const uploadSection = document.querySelector('.upload-section');
const fileInput = document.getElementById('fileInput');
const fileList = document.getElementById('fileList');
const fileItems = document.getElementById('fileItems');
const fileScrollProgress = document.getElementById('fileScrollProgress');
const fileScrollFill = document.getElementById('fileScrollFill');
const scanInfo = document.getElementById('scanInfo');
const scanInfoText = document.getElementById('scanInfoText');
const charactersInput = document.getElementById('charactersInput');
const processBtn = document.getElementById('processBtn');
const progressContainer = document.getElementById('progressContainer');
const progressFill = document.getElementById('progressFill');
const progressText = document.getElementById('progressText');

let timingText = null;

const downloadSection = document.getElementById('downloadSection');
const downloadItems = document.getElementById('downloadItems');
const downloadControls = document.getElementById('downloadControls');
const downloadAllBtn = document.getElementById('downloadAllBtn');
const dragOverlay = document.getElementById('dragOverlay');

const zipProgressContainer = document.getElementById('zipProgressContainer');
const zipProgressFill = document.getElementById('zipProgressFill');
const zipProgressText = document.getElementById('zipProgressText');
const zipProgressDetails = document.getElementById('zipProgressDetails');

const engineLoadingContainer = document.getElementById('engineLoadingContainer');
const engineLoadingStatus = document.getElementById('engineLoadingStatus');
const engineNotReadyHint = document.getElementById('engineNotReadyHint');

document.addEventListener('DOMContentLoaded', function() {
    console.log('DOM 已加载，开始初始化');
    
    const overlay = document.getElementById('dragOverlay');
    console.log('dragOverlay 元素:', overlay);
    
    initFontToolI18n();
    
    initDragAndDrop();
    initFileInput();
    initPasteSupport();
    
    showEngineLoadingStatus();
    
    initPyodideAsync();
    
    if (typeof loadCommonComponents === 'function') {
        loadCommonComponents();
    }
    
    setTimeout(() => {
        if (typeof JSZip !== 'undefined') {
            console.log('✅ JSZip库加载成功，支持文件夹ZIP下载');
        } else {
            console.log('❌ JSZip库加载失败，ZIP下载功能将不可用');
        }
    }, 2000);
});

function showEngineLoadingStatus() {
    if (engineLoadingContainer) {
        engineLoadingContainer.style.display = 'block';
    }
    
    if (processBtn) {
        processBtn.disabled = true;
        processBtn.style.opacity = '0.6';
        processBtn.style.cursor = 'not-allowed';
    }
    
    if (engineNotReadyHint) {
        engineNotReadyHint.style.display = 'flex';
    }
    
    console.log('🎨 字体处理引擎加载状态已显示');
}

function hideEngineLoadingStatus() {
    if (engineLoadingContainer) {
        engineLoadingContainer.classList.add('fade-out');
        
        setTimeout(() => {
            engineLoadingContainer.style.display = 'none';
        }, 300);
    }
    
    if (processBtn) {
        processBtn.disabled = false;
        processBtn.style.opacity = '1';
        processBtn.style.cursor = 'pointer';
    }
    
    if (engineNotReadyHint) {
        engineNotReadyHint.classList.add('fade-out');
        
        setTimeout(() => {
            engineNotReadyHint.style.display = 'none';
        }, 300);
    }
    
    console.log('🎨 字体处理引擎加载状态已隐藏');
}

function updateEngineLoadingStatus(message) {
    if (engineLoadingStatus) {
        engineLoadingStatus.textContent = translateText(message);
    }
    console.log(`⚙️ ${message}`);
}

async function initPyodideAsync() {
    try {
        updateEngineLoadingStatus('正在加载处理引擎...');
        
        pyodide = await loadPyodide();
        
        updateEngineLoadingStatus('正在安装核心库...');
        
        await pyodide.loadPackage(['micropip']);
        
        updateEngineLoadingStatus('正在配置字体处理组件...');
        
        await pyodide.runPythonAsync(`
            import micropip
            await micropip.install(['fonttools'])
        `);
        
        updateEngineLoadingStatus('正在初始化字体处理引擎...');
        
        await loadPythonFontProcessor();
        
        await testPythonEnvironment();
        
        pythonReady = true;
        updateEngineLoadingStatus('字体处理引擎已就绪！');
        
        setTimeout(() => {
            hideEngineLoadingStatus();
        }, 1000);
        
        console.log('🚀 专业字体处理引擎初始化完成！');
        
    } catch (error) {
        console.error('❌ 处理引擎初始化失败，将尝试备用方案...', error);
        updateEngineLoadingStatus('引擎加载失败，启用备用方案...');
        
        await loadFallbackLibrary();
        
        setTimeout(() => {
            hideEngineLoadingStatus();
        }, 2000);
    }
}

async function loadPythonFontProcessor() {
        pyodide.runPython(`
from fontTools.ttLib import TTFont
from fontTools.subset import Subsetter, Options
import base64
import io

def subset_font(font_data_base64, characters_to_keep):
    """
    更严格的字体子集化处理 - 彻底清理多余字符和复合字形
    """
    try:
        from fontTools.ttLib import TTFont
        from fontTools.subset import Subsetter, Options
        import base64
        import io
        
        print(f"[DEBUG] 开始严格字体处理，要保留的字符: {characters_to_keep}")
        print(f"[DEBUG] Base64数据长度: {len(font_data_base64)} 字符")
        
        font_data = base64.b64decode(font_data_base64)
        print(f"[DEBUG] 解码后字体数据大小: {len(font_data)} 字节")
        
        if len(font_data) >= 12:
            original_header = font_data[:12]
            header_hex = ' '.join(f'{b:02x}' for b in original_header)
            print(f"[DEBUG] 原始字体文件头: {header_hex}")
            
            signature = int.from_bytes(font_data[:4], 'big')
            if signature == 0x00010000:
                print("[DEBUG] 原始文件：有效的TTF格式")
            elif signature == 0x4F54544F:
                print("[DEBUG] 原始文件：有效的OTF格式") 
            else:
                print(f"[DEBUG] 原始文件：未知格式 0x{signature:08x}")
        
        font_io = io.BytesIO(font_data)
        font = TTFont(font_io)
        
        print(f"[DEBUG] 字体加载成功")
        print(f"[DEBUG] 原始表数量: {len(font.keys())}")
        print(f"[DEBUG] 原始表列表: {sorted(list(font.keys()))}")
        
        if 'head' in font:
            head = font['head']
            print(f"[DEBUG] unitsPerEm: {head.unitsPerEm}")
            print(f"[DEBUG] 字体创建时间: {head.created}")
        
        if 'cmap' in font:
            cmap = font.getBestCmap()
            print(f"[DEBUG] 字符映射数量: {len(cmap) if cmap else 0}")
            
            found_chars = []
            for char in characters_to_keep:
                char_code = ord(char)
                if cmap and char_code in cmap:
                    found_chars.append(char)
                    print(f"[DEBUG] 找到字符 '{char}' (U+{char_code:04X}) -> 字形{cmap[char_code]}")
                else:
                    print(f"[DEBUG] 未找到字符 '{char}' (U+{char_code:04X})")
            
            if not found_chars:
                raise Exception(f'在字体中未找到任何指定字符。字体包含字符范围: U+{min(cmap.keys()):04X} - U+{max(cmap.keys()):04X}')
        
        options = Options()
        
        options.desubroutinize = True          
        options.drop_tables = [               
            'DSIG',    
            'GSUB',    
            'GPOS',    
            'kern',    
            'hdmx',    
            'VDMX',    
            'LTSH',    
            'VORG',    
        ]
        options.passthrough_tables = False     
        options.recalc_bounds = True          
        options.recalc_timestamp = False      
        options.canonical_order = True       
        options.flavor = None                 
        options.with_zopfli = False          
        
        options.name_IDs = ['*']              
        options.name_legacy = False           
        options.name_languages = ['*']        
        
        options.notdef_glyph = True           
        options.notdef_outline = False        
        options.recommended_glyphs = False    
        options.glyph_names = False           
        
        options.layout_features = []          
        options.layout_scripts = []           
        
        subsetter = Subsetter(options=options)
        print(f"[DEBUG] 严格子集化器创建成功，已配置彻底清理选项")
        
        print(f"[DEBUG] 严格模式：只保留指定字符 {repr(characters_to_keep)}")
        subsetter.populate(text=characters_to_keep)
        print(f"[DEBUG] 字符设置完成: {repr(characters_to_keep)} (严格清理模式)")
        
        print(f"[DEBUG] 开始严格子集化处理...")
        subsetter.subset(font)
        print(f"[DEBUG] 严格子集化完成")
        
        print(f"[DEBUG] 处理后表数量: {len(font.keys())}")
        print(f"[DEBUG] 处理后表列表: {sorted(list(font.keys()))}")
        
        critical_tables = ['cmap', 'head', 'hhea', 'hmtx', 'maxp', 'name']
        for table in critical_tables:
            if table in font:
                print(f"[DEBUG] ✓ 关键表 '{table}' 存在")
            else:
                print(f"[DEBUG] ✗ 关键表 '{table}' 缺失")
        
        if 'cmap' in font:
            new_cmap = font.getBestCmap()
            print(f"[DEBUG] 处理后字符映射数量: {len(new_cmap) if new_cmap else 0}")
            if new_cmap:
                has_space = 32 in new_cmap
                has_null = 0 in new_cmap
                print(f"[DEBUG] 关键字符检查: 空格={has_space}, null={has_null}")
                
                for char_code, glyph_id in new_cmap.items():
                    char = chr(char_code) if 32 <= char_code <= 126 else f"U+{char_code:04X}"
                    print(f"[DEBUG] 保留的映射: {char} -> 字形{glyph_id}")
        
        if 'glyf' in font:
            glyf_table = font['glyf']
            print(f"[DEBUG] 字形表包含 {len(glyf_table)} 个字形")
            
            if '.notdef' in glyf_table:
                print(f"[DEBUG] ✓ .notdef字形存在")
            else:
                print(f"[DEBUG] ✗ .notdef字形缺失")
                
            glyph_names = list(glyf_table.keys())[:20]  
            print(f"[DEBUG] 字形列表(前20个): {glyph_names}")
        
        if 'name' in font:
            name_table = font['name']
            font_family = None
            for record in name_table.names:
                if record.nameID == 1:  
                    try:
                        font_family = record.toUnicode()
                        break
                    except:
                        pass
            print(f"[DEBUG] 字体家族名称: {font_family}")
        
        if 'OS/2' in font:
            os2_table = font['OS/2']
            print(f"[DEBUG] OS/2表版本: {os2_table.version}")
            print(f"[DEBUG] 字重: {os2_table.usWeightClass}")
        
        if 'maxp' in font:
            maxp_table = font['maxp']
            print(f"[DEBUG] 最大字形数: {maxp_table.numGlyphs}")
            if hasattr(maxp_table, 'maxPoints'):
                print(f"[DEBUG] 最大点数: {maxp_table.maxPoints}")
            if hasattr(maxp_table, 'maxContours'):
                print(f"[DEBUG] 最大轮廓数: {maxp_table.maxContours}")
        
        output_io = io.BytesIO()
        print(f"[DEBUG] 开始保存字体...")
        font.save(output_io)
        print(f"[DEBUG] 字体保存完成")
        
        font.close()
        
        output_data = output_io.getvalue()
        print(f"[DEBUG] 生成的字体大小: {len(output_data)} 字节")
        
        if len(output_data) < 100:
            raise Exception(f'生成的字体文件过小({len(output_data)}字节)')
        
        if len(output_data) >= 12:
            output_header = output_data[:12]
            header_hex = ' '.join(f'{b:02x}' for b in output_header)
            print(f"[DEBUG] 输出字体文件头: {header_hex}")
            
            signature = int.from_bytes(output_data[:4], 'big')
            if signature == 0x00010000:
                print("[DEBUG] 输出文件：有效的TTF格式")
            elif signature == 0x4F54544F:
                print("[DEBUG] 输出文件：有效的OTF格式")
            else:
                print(f"[DEBUG] 输出文件：异常格式 0x{signature:08x}")
        
        try:
            print(f"[DEBUG] 开始验证生成的字体...")
            verify_io = io.BytesIO(output_data)
            verify_font = TTFont(verify_io)
            verify_cmap = verify_font.getBestCmap()
            print(f"[DEBUG] 验证成功！生成的字体包含 {len(verify_cmap) if verify_cmap else 0} 个字符映射")
            
            verify_glyf = verify_font.get('glyf')
            if verify_glyf:
                print(f"[DEBUG] 字形表包含 {len(verify_glyf)} 个字形")
            
            verify_font.close()
        except Exception as verify_error:
            print(f"[ERROR] 生成的字体验证失败: {verify_error}")
            import traceback
            print(f"[ERROR] 验证错误详情: {traceback.format_exc()}")
            
        print(f"[INFO] === 严格清理模式处理完成 ===")
        print(f"[INFO] 处理模式: 严格子集化 + 彻底清理复合字形")
        print(f"[INFO] 清理选项: 移除GSUB/GPOS表，去除复合字形信息")
        print(f"[INFO] 输入字符: {repr(characters_to_keep)}")
        print(f"[INFO] 输出大小: {len(output_data)} 字节")
        print(f"[INFO] 已彻底清理多余字符和复合字形")
        print(f"[INFO] =====================================")
        
        result_base64 = base64.b64encode(output_data).decode('utf-8')
        print(f"[DEBUG] Base64编码完成，长度: {len(result_base64)} 字符")
        
        return {
            'success': True,
            'data': result_base64,
            'size': len(output_data),
            'message': f'严格清理完成，只保留 {len(characters_to_keep)} 个指定字符'
        }
        
    except Exception as e:
        import traceback
        error_detail = traceback.format_exc()
        print(f"[ERROR] 处理失败: {str(e)}")
        print(f"[ERROR] 详细错误: {error_detail}")
        return {
            'success': False,
            'error': str(e),
            'error_detail': error_detail,
            'message': f'处理失败: {str(e)}'
        }

def test_fonttools():
    return "FontTools库已就绪"
        `);
}

async function testPythonEnvironment() {
    try {
        const test_result = pyodide.runPython('test_fonttools()');
        console.log(`✅ ${test_result}`);
        
        const function_test = pyodide.runPython(`
import inspect
if 'subset_font' in globals():
    sig = inspect.signature(subset_font)
    f"subset_font函数已定义，参数: {list(sig.parameters.keys())}"
else:
    "ERROR: subset_font函数未定义"
        `);
        console.log(`🔧 ${function_test}`);
        
    } catch (testError) {
        console.error(`❌ 处理环境测试失败: ${testError.message}`, testError);
        throw testError;
    }
}

async function initPyodide() {
    return await initPyodideAsync();
}

async function loadFallbackLibrary() {
    try {
        const script = document.createElement('script');
        script.src = 'https://cdnjs.cloudflare.com/ajax/libs/opentype.js/1.3.4/opentype.min.js';
        script.onload = () => {
            console.log('📋 备用字体处理库已加载，功能有限。');
        };
        script.onerror = () => {
            console.error('❌ 无法加载任何字体处理库。');
        };
        document.head.appendChild(script);
    } catch (error) {
        console.error('❌ 备用库加载失败。', error);
    }
}

let dragCounter = 0;

function initDragAndDrop() {
    console.log('初始化拖拽功能');
    console.log('dragOverlay:', dragOverlay);
    console.log('uploadArea:', uploadArea);
    
    if (!dragOverlay) {
        console.error('拖拽覆盖层元素未找到！');
        return;
    }
    
    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        document.addEventListener(eventName, preventDefaults, false);
    });

    document.addEventListener('dragenter', handleDragEnter, false);
    document.addEventListener('dragleave', handleDragLeave, false);
    document.addEventListener('dragover', handleDragOver, false);
    document.addEventListener('drop', handlePageDrop, false);
    
    console.log('已添加全页面拖拽事件监听器');

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
    
    console.log('拖拽功能初始化完成');
    
    if (window.location.search.includes('debug=true')) {
        const testBtn = document.createElement('button');
        testBtn.textContent = translateText('测试覆盖层');
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
    console.log('拖拽进入事件，计数器:', dragCounter);
    
    if (e.dataTransfer && e.dataTransfer.types) {
        const hasFiles = e.dataTransfer.types.includes('Files');
        console.log('拖拽类型:', e.dataTransfer.types, '包含文件:', hasFiles);
        
        if (hasFiles) {
            showDragOverlay();
            console.log('检测到文件拖拽，显示覆盖层');
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

function checkDraggedFiles(dataTransfer) {
    const fontExtensions = ['.ttf', '.otf', '.woff', '.woff2'];
    
    for (let i = 0; i < dataTransfer.items.length; i++) {
        const item = dataTransfer.items[i];
        
        if (item.kind === 'file') {
            const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
            if (entry && entry.isDirectory) {
                return true;
            }
        }
        
        if (item.kind === 'file') {
            const file = item.getAsFile();
            if (file) {
                const fileName = file.name.toLowerCase();
                const hasValidExtension = fontExtensions.some(ext => fileName.endsWith(ext));
                if (hasValidExtension) {
                    return true;
                }
            }
        }
        
        if (item.type) {
            const validMimeTypes = [
                'font/ttf',
                'font/otf', 
                'font/woff',
                'font/woff2',
                'application/font-woff',
                'application/font-woff2',
                'application/x-font-ttf',
                'application/x-font-otf'
            ];
            
            if (validMimeTypes.some(mime => item.type.includes(mime))) {
                return true;
            }
        }
    }
    
    return false;
}

function handlePageDrop(e) {
    dragCounter = 0;
    hideDragOverlay();
    
    handleDrop(e);
}

function showDragOverlay() {
    console.log('显示拖拽覆盖层');
    if (dragOverlay) {
        dragOverlay.classList.add('active');
        document.body.style.overflow = 'hidden';
        console.log('覆盖层已激活');
    } else {
        console.error('dragOverlay 元素未找到');
    }
}

function hideDragOverlay() {
    console.log('隐藏拖拽覆盖层');
    if (dragOverlay) {
        dragOverlay.classList.remove('active');
        document.body.style.overflow = '';
        console.log('覆盖层已隐藏');
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
        console.log('正在扫描拖拽的内容...');
        console.log('拖拽项目数量:', dt.items.length);
        
        const files = [];
        const scanPromises = [];
        
        let mainFolderEntry = null;
        for (let i = 0; i < dt.items.length; i++) {
            const item = dt.items[i];
            console.log(`项目 ${i}:`, item.kind, item.type);
            
            if (item.kind === 'file') {
                const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : item.getAsEntry();
                if (entry) {
                    console.log(`条目 ${i}:`, entry.name, entry.isDirectory ? '目录' : '文件');
                    
                    if (entry.isDirectory) {
                        currentDropFolderMode = true;
                        currentDropFolderStructure.name = entry.name;
                        mainFolderEntry = entry;
                        console.log(`📁 检测到文件夹模式: ${entry.name}`);
                        console.log('主文件夹条目:', entry.name);
                        break; 
                    }
                }
            }
        }
        
        if (mainFolderEntry) {
            console.log('开始扫描主文件夹:', mainFolderEntry.name);
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
            
            console.log(`📁 扫描完成，发现 ${totalFiles} 个文件 (${files.length} 个字体文件, ${nonFontFiles} 个其他文件)`);
            
            if (currentDropFolderMode) {
                console.log(`📁 文件夹模式启用: 将保持目录结构并复制所有文件`);
                console.log(`🔍 调试: 目录数=${currentDropFolderStructure.directories.size}, 文件数=${currentDropFolderStructure.files.length}`);
            }
            
            handleFiles(files);
        } else {
            console.warn('未在拖拽的内容中找到任何文件');
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
        console.log('上传区域点击事件已绑定');
    } else {
        console.error('上传区域元素未找到！');
    }
}

function initPasteSupport() {
    document.addEventListener('paste', async function(e) {
        console.log('检测到粘贴事件');
        
        const clipboardData = e.clipboardData || window.clipboardData;
        if (!clipboardData) {
            console.log('无法访问剪贴板数据');
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
            console.log(`剪贴板中发现 ${clipboardData.items.length} 个项目`);
            
            for (let i = 0; i < clipboardData.items.length; i++) {
                const item = clipboardData.items[i];
                console.log(`项目 ${i}:`, item.kind, item.type);
                
                if (item.kind === 'file') {
                    const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
                    if (entry) {
                        console.log(`条目 ${i}:`, entry.name, entry.isDirectory ? '目录' : '文件');
                        
                        if (entry.isDirectory) {
                            console.log(`📁 检测到文件夹: ${entry.name}`);
                            folderMode = true;
                            folderStructure.name = entry.name;
                            foundFolderStructure = true;
                            
                            e.preventDefault();
                            
                            try {
                                await scanEntry(entry, files);
                                
                                if (files.length > 0) {
                                    const totalFiles = folderStructure.files.length;
                                    const nonFontFiles = totalFiles - files.length;
                                    
                                    console.log(`📁 文件夹扫描完成: ${totalFiles} 个文件 (${files.length} 个字体文件)`);
                                    
                                    updateScanInfo(totalFiles, files.length, nonFontFiles, folderMode);
                                    
                                    showTemporaryMessage(`${translateText('通过粘贴添加了文件夹')} "${entry.name}"${translateText('，包含')} ${files.length}${translateText('个字体文件')}`, 'success');
                                    
                                    handleFiles(files);
                                } else {
                                    showTemporaryMessage(`${translateText('文件夹')} "${entry.name}"${translateText('中没有找到字体文件')}`, 'warning');
                                }
                            } catch (error) {
                                console.error('文件夹扫描失败:', error);
                                showTemporaryMessage(translateText('文件夹处理失败，请尝试拖拽文件夹'), 'error');
                            }
                            return; 
                        } else if (entry.isFile) {
                            try {
                                await scanEntry(entry, files);
                            } catch (error) {
                                console.log('文件处理失败，将使用备用方法');
                            }
                        }
                    }
                }
            }
        }
        
        if (!foundFolderStructure) {
            const clipboardFiles = clipboardData.files;
            if (!clipboardFiles || clipboardFiles.length === 0) {
                console.log('剪贴板中没有文件');
                return;
            }
            
            console.log(`剪贴板中发现 ${clipboardFiles.length} 个文件`);
            
            const fontFiles = Array.from(clipboardFiles).filter(file => {
                const extension = file.name.toLowerCase().split('.').pop();
                return ['ttf', 'otf', 'woff', 'woff2'].includes(extension);
            });
            
            if (fontFiles.length > 0) {
                console.log(`检测到 ${fontFiles.length} 个字体文件，开始处理`);
                
                e.preventDefault();
                
                showTemporaryMessage(`${translateText('通过粘贴添加了')} ${fontFiles.length}${translateText('个字体文件')}`, 'success');
                
                handleFiles(fontFiles);
            } else {
                console.log('剪贴板中没有字体文件');
                if (clipboardFiles.length > 0) {
                    showTemporaryMessage(translateText('剪贴板中的文件不是支持的字体格式'), 'warning');
                }
            }
        }
    });
    
    console.log('全局粘贴支持已初始化（包含文件夹支持）');
}

async function scanEntry(entry, files, basePath = '') {
    console.log(`扫描条目: ${entry.name}, 类型: ${entry.isDirectory ? '目录' : '文件'}, 基础路径: ${basePath}`);
    
    if (entry.isFile) {
        return new Promise((resolve) => {
            entry.file((file) => {
                const relativePath = basePath ? `${basePath}/${file.name}` : file.name;
                console.log(`处理文件: ${file.name}, 相对路径: ${relativePath}`);
                
                const fileInfo = {
                    file: file,
                    relativePath: relativePath,
                    isFont: false
                };
                
                const extension = file.name.toLowerCase().split('.').pop();
                if (['ttf', 'otf', 'woff', 'woff2'].includes(extension)) {
                    fileInfo.isFont = true;
                    files.push(file); 
                    folderStructure.fontFiles.push(fileInfo);
                    console.log(`✅ 字体文件: ${relativePath}`);
                } else {
                    console.log(`📄 普通文件: ${relativePath}`);
                }
                
                folderStructure.files.push(fileInfo);
                
                if (basePath) {
                    folderStructure.directories.add(basePath);
                }
                
                resolve();
            }, () => resolve()); 
        });
    } else if (entry.isDirectory) {
        const currentPath = basePath ? `${basePath}/${entry.name}` : entry.name;
        console.log(`进入目录: ${entry.name}, 完整路径: ${currentPath}`);
        folderStructure.directories.add(currentPath);
        
        return new Promise((resolve) => {
            const reader = entry.createReader();
            const readEntries = async () => {
                reader.readEntries(async (entries) => {
                    if (entries.length === 0) {
                        resolve();
                        return;
                    }
                    
                    console.log(`目录 ${entry.name} 包含 ${entries.length} 个条目`);
                    const subPromises = entries.map(subEntry => scanEntry(subEntry, files, currentPath));
                    await Promise.all(subPromises);
                    
                    await readEntries();
                }, () => resolve()); 
            };
            readEntries();
        });
    }
}

async function scanEntryForCurrentDrop(entry, files, targetFolderStructure, basePath = '') {
    console.log(`扫描条目: ${entry.name}, 类型: ${entry.isDirectory ? '目录' : '文件'}, 基础路径: ${basePath}`);
    
    if (entry.isFile) {
        return new Promise((resolve) => {
            entry.file((file) => {
                const relativePath = basePath ? `${basePath}/${file.name}` : file.name;
                console.log(`处理文件: ${file.name}, 相对路径: ${relativePath}`);
                
                const fileInfo = {
                    file: file,
                    relativePath: relativePath,
                    isFont: false
                };
                
                const extension = file.name.toLowerCase().split('.').pop();
                if (['ttf', 'otf', 'woff', 'woff2'].includes(extension)) {
                    fileInfo.isFont = true;
                    files.push(file); 
                    targetFolderStructure.fontFiles.push(fileInfo);
                    console.log(`✅ 字体文件: ${relativePath}`);
                } else {
                    console.log(`📄 普通文件: ${relativePath}`);
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
        console.log(`进入目录: ${entry.name}, 完整路径: ${currentPath}`);
        targetFolderStructure.directories.add(currentPath);
        
        return new Promise((resolve) => {
            const reader = entry.createReader();
            const readEntries = async () => {
                reader.readEntries(async (entries) => {
                    if (entries.length === 0) {
                        resolve();
                        return;
                    }
                    
                    console.log(`目录 ${entry.name} 包含 ${entries.length} 个条目`);
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

function updateScanInfo(totalFiles, fontFiles, nonFontFiles, isFolder) {
    if (!scanInfo || !scanInfoText) return;
    
    if (totalFiles > 0) {
        scanInfo.style.display = 'flex';
        
        let infoText = `${translateText('扫描完成，发现')} ${totalFiles}${translateText('个文件')}`;
        if (totalFiles > fontFiles) {
            infoText += ` (${fontFiles}${translateText('个字体文件')}, ${nonFontFiles}${translateText('个其他文件')})`;
        }
        
        if (isFolder) {
            infoText += ` 📁 ${translateText('文件夹模式')}`;
        }
        
        scanInfoText.textContent = infoText;
        
        scanInfo.style.opacity = '0';
        setTimeout(() => {
            scanInfo.style.opacity = '1';
        }, 100);
    } else {
        scanInfo.style.display = 'none';
    }
}

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
        
        fileItem.innerHTML = `
            <div class="file-info">
                <div class="file-name">${file.name}</div>
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

function scrollToUploadArea() {
    scrollToElement(uploadSection, '上传卡片区域');
    
    if (uploadSection) {
        setTimeout(() => {
            uploadSection.style.animation = 'highlightFileList 1.5s ease-in-out';
            
            setTimeout(() => {
                uploadSection.style.animation = '';
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
    
    downloadItem.innerHTML = `
        <div class="download-info">
            <div class="download-name">${font.name}</div>
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
        console.error('❌ JSZip库未加载，无法创建ZIP文件');
        showTemporaryMessage(translateText('请刷新页面重试，或检查网络连接'), 'error');
        return;
    }

    if (!folderStructure.files || folderStructure.files.length === 0) {
        console.error('❌ 没有找到文件夹结构数据，无法创建ZIP');
        console.error(`🔍 调试: folderStructure.files=${folderStructure.files ? folderStructure.files.length : 'null'}, folderMode=${folderMode}`);
        showTemporaryMessage(translateText('请重新拖拽文件夹后再试'), 'warning');
        return;
    }

    console.log('📦 正在创建ZIP文件，保持目录结构...');
    
    try {
        const zip = new JSZip();
        const outputFolderName = folderStructure.folderNames.length > 1 
            ? folderStructure.folderNames.join('_') 
            : folderStructure.name;
        console.log('输出文件夹名称:', outputFolderName);
        console.log('文件夹列表:', folderStructure.folderNames);
        
        updateZipProgress(10, '正在创建目录结构...', `创建 ${folderStructure.directories.size} 个目录`);
        console.log('开始创建目录，总数:', folderStructure.directories.size);
        let dirCount = 0;
        folderStructure.directories.forEach(dirPath => {
            let fullPath;
            if (folderStructure.folderNames.length > 1) {
                fullPath = `${dirPath}/`;
            } else {
                const relativePath = dirPath.replace(new RegExp(`^${folderStructure.name}/?`), '');
                if (relativePath) {
                    fullPath = `${relativePath}/`;
                } else {
                    return; 
                }
            }
            zip.folder(fullPath);
            console.log('创建目录:', fullPath);
            dirCount++;
        });
        console.log(`✅ 完成创建 ${dirCount} 个目录（${folderStructure.folderNames.length > 1 ? '多文件夹保持结构' : '单文件夹扁平化'}）`);
        
        updateZipProgress(20, '正在准备字体文件...', `映射 ${processedFonts.length} 个处理后的字体`);
        const processedFontMap = new Map();
        processedFonts.forEach(font => {
            processedFontMap.set(font.name, font.data);
            console.log(`映射字体: ${font.name} -> ${font.data ? font.data.byteLength + '字节' : 'null'}`);
        });
        console.log(`✅ 字体映射完成，共 ${processedFontMap.size} 个字体`);
        
        console.log('开始添加文件到ZIP，总数:', folderStructure.files.length);
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
                const flattenedPath = relativePath.replace(new RegExp(`^${folderStructure.name}/?`), '');
                finalPath = flattenedPath || file.name; 
            }
            
            const fileProgress = 20 + (i / totalFiles) * 60;
            updateZipProgress(fileProgress, '正在添加文件...', `处理 ${finalPath} (${i + 1}/${totalFiles})`);
            
            try {
                if (isFont) {
                    const processedData = processedFontMap.get(file.name);
                    if (processedData) {
                        zip.file(finalPath, processedData);
                        console.log(`✅ 添加处理后的字体: ${finalPath} (${processedData.byteLength}字节)`);
                        addedFiles++;
                    } else {
                        console.log(`❌ 未找到处理后的字体数据: ${file.name}`);
                        skippedFiles++;
                    }
                } else {
                    const fileData = await readFileAsArrayBuffer(file);
                    zip.file(finalPath, fileData);
                    console.log(`✅ 复制原文件: ${finalPath} (${fileData.byteLength}字节)`);
                    addedFiles++;
                }
            } catch (error) {
                console.error(`❌ 处理文件失败 ${finalPath}:`, error);
                skippedFiles++;
            }
        }
        
        console.log(`✅ 文件添加完成: 成功${addedFiles}个, 跳过${skippedFiles}个`);
        console.log(`📦 已添加 ${addedFiles} 个文件到ZIP中`);
        
        updateZipProgress(80, '正在生成ZIP文件...', '压缩数据，请稍候...');
        console.log('📦 正在生成ZIP文件...');
        console.log('开始生成ZIP文件...');
        
        const zipBlob = await zip.generateAsync({
            type: 'blob',
            compression: 'DEFLATE',
            compressionOptions: {
                level: 6
            }
        });
        
        console.log(`✅ ZIP文件生成完成，大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log(`📦 ZIP文件大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        
        updateZipProgress(95, '正在准备下载...', `文件大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log('开始下载ZIP文件...');
        const url = URL.createObjectURL(zipBlob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${outputFolderName}.zip`;
        
        console.log('下载链接:', url);
        console.log('下载文件名:', `${outputFolderName}.zip`);
        
        updateZipProgress(100, '下载完成！', `${outputFolderName}.zip 已开始下载`);
        
        document.body.appendChild(a);
        console.log('触发下载...');
        a.click();
        console.log('下载已触发');
        
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        const fontFiles = folderStructure.fontFiles.length;
        const nonFontFiles = totalFiles - fontFiles;
        
        console.log(`🎉 ZIP文件下载完成！`);
        console.log(`📊 包含: ${fontFiles} 个处理后的字体文件, ${nonFontFiles} 个原始文件`);
        console.log(`📁 单独文件夹模式：扁平化结构，解压后直接可用，无需额外操作`);
        console.log('ZIP下载过程完成');
        
        hideZipProgress();
        
    } catch (error) {
        console.error(`❌创建ZIP文件失败: ${error.message}`);
        console.error('ZIP creation error:', error);
        
        hideZipProgress();
    }
}

async function downloadMixedModeAsZip() {
    console.log('=== downloadMixedModeAsZip 调试信息 ===');
    console.log('JSZip类型:', typeof JSZip);
    console.log('fileSourceTracking:', fileSourceTracking);
    console.log('folderStructure:', folderStructure);
    console.log('processedFonts.length:', processedFonts.length);
    console.log('================================');

    if (typeof JSZip === 'undefined') {
        console.error('❌ JSZip库未加载，无法创建ZIP文件');
        showTemporaryMessage(translateText('请刷新页面重试，或检查网络连接'), 'error');
        return;
    }

    console.log('📦 正在创建混合模式ZIP文件...');
    
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
            
        console.log('输出文件夹名称:', outputFolderName);
        console.log('单独文件:', fileSourceTracking.standalone.map(f => f.name));
        console.log('文件夹列表:', folderStructure.folderNames);
        console.log('名称组件:', nameComponents);
        
        updateZipProgress(10, '正在创建目录结构...', `创建 ${folderStructure.directories.size} 个目录`);
        console.log('开始创建目录，总数:', folderStructure.directories.size);
        let dirCount = 0;
        folderStructure.directories.forEach(dirPath => {
            const fullPath = `${dirPath}/`;
            zip.folder(fullPath);
            dirCount++;
            if (dirCount <= 5) { 
                console.log('创建目录:', fullPath);
            }
        });
        console.log(`✅ 完成创建 ${dirCount} 个目录`);
        
        updateZipProgress(20, '正在准备字体文件...', `映射 ${processedFonts.length} 个处理后的字体`);
        const processedFontMap = new Map();
        processedFonts.forEach(font => {
            processedFontMap.set(font.name, font.data);
            console.log(`映射字体: ${font.name} -> ${font.data ? font.data.byteLength + '字节' : 'null'}`);
        });
        console.log(`✅ 字体映射完成，共 ${processedFontMap.size} 个字体`);

        console.log('开始添加单独文件到ZIP根目录，总数:', fileSourceTracking.standalone.length);
        let addedStandaloneFiles = 0;
        
        for (let i = 0; i < fileSourceTracking.standalone.length; i++) {
            const file = fileSourceTracking.standalone[i];
            
            const fileProgress = 20 + (i / fileSourceTracking.standalone.length) * 20;
            updateZipProgress(fileProgress, '正在添加单独文件...', `处理 ${file.name} (${i + 1}/${fileSourceTracking.standalone.length})`);
            
            try {
                const processedData = processedFontMap.get(file.name);
                if (processedData) {
                    zip.file(file.name, processedData);
                    console.log(`✅ 添加单独文件到根目录: ${file.name} (${processedData.byteLength}字节)`);
                    addedStandaloneFiles++;
                } else {
                    console.log(`❌ 未找到单独文件的处理后数据: ${file.name}`);
                }
            } catch (error) {
                console.error(`❌ 处理单独文件失败 ${file.name}:`, error);
            }
        }
        console.log(`✅ 单独文件添加完成: 成功${addedStandaloneFiles}个`);
        
        console.log('开始添加文件夹文件到ZIP，总数:', folderStructure.files.length);
        let addedFolderFiles = 0;
        let skippedFiles = 0;
        const totalFolderFiles = folderStructure.files.length;
        
        for (let i = 0; i < folderStructure.files.length; i++) {
            const fileInfo = folderStructure.files[i];
            const { file, relativePath, isFont } = fileInfo;
            
            const fileProgress = 40 + (i / totalFolderFiles) * 40;
            updateZipProgress(fileProgress, '正在添加文件夹文件...', `处理 ${relativePath} (${i + 1}/${totalFolderFiles})`);
            
            try {
                if (isFont) {
                    const processedData = processedFontMap.get(file.name);
                    if (processedData) {
                        zip.file(relativePath, processedData);
                        console.log(`✅ 添加文件夹字体: ${relativePath} (${processedData.byteLength}字节)`);
                        addedFolderFiles++;
                    } else {
                        console.log(`❌ 未找到文件夹字体的处理后数据: ${file.name}`);
                        skippedFiles++;
                    }
                } else {
                    const fileData = await readFileAsArrayBuffer(file);
                    zip.file(relativePath, fileData);
                    console.log(`✅ 复制原文件: ${relativePath} (${fileData.byteLength}字节)`);
                    addedFolderFiles++;
                }
            } catch (error) {
                console.error(`❌ 处理文件夹文件失败 ${relativePath}:`, error);
                skippedFiles++;
            }
        }
        
        console.log(`✅ 文件夹文件添加完成: 成功${addedFolderFiles}个, 跳过${skippedFiles}个`);
        console.log(`📦 混合模式ZIP: ${addedStandaloneFiles}个单独文件(根目录) + ${addedFolderFiles}个文件夹文件(目录结构)`);
        
        updateZipProgress(80, '正在生成ZIP文件...', '压缩数据，请稍候...');
        console.log('📦 正在生成混合模式ZIP文件...');
        
        const zipBlob = await zip.generateAsync({
            type: 'blob',
            compression: 'DEFLATE',
            compressionOptions: {
                level: 6
            }
        });
        
        console.log(`✅ 混合模式ZIP文件生成完成，大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        
        updateZipProgress(95, '正在准备下载...', `文件大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log('开始下载混合模式ZIP文件...');
        const url = URL.createObjectURL(zipBlob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${outputFolderName}.zip`;
        
        console.log('下载链接:', url);
        console.log('下载文件名:', `${outputFolderName}.zip`);
        
        updateZipProgress(100, '下载完成！', `${outputFolderName}.zip 已开始下载`);
        
        document.body.appendChild(a);
        console.log('触发下载...');
        a.click();
        console.log('下载已触发');
        
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        const totalProcessedFiles = addedStandaloneFiles + addedFolderFiles;
        
        console.log(`🎉 混合模式ZIP文件下载完成！`);
        console.log(`📊 包含: ${addedStandaloneFiles}个单独文件(根目录) + ${addedFolderFiles}个文件夹文件(目录结构)`);
        console.log(`📁 混合模式处理完成`);
        console.log('混合模式ZIP下载过程完成');
        
        hideZipProgress();
        
    } catch (error) {
        console.error(`❌创建混合模式ZIP文件失败: ${error.message}`);
        console.error('Mixed mode ZIP creation error:', error);
        
        hideZipProgress();
    }
}

function readFileAsArrayBuffer(file) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => resolve(reader.result);
        reader.onerror = () => reject(new Error('文件读取失败'));
        reader.readAsArrayBuffer(file);
    });
}

function showZipProgress() {
    if (zipProgressContainer) {
        zipProgressContainer.style.display = 'block';
        zipProgressFill.style.width = '0%';
        zipProgressText.textContent = translateText('正在准备ZIP生成...');
        zipProgressDetails.textContent = translateText('初始化中...');
    }
}

function hideZipProgress() {
    if (zipProgressContainer) {
        setTimeout(() => {
            zipProgressContainer.style.display = 'none';
        }, 2000); 
    }
}

function updateZipProgress(percentage, statusText, detailText) {
    if (zipProgressFill && zipProgressText && zipProgressDetails) {
        zipProgressFill.style.width = `${Math.min(100, Math.max(0, percentage))}%`;
        zipProgressText.textContent = statusText;
        zipProgressDetails.textContent = detailText;
        
        if (percentage >= 100) {
            zipProgressFill.style.background = 'linear-gradient(90deg, #4caf50, #8bc34a)';
            zipProgressText.innerHTML = '<i class="fas fa-check"></i> ' + statusText;
        }
    }
}

function clearAllProcessedFiles() {
    console.log('🧹 开始清理全部文件和处理结果...');
    
    selectedFiles = [];
    
    processedFonts = [];
    
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
    
    downloadSection.style.display = 'none';
    downloadItems.innerHTML = '';
    downloadControls.style.display = 'none';
    
    resetProgressBar();
    
    resetTimingDisplay();
    
    processBtn.disabled = false;
    processBtn.innerHTML = `<i class="fas fa-rocket"></i> ${translateText('开始处理字体')}`;
    
    processingStartTime = null;
    
    if (fileInput) {
        fileInput.value = '';
    }
    
    console.log('✅ 完全清理完成！已重置到初始状态');
    
    showTemporaryMessage(translateText('已清理全部文件和处理结果，界面已重置'), 'success');
    
    scrollToUploadArea();
}

function resetProgressBar() {
    if (progressContainer) {
        progressContainer.style.display = 'none';
        progressFill.style.width = '0%';
        progressText.textContent = '0%';
    }
}

function resetTimingDisplay() {
    if (timingInterval) {
        clearInterval(timingInterval);
        timingInterval = null;
    }
    
    if (timingText) {
        timingText.remove();
        timingText = null;
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
    messageDiv.innerHTML = `
        <i class="fas fa-${iconClass}"></i>
        <span>${message}</span>
    `;
    
    document.body.insertBefore(messageDiv, document.body.firstChild);
    
    setTimeout(() => {
        messageDiv.classList.add('show');
    }, 100);
    
    setTimeout(() => {
        messageDiv.classList.remove('show');
        setTimeout(() => {
            if (messageDiv.parentNode) {
                messageDiv.parentNode.removeChild(messageDiv);
            }
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

document.addEventListener('DOMContentLoaded', function() {
    initFileScrollProgress();
});

window.addEventListener('error', function(e) {
    console.error(`发生错误: ${e.message}`);
});

window.addEventListener('unhandledrejection', function(e) {
    console.error(`Promise错误: ${e.reason}`);
    e.preventDefault();
});

function initFontToolI18n() {
    setupTranslateFunction();
    
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    const htmlRoot = document.getElementById('html-root');
    if (htmlRoot) {
        htmlRoot.lang = currentLang === 'zh' ? 'zh-CN' : 'en';
    }
    
    if (currentLang === 'en') {
        applyFontToolTranslations();
    }
    
    setTimeout(initLanguageToggleForFontTool, 100);
}

function setupTranslateFunction() {
    const translations = {
        'Catime - 字体简化工具': 'Catime - Font Simplifier',
        'Catime 字体简化工具 - 批量处理字体文件，只保留指定字符的专业级 Web 版本': 'Catime Font Simplifier - Professional web tool for batch processing font files, keeping only specified characters',
        
        '字体简化工具': 'Font Simplifier',
        
        '拖拽字体文件或文件夹到这里': 'Drag font files or folders here',
        '或者通过 Ctrl+V 粘贴': 'Or paste with Ctrl+V',
        '支持拖拽/粘贴文件夹，会自动扫描所有子文件夹中的字体文件': 'Support drag/paste folders, automatically scan all font files in subfolders',
        '选择文件': 'Choose Files',
        '拖拽字体文件到这里': 'Drag font files here',
        '支持 .ttf, .otf, .woff, .woff2 格式': 'Support .ttf, .otf, .woff, .woff2 formats',
        '可以拖拽/粘贴文件夹，自动扫描所有字体文件': 'Drag/paste folders to auto-scan all font files',
        
        '清除所有文件': 'Clear All Files',
        
        '要保留的字符': 'Characters to Keep',
        '请输入要保留的字符，例如：0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz': 'Enter characters to keep, e.g.: 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz',
        '数字+:.': 'Numbers+:.',
        '数字 0-9': 'Numbers 0-9',
        '英文字母': 'Letters',
        '字母+数字': 'Letters+Numbers',
        
        '开始处理字体': 'Start Processing',
        
        '正在准备字体处理引擎': 'Preparing Font Processing Engine',
        '正在加载处理引擎...': 'Loading processing engine...',
        '正在安装核心库...': 'Installing core libraries...',
        '正在配置字体处理组件...': 'Configuring font processing components...',
        '正在初始化字体处理引擎...': 'Initializing font processing engine...',
        '字体处理引擎已就绪！': 'Font processing engine ready!',
        '引擎加载失败，启用备用方案...': 'Engine loading failed, enabling fallback...',
        '字体处理引擎正在初始化，请稍候...': 'Font processing engine is initializing, please wait...',
        
        '处理后的字体': 'Processed Fonts',
        '下载字体文件': 'Download Fonts',
        '清理全部': 'Clear All',
        
        '完全本地处理，所有计算在浏览器中完成，数据不会上传到任何服务器。': 'Fully local processing. All calculations are done in your browser. No data is uploaded to any server.',
        
        '正在生成ZIP文件...': 'Generating ZIP file...',
        '准备中...': 'Preparing...',
        
        '处理中...': 'Processing...',
        '处理完成': 'Processing Completed',
        '下载': 'Download',
        '下载字体文件': 'Download Fonts',
        '下载所有字体文件': 'Download All Fonts',
        '下载完整文件夹 (ZIP)': 'Download Complete Folder (ZIP)',
        '扫描完成，发现': 'Scan completed, found',
        '个文件': ' files',
        '个字体文件': ' font files',
        '个其他文件': ' other files',
        '所有字体处理完成！成功处理': 'All fonts processed! Successfully processed',
        '字体处理完成！成功处理': 'Font processing completed! Successfully processed',
        '字体处理失败，没有成功处理任何文件': 'Font processing failed, no files were successfully processed',
        '字体处理过程中发生错误，请重试': 'An error occurred during font processing, please try again',
        '成功添加': 'Successfully added',
        '个字体文件，总计': ' font files, total',
        '个文件待处理。': ' files to process.',
        '总耗时: ': 'Total time: ',
        '秒': 's',
        '分钟': 'm',
        '小时': 'h',
        '文件夹模式': 'Folder Mode',
        '压缩了': 'compressed',
        '处理完成，总耗时: ': 'Processing completed, total time: ',
        '个文件': ' files',
        '包含目录结构和所有非字体文件': 'Including directory structure and all non-font files',
        '个单独文件': ' standalone files',
        '个文件夹文件': ' folder files',
        '已耗时: ': 'Elapsed: ',
        '请先选择要处理的字体文件！': 'Please select font files to process first!',
        '请输入要保留的字符！': 'Please enter characters to keep!',
        '字体处理引擎尚未就绪，请稍候再试': 'Font processing engine not ready, please try again later',
        '通过粘贴添加了文件夹': 'Added folder via paste',
        '，包含': ', containing',
        '中没有找到字体文件': ' contains no font files',
        '文件夹处理失败，请尝试拖拽文件夹': 'Folder processing failed, please try dragging folder',
        '通过粘贴添加了': 'Added via paste',
        '剪贴板中的文件不是支持的字体格式': 'Files in clipboard are not supported font formats',
        '已删除字体: ': 'Deleted font: ',
        '请刷新页面重试，或检查网络连接': 'Please refresh the page or check network connection',
        '请重新拖拽文件夹后再试': 'Please drag the folder again and try',
        '已清理全部文件和处理结果，界面已重置': 'All files and processing results cleared, interface reset',
        '文件夹': 'Folder',
        '删除此处理后的字体': 'Delete this processed font',
        '正在准备ZIP生成...': 'Preparing ZIP generation...',
        '初始化中...': 'Initializing...',
        '测试覆盖层': 'Test Overlay',
    };
    
    window.translateText = function(text) {
        if (localStorage.getItem('catime-language') !== 'en') return text;
        return translations[text] || text;
    };
}

function applyFontToolTranslations() {
    
    const pageTitle = document.querySelector('title');
    if (pageTitle) {
        const translatedTitle = translateText(pageTitle.textContent);
        if (translatedTitle !== pageTitle.textContent) {
            pageTitle.textContent = translatedTitle;
        }
    }
    
    const metaDescription = document.querySelector('meta[name="description"]');
    if (metaDescription) {
        const content = metaDescription.getAttribute('content');
        const translatedContent = translateText(content);
        if (translatedContent !== content) {
            metaDescription.setAttribute('content', translatedContent);
        }
    }
    
    const staticTexts = [
        '字体简化工具',
        '拖拽字体文件或文件夹到这里',
        '或者通过 Ctrl+V 粘贴',
        '支持拖拽/粘贴文件夹，会自动扫描所有子文件夹中的字体文件',
        '选择文件',
        '拖拽字体文件到这里',
        '支持 .ttf, .otf, .woff, .woff2 格式',
        '可以拖拽/粘贴文件夹，自动扫描所有字体文件',
        '清除所有文件',
        '要保留的字符',
        '请输入要保留的字符，例如：0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz',
        '数字+:.',
        '数字 0-9',
        '英文字母',
        '字母+数字',
        '开始处理字体',
        '处理后的字体',
        '下载字体文件',
        '清理全部',
        '完全本地处理，所有计算在浏览器中完成，数据不会上传到任何服务器。',
        '正在准备字体处理引擎',
        '字体处理引擎正在初始化，请稍候...'
    ];
    
    staticTexts.forEach(chinese => {
        const english = translateText(chinese);
        if (english !== chinese) {
            const elements = document.querySelectorAll('*:not(script):not(style)');
            elements.forEach(element => {
                if (element.childNodes.length > 0) {
                    element.childNodes.forEach(node => {
                        if (node.nodeType === Node.TEXT_NODE && node.textContent.trim() === chinese) {
                            node.textContent = english;
                            
                            if (chinese === '完全本地处理，所有计算在浏览器中完成，数据不会上传到任何服务器。') {
                                element.classList.add('english-notice');
                            } else if (chinese === '支持拖拽/粘贴文件夹，会自动扫描所有子文件夹中的字体文件') {
                                element.classList.add('english-folder-hint');
                            }
                        }
                    });
                }
                
                if (element.placeholder === chinese) {
                    element.placeholder = english;
                }
                
                if (element.title === chinese) {
                    element.title = english;
                }
            });
        }
    });
    
    handleSpecialTranslations();
    
    updateButtonTexts();
}

function updateButtonTexts() {
    if (processBtn && !processBtn.disabled) {
        processBtn.innerHTML = `<i class="fas fa-rocket"></i> ${translateText('开始处理字体')}`;
    }
    
    if (downloadAllBtn && typeof updateDownloadButtonText === 'function') {
        updateDownloadButtonText();
    }
}

function handleSpecialTranslations() {
    const lang = localStorage.getItem('catime-language') || 'zh';
    if (lang !== 'en') return;
    
    const heroTitle = document.querySelector('.guide-hero-title');
    if (heroTitle) {
        const catimeSpan = heroTitle.querySelector('.catime-text');
        const accentSpan = heroTitle.querySelector('.guide-accent');
        if (catimeSpan && accentSpan) {
            accentSpan.textContent = 'Font Simplifier';
        }
    }
}

function initLanguageToggleForFontTool() {
    const languageToggle = document.getElementById('language-toggle');
    if (!languageToggle) return;
    
    const currentLang = localStorage.getItem('catime-language') || 'zh';
    
    updateToggleTextForFontTool(currentLang);
    
    if (!languageToggle.dataset.fontToolListener) {
        languageToggle.addEventListener('click', function(e) {
            e.preventDefault();
            
            const newLang = currentLang === 'zh' ? 'en' : 'zh';
            localStorage.setItem('catime-language', newLang);
            
            window.location.reload();
        });
        
        languageToggle.dataset.fontToolListener = 'true';
    }
}

function updateToggleTextForFontTool(lang) {
    const languageToggle = document.getElementById('language-toggle');
    if (!languageToggle) return;
    
    if (lang === 'zh') {
        languageToggle.innerHTML = '<i class="fas fa-language"></i> English';
    } else {
        languageToggle.innerHTML = '<i class="fas fa-language"></i> 中文';
    }
}