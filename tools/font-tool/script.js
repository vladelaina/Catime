// 全局变量
let selectedFiles = [];
let processedFonts = [];
let pyodide = null;
let pythonReady = false;

// 文件夹模式相关变量
let folderMode = false;
let folderStructure = {
    name: '',
    files: [], // 所有文件（包括非字体文件）
    fontFiles: [], // 仅字体文件
    directories: new Set() // 所有目录路径
};

// 文件来源跟踪（用于智能下载按钮）
let fileSourceTracking = {
    standalone: [], // 单独添加的文件
    fromFolders: []  // 从文件夹扫描来的文件
};

// 计时相关变量
let processingStartTime = null;
let timingInterval = null;

// DOM 元素
const uploadArea = document.getElementById('uploadArea');
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

// 计时显示元素（将在进度条显示时动态创建）
let timingText = null;

const downloadSection = document.getElementById('downloadSection');
const downloadItems = document.getElementById('downloadItems');
const downloadControls = document.getElementById('downloadControls');
const downloadAllBtn = document.getElementById('downloadAllBtn');
const dragOverlay = document.getElementById('dragOverlay');

// ZIP进度条元素
const zipProgressContainer = document.getElementById('zipProgressContainer');
const zipProgressFill = document.getElementById('zipProgressFill');
const zipProgressText = document.getElementById('zipProgressText');
const zipProgressDetails = document.getElementById('zipProgressDetails');

// 初始化
document.addEventListener('DOMContentLoaded', function() {
    // 确保所有 DOM 元素都已加载
    console.log('DOM 已加载，开始初始化');
    
    // 检查关键元素是否存在
    const overlay = document.getElementById('dragOverlay');
    console.log('dragOverlay 元素:', overlay);
    
    initPyodide();
    initDragAndDrop();
    initFileInput();
    initPasteSupport();
    
    // 加载通用组件
    if (typeof loadCommonComponents === 'function') {
        loadCommonComponents();
    }
    
    // 检查JSZip库是否加载
    setTimeout(() => {
        if (typeof JSZip !== 'undefined') {
            console.log('✅ JSZip库加载成功，支持文件夹ZIP下载');
        } else {
            console.log('❌ JSZip库加载失败，ZIP下载功能将不可用');
        }
    }, 2000);
});

// 初始化Pyodide (Python in Browser)
async function initPyodide() {
    try {
        console.log('📦 正在加载Python运行环境，请稍候...');
        
        // 加载Pyodide
        pyodide = await loadPyodide();
        
        console.log('📚 正在安装fonttools库...');
        
        // 安装必要的Python包
        await pyodide.loadPackage(['micropip']);
        
        // 修复：正确的异步安装方式
        await pyodide.runPythonAsync(`
            import micropip
            await micropip.install(['fonttools'])
        `);
        
        // 加载字体处理Python代码
        pyodide.runPython(`
from fontTools.ttLib import TTFont
from fontTools.subset import Subsetter
import base64
import io

def subset_font(font_data_base64, characters_to_keep):
    """
    与本地版本完全一致的精简版本 + 诊断信息
    """
    try:
        from fontTools.ttLib import TTFont
        from fontTools.subset import Subsetter
        import base64
        import io
        
        print(f"[DEBUG] 开始处理字体，要保留的字符: {characters_to_keep}")
        print(f"[DEBUG] Base64数据长度: {len(font_data_base64)} 字符")
        
        # 解码字体数据
        font_data = base64.b64decode(font_data_base64)
        print(f"[DEBUG] 解码后字体数据大小: {len(font_data)} 字节")
        
        # 验证原始数据
        if len(font_data) >= 12:
            original_header = font_data[:12]
            header_hex = ' '.join(f'{b:02x}' for b in original_header)
            print(f"[DEBUG] 原始字体文件头: {header_hex}")
            
            # 检查TTF签名
            signature = int.from_bytes(font_data[:4], 'big')
            if signature == 0x00010000:
                print("[DEBUG] 原始文件：有效的TTF格式")
            elif signature == 0x4F54544F:
                print("[DEBUG] 原始文件：有效的OTF格式") 
            else:
                print(f"[DEBUG] 原始文件：未知格式 0x{signature:08x}")
        
        # 加载字体
        font_io = io.BytesIO(font_data)
        font = TTFont(font_io)
        
        print(f"[DEBUG] 字体加载成功")
        print(f"[DEBUG] 原始表数量: {len(font.keys())}")
        print(f"[DEBUG] 原始表列表: {sorted(list(font.keys()))}")
        
        # 获取字体基本信息
        if 'head' in font:
            head = font['head']
            print(f"[DEBUG] unitsPerEm: {head.unitsPerEm}")
            print(f"[DEBUG] 字体创建时间: {head.created}")
        
        if 'cmap' in font:
            cmap = font.getBestCmap()
            print(f"[DEBUG] 字符映射数量: {len(cmap) if cmap else 0}")
            
            # 检查指定字符是否存在
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
        
        # 创建子集化器
        subsetter = Subsetter()
        print(f"[DEBUG] 子集化器创建成功")
        
        # 完全与本地版本一致 - 不添加任何额外字符
        subsetter.populate(text=characters_to_keep)
        print(f"[DEBUG] 字符设置完成: {repr(characters_to_keep)} (与本地版本完全一致)")
        
        # 应用子集化
        print(f"[DEBUG] 开始子集化...")
        subsetter.subset(font)
        print(f"[DEBUG] 子集化完成")
        
        print(f"[DEBUG] 处理后表数量: {len(font.keys())}")
        print(f"[DEBUG] 处理后表列表: {sorted(list(font.keys()))}")
        
        # 检查关键表是否存在
        critical_tables = ['cmap', 'head', 'hhea', 'hmtx', 'maxp', 'name']
        for table in critical_tables:
            if table in font:
                print(f"[DEBUG] ✓ 关键表 '{table}' 存在")
            else:
                print(f"[DEBUG] ✗ 关键表 '{table}' 缺失")
        
        # 验证处理后的字符映射
        if 'cmap' in font:
            new_cmap = font.getBestCmap()
            print(f"[DEBUG] 处理后字符映射数量: {len(new_cmap) if new_cmap else 0}")
            if new_cmap:
                # 检查关键字符
                has_space = 32 in new_cmap
                has_null = 0 in new_cmap
                print(f"[DEBUG] 关键字符检查: 空格={has_space}, null={has_null}")
                
                for char_code, glyph_id in new_cmap.items():
                    char = chr(char_code) if 32 <= char_code <= 126 else f"U+{char_code:04X}"
                    print(f"[DEBUG] 保留的映射: {char} -> 字形{glyph_id}")
        
        # 验证字形表
        if 'glyf' in font:
            glyf_table = font['glyf']
            print(f"[DEBUG] 字形表包含 {len(glyf_table)} 个字形")
            
            # 检查.notdef字形
            if '.notdef' in glyf_table:
                print(f"[DEBUG] ✓ .notdef字形存在")
            else:
                print(f"[DEBUG] ✗ .notdef字形缺失")
                
            # 列出所有字形
            glyph_names = list(glyf_table.keys())[:20]  # 只显示前20个
            print(f"[DEBUG] 字形列表(前20个): {glyph_names}")
        
        # 验证name表
        if 'name' in font:
            name_table = font['name']
            font_family = None
            for record in name_table.names:
                if record.nameID == 1:  # Font Family name
                    try:
                        font_family = record.toUnicode()
                        break
                    except:
                        pass
            print(f"[DEBUG] 字体家族名称: {font_family}")
        
        # 验证OS/2表
        if 'OS/2' in font:
            os2_table = font['OS/2']
            print(f"[DEBUG] OS/2表版本: {os2_table.version}")
            print(f"[DEBUG] 字重: {os2_table.usWeightClass}")
        
        # 验证maxp表
        if 'maxp' in font:
            maxp_table = font['maxp']
            print(f"[DEBUG] 最大字形数: {maxp_table.numGlyphs}")
            if hasattr(maxp_table, 'maxPoints'):
                print(f"[DEBUG] 最大点数: {maxp_table.maxPoints}")
            if hasattr(maxp_table, 'maxContours'):
                print(f"[DEBUG] 最大轮廓数: {maxp_table.maxContours}")
        
        # 输出处理后的字体
        output_io = io.BytesIO()
        print(f"[DEBUG] 开始保存字体...")
        font.save(output_io)
        print(f"[DEBUG] 字体保存完成")
        
        # 关闭字体对象
        font.close()
        
        # 获取输出数据
        output_data = output_io.getvalue()
        print(f"[DEBUG] 生成的字体大小: {len(output_data)} 字节")
        
        # 详细验证输出
        if len(output_data) < 100:
            raise Exception(f'生成的字体文件过小({len(output_data)}字节)')
        
        # 验证文件头
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
        
        # 尝试重新验证生成的字体
        try:
            print(f"[DEBUG] 开始验证生成的字体...")
            verify_io = io.BytesIO(output_data)
            verify_font = TTFont(verify_io)
            verify_cmap = verify_font.getBestCmap()
            print(f"[DEBUG] 验证成功！生成的字体包含 {len(verify_cmap) if verify_cmap else 0} 个字符映射")
            
            # 额外的完整性检查
            verify_glyf = verify_font.get('glyf')
            if verify_glyf:
                print(f"[DEBUG] 字形表包含 {len(verify_glyf)} 个字形")
            
            verify_font.close()
        except Exception as verify_error:
            print(f"[ERROR] 生成的字体验证失败: {verify_error}")
            import traceback
            print(f"[ERROR] 验证错误详情: {traceback.format_exc()}")
            
        # 与本地版本的兼容性检查
        print(f"[INFO] === 本地版本兼容性检查 ===")
        print(f"[INFO] 本地版本步骤: TTFont() -> Subsetter() -> populate() -> subset() -> save()")
        print(f"[INFO] Web版本步骤: 相同")
        print(f"[INFO] 输入字符: {repr(characters_to_keep)}")
        print(f"[INFO] 输出大小: {len(output_data)} 字节")
        print(f"[INFO] 应该与本地版本生成相同的结果")
        print(f"[INFO] ================================")
        
        result_base64 = base64.b64encode(output_data).decode('utf-8')
        print(f"[DEBUG] Base64编码完成，长度: {len(result_base64)} 字符")
        
        return {
            'success': True,
            'data': result_base64,
            'size': len(output_data),
            'message': f'成功处理，包含 {len(characters_to_keep)} 个字符'
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

# 测试函数可用性
def test_fonttools():
    return "FontTools库已就绪"
        `);
        
        // 测试Python环境
        try {
            const test_result = pyodide.runPython('test_fonttools()');
            console.log(`✅ ${test_result}`);
            
            // 额外测试：确保subset_font函数已定义
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
            console.error(`❌ Python环境测试失败: ${testError.message}`, testError);
        }
        
        pythonReady = true;
        console.log('🚀 专业Python字体处理引擎初始化完成！');
        
    } catch (error) {
        console.error('❌ Python引擎初始化失败，将尝试备用方案...', error);
        await loadFallbackLibrary();
    }
}

// 加载备用库
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

// 初始化拖拽功能
// 全页面拖拽相关变量
let dragCounter = 0;

function initDragAndDrop() {
    console.log('初始化拖拽功能');
    console.log('dragOverlay:', dragOverlay);
    console.log('uploadArea:', uploadArea);
    
    if (!dragOverlay) {
        console.error('拖拽覆盖层元素未找到！');
        return;
    }
    
    // 防止默认行为
    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        document.addEventListener(eventName, preventDefaults, false);
    });

    // 全页面拖拽进入/离开检测
    document.addEventListener('dragenter', handleDragEnter, false);
    document.addEventListener('dragleave', handleDragLeave, false);
    document.addEventListener('dragover', handleDragOver, false);
    document.addEventListener('drop', handlePageDrop, false);
    
    console.log('已添加全页面拖拽事件监听器');

    // 原有上传区域的拖拽处理
    if (uploadArea) {
        ['dragenter', 'dragover'].forEach(eventName => {
            uploadArea.addEventListener(eventName, highlight, false);
        });

        ['dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, unhighlight, false);
        });
    }

    // 拖拽覆盖层的点击事件（点击覆盖层隐藏）
    dragOverlay.addEventListener('click', function(e) {
        if (e.target === dragOverlay) {
            hideDragOverlay();
        }
    });

    // ESC 键支持
    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape' && dragOverlay.classList.contains('active')) {
            hideDragOverlay();
        }
    });
    
    console.log('拖拽功能初始化完成');
    
    // 添加测试按钮（仅用于调试）
    if (window.location.search.includes('debug=true')) {
        const testBtn = document.createElement('button');
        testBtn.textContent = '测试覆盖层';
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
    
    // 简化检测逻辑：只要有拖拽类型就显示覆盖层
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
    // 简化检测逻辑：只要有拖拽文件类型就显示覆盖层
    if (e.dataTransfer && e.dataTransfer.types && e.dataTransfer.types.includes('Files')) {
        showDragOverlay();
    }
}

function checkDraggedFiles(dataTransfer) {
    // 支持的字体文件扩展名
    const fontExtensions = ['.ttf', '.otf', '.woff', '.woff2'];
    
    for (let i = 0; i < dataTransfer.items.length; i++) {
        const item = dataTransfer.items[i];
        
        // 如果是文件夹，总是显示覆盖层
        if (item.kind === 'file') {
            const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
            if (entry && entry.isDirectory) {
                return true;
            }
        }
        
        // 检查文件类型
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
        
        // 检查 MIME 类型
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
    
    // 处理文件拖拽
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
    
    // 当前拖拽的文件夹结构信息（不重置现有文件）
    let currentDropFolderMode = false;
    let currentDropFolderStructure = {
        name: '',
        files: [],
        fontFiles: [],
        directories: new Set()
    };
    
    // 检查是否支持文件夹拖拽
    if (dt.items && dt.items.length > 0) {
        console.log('正在扫描拖拽的内容...');
        console.log('拖拽项目数量:', dt.items.length);
        
        // 使用DataTransferItemList处理文件夹
        const files = [];
        const scanPromises = [];
        
        // 首先检查是否有文件夹被拖拽
        let mainFolderEntry = null;
        for (let i = 0; i < dt.items.length; i++) {
            const item = dt.items[i];
            console.log(`项目 ${i}:`, item.kind, item.type);
            
            if (item.kind === 'file') {
                const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : item.getAsEntry();
                if (entry) {
                    console.log(`条目 ${i}:`, entry.name, entry.isDirectory ? '目录' : '文件');
                    
                    // 检测是否为文件夹拖拽
                    if (entry.isDirectory) {
                        currentDropFolderMode = true;
                        currentDropFolderStructure.name = entry.name;
                        mainFolderEntry = entry;
                        console.log(`📁 检测到文件夹模式: ${entry.name}`);
                        console.log('主文件夹条目:', entry.name);
                        break; // 找到主文件夹后停止，只处理这一个文件夹
                    }
                }
            }
        }
        
        // 只扫描主文件夹，避免扫描额外内容
        if (mainFolderEntry) {
            console.log('开始扫描主文件夹:', mainFolderEntry.name);
            scanPromises.push(scanEntryForCurrentDrop(mainFolderEntry, files, currentDropFolderStructure));
        } else {
            // 没有文件夹，处理单个文件
            for (let i = 0; i < dt.items.length; i++) {
                const item = dt.items[i];
                if (item.kind === 'file') {
                    const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : item.getAsEntry();
                    if (entry && entry.isFile) {
                        scanPromises.push(scanEntryForCurrentDrop(entry, files, currentDropFolderStructure));
                    } else {
                        // 后备：直接获取文件
                        const file = item.getAsFile();
                        if (file) files.push(file);
                    }
                }
            }
        }
        
        await Promise.all(scanPromises);
        
        if (files.length > 0) {
            // 合并当前拖拽的文件夹结构到全局状态
            if (currentDropFolderMode) {
                // 如果当前是文件夹模式，合并到全局文件夹结构
                folderMode = true;
                if (!folderStructure.name) {
                    folderStructure.name = currentDropFolderStructure.name;
                }
                folderStructure.files.push(...currentDropFolderStructure.files);
                folderStructure.fontFiles.push(...currentDropFolderStructure.fontFiles);
                currentDropFolderStructure.directories.forEach(dir => folderStructure.directories.add(dir));
                
                // 记录文件来源
                files.forEach(file => {
                    if (!fileSourceTracking.fromFolders.some(f => f.name === file.name && f.size === file.size)) {
                        fileSourceTracking.fromFolders.push(file);
                    }
                });
            } else {
                // 单独文件模式，记录到standalone
                files.forEach(file => {
                    if (!fileSourceTracking.standalone.some(f => f.name === file.name && f.size === file.size)) {
                        fileSourceTracking.standalone.push(file);
                    }
                });
            }
            
            const totalFiles = currentDropFolderMode ? currentDropFolderStructure.files.length : files.length;
            const nonFontFiles = totalFiles - files.length;
            
            // 更新扫描信息显示（显示在文件列表旁边）
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
        // 后备：使用传统的files方式
        const files = dt.files;
        handleFiles(files);
    }
}

// 初始化文件输入
function initFileInput() {
    fileInput.addEventListener('change', function(e) {
        handleFiles(e.target.files);
    });
    
    // 为上传区域添加点击事件
    if (uploadArea) {
        uploadArea.addEventListener('click', function(e) {
            // 确保点击的不是按钮本身
            if (!e.target.closest('button')) {
                fileInput.click();
            }
        });
        console.log('上传区域点击事件已绑定');
    } else {
        console.error('上传区域元素未找到！');
    }
}

// 初始化粘贴支持
function initPasteSupport() {
    document.addEventListener('paste', async function(e) {
        console.log('检测到粘贴事件');
        
        // 检查剪贴板是否包含内容
        const clipboardData = e.clipboardData || window.clipboardData;
        if (!clipboardData) {
            console.log('无法访问剪贴板数据');
            return;
        }
        
        // 重置文件夹状态
        folderMode = false;
        folderStructure = {
            name: '',
            files: [],
            fontFiles: [],
            directories: new Set()
        };
        
        let files = [];
        let foundFolderStructure = false;
        
        // 优先尝试处理文件夹（使用 items API）
        if (clipboardData.items && clipboardData.items.length > 0) {
            console.log(`剪贴板中发现 ${clipboardData.items.length} 个项目`);
            
            // 检查是否有文件夹条目
            for (let i = 0; i < clipboardData.items.length; i++) {
                const item = clipboardData.items[i];
                console.log(`项目 ${i}:`, item.kind, item.type);
                
                if (item.kind === 'file') {
                    // 尝试获取文件夹条目（如果支持）
                    const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
                    if (entry) {
                        console.log(`条目 ${i}:`, entry.name, entry.isDirectory ? '目录' : '文件');
                        
                        if (entry.isDirectory) {
                            console.log(`📁 检测到文件夹: ${entry.name}`);
                            folderMode = true;
                            folderStructure.name = entry.name;
                            foundFolderStructure = true;
                            
                            // 阻止默认粘贴行为
                            e.preventDefault();
                            
                            try {
                                // 扫描文件夹结构
                                await scanEntry(entry, files);
                                
                                if (files.length > 0) {
                                    const totalFiles = folderStructure.files.length;
                                    const nonFontFiles = totalFiles - files.length;
                                    
                                    console.log(`📁 文件夹扫描完成: ${totalFiles} 个文件 (${files.length} 个字体文件)`);
                                    
                                    // 更新扫描信息显示
                                    updateScanInfo(totalFiles, files.length, nonFontFiles, folderMode);
                                    
                                    // 显示成功消息
                                    showTemporaryMessage(`通过粘贴添加了文件夹 "${entry.name}"，包含 ${files.length} 个字体文件`, 'success');
                                    
                                    // 处理文件
                                    handleFiles(files);
                                } else {
                                    showTemporaryMessage(`文件夹 "${entry.name}" 中没有找到字体文件`, 'warning');
                                }
                            } catch (error) {
                                console.error('文件夹扫描失败:', error);
                                showTemporaryMessage('文件夹处理失败，请尝试拖拽文件夹', 'error');
                            }
                            return; // 处理完文件夹后退出
                        } else if (entry.isFile) {
                            // 单个文件，添加到文件列表
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
        
        // 如果没有找到文件夹结构，使用传统的 files API
        if (!foundFolderStructure) {
            const clipboardFiles = clipboardData.files;
            if (!clipboardFiles || clipboardFiles.length === 0) {
                console.log('剪贴板中没有文件');
                return;
            }
            
            console.log(`剪贴板中发现 ${clipboardFiles.length} 个文件`);
            
            // 过滤字体文件
            const fontFiles = Array.from(clipboardFiles).filter(file => {
                const extension = file.name.toLowerCase().split('.').pop();
                return ['ttf', 'otf', 'woff', 'woff2'].includes(extension);
            });
            
            if (fontFiles.length > 0) {
                console.log(`检测到 ${fontFiles.length} 个字体文件，开始处理`);
                
                // 阻止默认粘贴行为
                e.preventDefault();
                
                // 显示临时消息提示用户
                showTemporaryMessage(`通过粘贴添加了 ${fontFiles.length} 个字体文件`, 'success');
                
                // 使用现有的文件处理逻辑
                handleFiles(fontFiles);
            } else {
                console.log('剪贴板中没有字体文件');
                if (clipboardFiles.length > 0) {
                    showTemporaryMessage('剪贴板中的文件不是支持的字体格式', 'warning');
                }
            }
        }
    });
    
    console.log('全局粘贴支持已初始化（包含文件夹支持）');
}

// 递归扫描文件夹条目（与本地版本逻辑一致，记录完整结构）
async function scanEntry(entry, files, basePath = '') {
    console.log(`扫描条目: ${entry.name}, 类型: ${entry.isDirectory ? '目录' : '文件'}, 基础路径: ${basePath}`);
    
    if (entry.isFile) {
        // 这是一个文件
        return new Promise((resolve) => {
            entry.file((file) => {
                // 计算文件的相对路径
                const relativePath = basePath ? `${basePath}/${file.name}` : file.name;
                console.log(`处理文件: ${file.name}, 相对路径: ${relativePath}`);
                
                // 创建文件信息对象
                const fileInfo = {
                    file: file,
                    relativePath: relativePath,
                    isFont: false
                };
                
                // 检查是否为字体文件
                const extension = file.name.toLowerCase().split('.').pop();
                if (['ttf', 'otf', 'woff', 'woff2'].includes(extension)) {
                    fileInfo.isFont = true;
                    files.push(file); // 保持原有逻辑，只把字体文件加入selectedFiles
                    folderStructure.fontFiles.push(fileInfo);
                    console.log(`✅ 字体文件: ${relativePath}`);
                } else {
                    console.log(`📄 普通文件: ${relativePath}`);
                }
                
                // 所有文件都记录到文件夹结构中
                folderStructure.files.push(fileInfo);
                
                // 记录目录路径
                if (basePath) {
                    folderStructure.directories.add(basePath);
                }
                
                resolve();
            }, () => resolve()); // 错误时继续
        });
    } else if (entry.isDirectory) {
        // 这是一个文件夹，递归扫描（与本地版本的os.walk相同）
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
                    
                    // 继续读取（因为readEntries可能不会一次返回所有条目）
                    await readEntries();
                }, () => resolve()); // 错误时继续
            };
            readEntries();
        });
    }
}

// 为当前拖拽扫描条目（使用传入的文件夹结构）
async function scanEntryForCurrentDrop(entry, files, targetFolderStructure, basePath = '') {
    console.log(`扫描条目: ${entry.name}, 类型: ${entry.isDirectory ? '目录' : '文件'}, 基础路径: ${basePath}`);
    
    if (entry.isFile) {
        // 这是一个文件
        return new Promise((resolve) => {
            entry.file((file) => {
                // 计算文件的相对路径
                const relativePath = basePath ? `${basePath}/${file.name}` : file.name;
                console.log(`处理文件: ${file.name}, 相对路径: ${relativePath}`);
                
                // 创建文件信息对象
                const fileInfo = {
                    file: file,
                    relativePath: relativePath,
                    isFont: false
                };
                
                // 检查是否为字体文件
                const extension = file.name.toLowerCase().split('.').pop();
                if (['ttf', 'otf', 'woff', 'woff2'].includes(extension)) {
                    fileInfo.isFont = true;
                    files.push(file); // 保持原有逻辑，只把字体文件加入selectedFiles
                    targetFolderStructure.fontFiles.push(fileInfo);
                    console.log(`✅ 字体文件: ${relativePath}`);
                } else {
                    console.log(`📄 普通文件: ${relativePath}`);
                }
                
                // 所有文件都记录到传入的文件夹结构中
                targetFolderStructure.files.push(fileInfo);
                
                // 记录目录路径
                if (basePath) {
                    targetFolderStructure.directories.add(basePath);
                }
                
                resolve();
            }, () => resolve()); // 错误时继续
        });
    } else if (entry.isDirectory) {
        // 这是一个文件夹，递归扫描（与本地版本的os.walk相同）
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
                    
                    // 继续读取（因为readEntries可能不会一次返回所有条目）
                    await readEntries();
                }, () => resolve()); // 错误时继续
            };
            readEntries();
        });
    }
}

// 处理选中的文件
function handleFiles(files) {
    const fontFiles = Array.from(files).filter(file => {
        const extension = file.name.toLowerCase().split('.').pop();
        return ['ttf', 'otf', 'woff', 'woff2'].includes(extension);
    });

    if (fontFiles.length === 0) {
        console.warn('未检测到有效的字体文件，请选择 .ttf、.otf、.woff 或 .woff2 格式的文件。💡 提示：可以直接拖拽包含字体文件的文件夹！');
        return;
    }

    // 检查重复文件（基于文件名和大小）
    let addedCount = 0;
    fontFiles.forEach(file => {
        if (!selectedFiles.some(f => f.name === file.name && f.size === file.size)) {
            selectedFiles.push(file);
            addedCount++;
        }
    });

    updateFileList();
    
    // 如果不是从文件夹扫描来的，也显示扫描信息
    if (!folderMode && selectedFiles.length > 0) {
        updateScanInfo(selectedFiles.length, selectedFiles.length, 0, false);
    }
    
    if (addedCount > 0) {
        console.log(`✅ 成功添加 ${addedCount} 个字体文件，总计 ${selectedFiles.length} 个文件待处理。`);
        
        // 如果添加的文件数量比总文件数少，说明有文件夹被扫描
        if (fontFiles.length > addedCount) {
            console.log(`📁 文件夹模式：已自动扫描并添加字体文件（与本地版本保持一致）`);
        }
    } else {
        console.log(`ℹ️ 所有字体文件都已存在，未添加新文件。`);
    }
}

// 更新扫描信息显示
function updateScanInfo(totalFiles, fontFiles, nonFontFiles, isFolder) {
    if (!scanInfo || !scanInfoText) return;
    
    if (totalFiles > 0) {
        scanInfo.style.display = 'flex';
        
        let infoText = `扫描完成，发现 ${totalFiles} 个文件`;
        if (totalFiles > fontFiles) {
            infoText += ` (${fontFiles} 个字体文件, ${nonFontFiles} 个其他文件)`;
        }
        
        if (isFolder) {
            infoText += ` 📁 文件夹模式`;
        }
        
        scanInfoText.textContent = infoText;
        
        // 添加淡入动画
        scanInfo.style.opacity = '0';
        setTimeout(() => {
            scanInfo.style.opacity = '1';
        }, 100);
    } else {
        scanInfo.style.display = 'none';
    }
}

// 隐藏扫描信息
function hideScanInfo() {
    if (scanInfo) {
        scanInfo.style.display = 'none';
    }
}

// 更新文件列表显示
function updateFileList() {
    if (selectedFiles.length === 0) {
        fileList.style.display = 'none';
        hideScanInfo(); // 没有文件时隐藏扫描信息
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
}

function removeFile(index) {
    selectedFiles.splice(index, 1);
    updateFileList();
    console.log('文件已移除。');
}

function clearFiles() {
    selectedFiles = [];
    // 重置文件夹模式和扫描信息
    folderMode = false;
    folderStructure = {
        name: '',
        files: [],
        fontFiles: [],
        directories: new Set()
    };
    // 重置文件来源跟踪
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

// 创建并显示计时元素
function createTimingDisplay() {
    // 如果计时元素已存在，先移除
    if (timingText) {
        timingText.remove();
    }
    
    // 创建计时显示元素
    timingText = document.createElement('div');
    timingText.className = 'timing-text';
    timingText.innerHTML = '<i class="fas fa-clock"></i> 已耗时: 0秒';
    
    // 将计时元素添加到进度条容器中
    progressContainer.appendChild(timingText);
    
    // 开始计时更新
    startTimingUpdate();
}

// 开始计时更新
function startTimingUpdate() {
    // 清除之前的计时器
    if (timingInterval) {
        clearInterval(timingInterval);
    }
    
    // 每秒更新一次耗时显示
    timingInterval = setInterval(() => {
        if (processingStartTime) {
            const elapsedTime = Date.now() - processingStartTime;
            updateTimingDisplay(elapsedTime);
        }
    }, 1000);
    
    // 立即更新一次
    updateTimingDisplay(0);
}

// 更新计时显示
function updateTimingDisplay(elapsedTime) {
    if (!timingText) return;
    
    const seconds = Math.floor(elapsedTime / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    
    let timeString;
    if (hours > 0) {
        timeString = `${hours}小时${minutes % 60}分${seconds % 60}秒`;
    } else if (minutes > 0) {
        timeString = `${minutes}分${seconds % 60}秒`;
    } else {
        timeString = `${seconds}秒`;
    }
    
    timingText.innerHTML = `<i class="fas fa-clock"></i> 已耗时: ${timeString}`;
}

// 停止计时并显示最终耗时
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
            timeString = `${hours}小时${minutes % 60}分${seconds % 60}秒`;
        } else if (minutes > 0) {
            timeString = `${minutes}分${seconds % 60}秒`;
        } else {
            timeString = `${seconds}秒`;
        }
        
        timingText.innerHTML = `<i class="fas fa-check-circle"></i> 处理完成，总耗时: ${timeString}`;
        timingText.classList.add('timing-completed');
    }
}

// 开始处理字体
async function startProcessing() {
    if (selectedFiles.length === 0) {
        showTemporaryMessage('请先选择要处理的字体文件！', 'warning');
        return;
    }

    const characters = charactersInput.value.trim();
    if (!characters) {
        showTemporaryMessage('请输入要保留的字符！', 'warning');
        return;
    }

    if (!pythonReady && typeof opentype === 'undefined') {
        showTemporaryMessage('字体处理引擎尚未就绪，请稍候再试', 'error');
        return;
    }

    // 记录开始时间
    processingStartTime = Date.now();
    
    processBtn.disabled = true;
    processBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 处理中...';
    progressContainer.style.display = 'block';
    downloadSection.style.display = 'block'; // 立即显示下载区域
    downloadItems.innerHTML = ''; // 清空现有内容
    
    // 创建并显示计时元素
    createTimingDisplay();
    
    processedFonts = [];
    
    // 初始化下载区域标题
    const downloadTitle = downloadSection.querySelector('h2');
    downloadTitle.innerHTML = `<i class="fas fa-download"></i> 处理后的字体 <span style="font-size: 14px; color: #666; font-weight: normal;">(处理中...)</span>`;
    
    const engineType = pythonReady ? 'Python FontTools' : 'JavaScript OpenType.js';
    console.log(`开始使用 ${engineType} 处理 ${selectedFiles.length} 个字体文件...`);
    console.log(`保留字符: ${characters}`);

    try {
        for (let i = 0; i < selectedFiles.length; i++) {
            const file = selectedFiles[i];
            console.log(`正在处理: ${file.name} (${(file.size / 1024 / 1024).toFixed(1)}MB)`);
            
            updateProgress(i, selectedFiles.length);
            
            try {
                const processedFont = await processFont(file, characters);
                processedFonts.push(processedFont);
                console.log(`✅ 完成: ${file.name}`);
                
                // 立即添加这个处理完成的文件到下载区域
                addSingleDownloadItem(processedFont, processedFonts.length - 1);
                updateDownloadSectionTitle(); // 更新标题统计
                
                // 如果是第一个处理完成的文件，显示下载控制按钮
                if (processedFonts.length === 1) {
                    addBatchDownloadButton();
                }
                
                // 在处理大文件后添加小延迟，让浏览器有时间清理内存
                if (file.size > 1024 * 1024) { // 大于1MB的文件
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
            
            // 显示处理完成的成功消息
            const successCount = processedFonts.length;
            const totalCount = selectedFiles.length;
            
            if (successCount === totalCount) {
                showTemporaryMessage(`所有字体处理完成！成功处理 ${successCount} 个文件`, 'success');
            } else {
                showTemporaryMessage(`字体处理完成！成功处理 ${successCount}/${totalCount} 个文件`, 'warning');
            }
        } else {
            showTemporaryMessage('字体处理失败，没有成功处理任何文件', 'error');
        }

    } catch (error) {
        console.error(`处理过程中发生错误: ${error.message}`);
        console.error('Processing error:', error);
    } finally {
        // 停止计时并显示最终结果
        stopTimingAndShowResult();
        
        processBtn.disabled = false;
        processBtn.innerHTML = '<i class="fas fa-rocket"></i> 开始处理字体';
    }
}

// 处理单个字体文件
async function processFont(file, characters) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        
        reader.onload = async function(e) {
            try {
                const arrayBuffer = e.target.result;
                
                let subsetFont;
                
                if (pythonReady && pyodide) {
                    // 使用Python fonttools专业处理
                    subsetFont = await createPythonSubset(arrayBuffer, characters);
                } else if (typeof opentype !== 'undefined') {
                    // 使用OpenType.js备用方案
                    subsetFont = await createOpenTypeSubset(arrayBuffer, characters);
                } else {
                    throw new Error('没有可用的字体处理引擎');
                }
                
                resolve({
                    name: `simplified_${file.name}`,
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

// 使用Python fonttools创建字体子集
async function createPythonSubset(fontBuffer, characters) {
    try {
        // 正确的base64编码，保证数据完整性
        const uint8Array = new Uint8Array(fontBuffer);
        
        // 方法1：使用原生的浏览器API（最安全）
        let base64Data;
        try {
            // 直接转换整个ArrayBuffer为Base64
            const binaryString = String.fromCharCode.apply(null, uint8Array);
            base64Data = btoa(binaryString);
        } catch (rangeError) {
            // 如果数组太大，使用分块方法但保持数据完整性
            console.log('文件较大，使用分块处理...');
            
            let binaryString = '';
            const chunkSize = 8192; // 8KB chunks
            
            for (let i = 0; i < uint8Array.length; i += chunkSize) {
                const chunk = uint8Array.slice(i, i + chunkSize);
                // 安全地构建二进制字符串
                for (let j = 0; j < chunk.length; j++) {
                    binaryString += String.fromCharCode(chunk[j]);
                }
            }
            
            // 对完整的二进制字符串进行Base64编码
            base64Data = btoa(binaryString);
        }
        
        // 验证base64编码
        if (!base64Data || base64Data.length === 0) {
            throw new Error('Base64编码失败');
        }
        
        // 验证编码完整性：解码验证
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
        
        // 在Python中处理字体
        console.log(`设置Python变量: font_data_b64(${base64Data.length}字符), chars_to_keep(${characters})`);
        
        // 分批设置大型base64数据，避免内存问题
        try {
            pyodide.globals.set('font_data_b64', base64Data);
            pyodide.globals.set('chars_to_keep', characters);
        } catch (error) {
            if (error.message.includes('out of memory') || error.message.includes('stack')) {
                throw new Error(`字体文件过大(${(fontBuffer.byteLength / 1024 / 1024).toFixed(1)}MB)，建议处理较小的文件`);
            }
            throw error;
        }
        
        // 验证变量是否正确设置
        const var_check = pyodide.runPython(`
f"Python收到的变量: font_data_b64长度={len(font_data_b64)}, chars_to_keep='{chars_to_keep}'"
        `);
        console.log('Python变量验证:', var_check);
        
        // 捕获Python的print输出
        const originalConsole = pyodide.runPython(`
import sys
from io import StringIO

# 创建一个字符串缓冲区来捕获print输出
capture_output = StringIO()
original_stdout = sys.stdout
sys.stdout = capture_output
        `);
        
        let result;
        try {
            result = pyodide.runPython(`
result = subset_font(font_data_b64, chars_to_keep)

# 恢复原始stdout并获取捕获的输出
sys.stdout = original_stdout
captured_output = capture_output.getvalue()
capture_output.close()

# 将调试信息添加到结果中
result['debug_output'] = captured_output
result
            `);
        } catch (pythonError) {
            console.error('Python代码执行失败:', pythonError);
            throw new Error(`Python代码执行失败: ${pythonError.message}`);
        }
        
        // 验证result对象
        if (!result) {
            console.error('Python返回的结果无效:', result);
            throw new Error('Python处理返回了无效的结果');
        }
        
        // 显示Python调试输出 - 正确处理Pyodide Proxy对象
        console.log('Python处理结果对象类型:', typeof result);
        console.log('Python处理结果对象:', result);
        
        // 从Pyodide Proxy获取属性的正确方式
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
            
            // 尝试转换为JS对象
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
                throw new Error('无法解析Python返回的结果');
            }
        }
        
        if (debug_output) {
            console.log('=== Python调试输出 ===');
            console.log(debug_output);
            console.log('=== 调试输出结束 ===');
            
            // 也在页面日志中显示关键信息
            const debugLines = debug_output.split('\n');
            debugLines.forEach(line => {
                if (line.includes('[DEBUG]') || line.includes('[ERROR]') || line.includes('[WARNING]')) {
                    const cleanLine = line.replace(/^\[.*?\]\s*/, ''); // 移除时间戳
                    console.log(`🔍 ${cleanLine}`);
                }
            });
        } else {
            console.warn('没有收到Python调试输出');
        }
        
        if (!success) {
            // 记录详细错误信息
            console.error('Python处理失败，详细信息:', { success, message, error, error_detail });
            
            if (error_detail) {
                console.error('Python处理详细错误:', error_detail);
                
                // 分析具体错误类型并提供解决建议
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
                console.error('Python错误:', error);
            }
            
            const errorMsg = message || error || '字体处理失败，请查看详细日志';
            throw new Error(errorMsg);
        }
        
        // 使用解析出的属性
        result = { success, debug_output, error_detail, error, message, data, size };
        
        // 改进的base64解码
        const binaryString = atob(result.data);
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }
        
        // 详细验证生成的字体数据
        console.log(`JavaScript收到的字体数据大小: ${bytes.length} 字节`);
        
        if (bytes.length < 100) {
            throw new Error(`生成的字体文件过小(${bytes.length}字节)，可能损坏`);
        }
        
        // 验证TTF文件头
        const header = new DataView(bytes.buffer, 0, Math.min(12, bytes.length));
        const signature = header.getUint32(0, false);
        
        // 显示文件头的16进制
        const headerBytes = new Uint8Array(bytes.buffer, 0, Math.min(12, bytes.length));
        const headerHex = Array.from(headerBytes).map(b => b.toString(16).padStart(2, '0')).join(' ');
        console.log(`JavaScript验证文件头: ${headerHex}`);
        
        // TTF文件应该以0x00010000或'OTTO'开头
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
        
        // 额外检查：验证文件是否真的是完整的字体文件
        if (bytes.length >= 12) {
            const numTables = header.getUint16(4, false);
            console.log(`字体表数量: ${numTables}`);
            
            if (numTables === 0 || numTables > 50) {
                console.warn(`  ⚠️ 字体表数量异常: ${numTables}`);
            } else {
                console.log(`  ✅ 字体表数量正常: ${numTables}`);
            }
        }
        
        console.log(`  ✅ Python处理成功: ${result.message}`);
        
        return { buffer: bytes.buffer };
        
    } catch (error) {
        console.error(`  ❌ Python处理失败: ${error.message}`);
        console.error('Python字体处理错误:', error);
        throw error;
    }
}

// 备用方案：使用OpenType.js
async function createOpenTypeSubset(fontBuffer, characters) {
    try {
        const font = opentype.parse(fontBuffer);
        
        if (!font || !font.glyphs) {
            throw new Error('无法解析字体文件');
        }
        
        const glyphsToKeep = [];
        const charToGlyph = {};
        
        // 添加 .notdef 字形
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
        
        // 创建新字体
        const newFont = new opentype.Font({
            familyName: (font.names?.fontFamily?.en || 'SimplifiedFont'),
            styleName: (font.names?.fontSubfamily?.en || 'Regular'),
            unitsPerEm: font.unitsPerEm || 1000,
            ascender: font.ascender || 800,
            descender: font.descender || -200,
            glyphs: glyphsToKeep
        });
        
        // 设置字符映射
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

// 更新下载区域标题统计
function updateDownloadSectionTitle() {
    const downloadTitle = downloadSection.querySelector('h2');
    
    if (processedFonts.length === 0) {
        // 没有处理后的字体时，重置标题为原始状态
        downloadTitle.innerHTML = `<i class="fas fa-download"></i> 处理后的字体`;
        return;
    }
    
    const totalOriginalSize = processedFonts.reduce((sum, font) => sum + font.originalSize, 0);
    const totalNewSize = processedFonts.reduce((sum, font) => sum + font.newSize, 0);
    const totalCompressionRatio = ((totalOriginalSize - totalNewSize) / totalOriginalSize * 100).toFixed(1);
    
    downloadTitle.innerHTML = `
        <i class="fas fa-download"></i> 处理后的字体 
        <span style="font-size: 14px; color: #666; font-weight: normal;">
            ${formatFileSize(totalOriginalSize)} => ${formatFileSize(totalNewSize)} (压缩了 ${totalCompressionRatio}%)
        </span>
    `;
}

// 添加单个下载项
function addSingleDownloadItem(font, index) {
    const downloadItem = document.createElement('div');
    downloadItem.className = 'download-item';
    downloadItem.setAttribute('data-index', index); // 用于删除时识别
    
    const compressionRatio = ((font.originalSize - font.newSize) / font.originalSize * 100).toFixed(1);
    
    downloadItem.innerHTML = `
        <div class="download-info">
            <div class="download-name">${font.name}</div>
            <div class="download-size">
                ${formatFileSize(font.originalSize)} => ${formatFileSize(font.newSize)} 
                (压缩了 ${compressionRatio}%)
            </div>
        </div>
        <div class="download-actions">
            <button class="download-remove" onclick="removeProcessedFont(${index})" title="删除此处理后的字体">
                <i class="fas fa-times"></i>
            </button>
            <button class="btn btn-success" onclick="downloadFont(${index})">
                <i class="fas fa-download"></i> 下载
            </button>
        </div>
    `;
    
    downloadItems.appendChild(downloadItem);
}

// 显示下载区域（现在主要用于批量下载按钮和最终整理）
function showDownloadSection() {
    // 确保下载区域已显示（实际上在开始处理时就已显示）
    downloadSection.style.display = 'block';
    
    // 所有文件都处理完成后，添加批量下载按钮
    if (processedFonts.length > 0) {
        addBatchDownloadButton();
    }
}

// 显示批量下载按钮
function addBatchDownloadButton() {
    if (processedFonts.length > 0) {
        // 显示下载控制区域
        downloadControls.style.display = 'block';
        
        // 更新按钮文本
        updateDownloadButtonText();
    }
}

// 更新下载按钮文本（智能按钮功能）
function updateDownloadButtonText() {
    // 分析文件来源
    const standaloneCount = fileSourceTracking.standalone.length;
    const folderCount = fileSourceTracking.fromFolders.length;
    const totalCount = standaloneCount + folderCount;
    
    let downloadAllText = '';
    let downloadAllHint = '';
    
    if (totalCount === 0) {
        // 没有文件，使用默认文案
        downloadAllText = `<i class="fas fa-download"></i> 下载字体文件`;
    } else if (standaloneCount > 0 && folderCount === 0) {
        // 纯单独文件
        downloadAllText = `<i class="fas fa-download"></i> 下载所有字体文件`;
    } else if (standaloneCount === 0 && folderCount > 0) {
        // 纯文件夹文件
        downloadAllText = `<i class="fas fa-archive"></i> 下载完整文件夹 (ZIP)`;
        downloadAllHint = `<small style="display: block; margin-top: 5px; color: #666;">包含目录结构和所有非字体文件</small>`;
    } else {
        // 混合模式（既有单独文件又有文件夹文件）
        downloadAllText = `<i class="fas fa-download"></i> 下载所有字体文件`;
        downloadAllHint = `<small style="display: block; margin-top: 5px; color: #666;">${standaloneCount}个单独文件 + ${folderCount}个文件夹文件 (ZIP)</small>`;
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

// 删除处理后的字体
function removeProcessedFont(index) {
    if (index < 0 || index >= processedFonts.length) {
        console.warn('无效的字体索引:', index);
        return;
    }
    
    const font = processedFonts[index];
    console.log(`删除处理后的字体: ${font.name}`);
    
    // 从数组中移除
    processedFonts.splice(index, 1);
    
    // 重新生成所有下载项（因为索引会改变）
    updateDownloadItemsDisplay();
    
    // 更新标题统计
    updateDownloadSectionTitle();
    
    // 更新下载按钮文本
    if (downloadAllBtn && typeof updateDownloadButtonText === 'function') {
        updateDownloadButtonText();
    }
    
    // 如果没有处理后的字体了，隐藏下载控制区域
    if (processedFonts.length === 0) {
        downloadControls.style.display = 'none';
    }
    
    console.log(`已删除字体，剩余 ${processedFonts.length} 个字体`);
    
    // 显示删除成功提示
    showTemporaryMessage(`已删除字体: ${font.name}`, 'success');
}

// 更新下载项显示
function updateDownloadItemsDisplay() {
    // 清空现有显示
    downloadItems.innerHTML = '';
    
    // 重新生成所有下载项（确保索引正确）
    processedFonts.forEach((font, index) => {
        addSingleDownloadItem(font, index);
    });
}

async function downloadAllFonts() {
    // 添加调试信息
    console.log('=== downloadAllFonts 调试信息 ===');
    console.log('folderMode:', folderMode);
    console.log('folderStructure:', folderStructure);
    console.log('processedFonts.length:', processedFonts.length);
    console.log('JSZip可用:', typeof JSZip !== 'undefined');
    console.log('================================');
    
    console.log(`🔍 下载模式: ${folderMode ? '文件夹ZIP模式' : '单文件模式'}`);
    
    if (!folderMode) {
        // 非文件夹模式：单独下载每个文件
        if (processedFonts.length === 1) {
            downloadFont(0);
            return;
        }

        console.log('开始下载所有文件...');
        
        for (let i = 0; i < processedFonts.length; i++) {
            await new Promise(resolve => setTimeout(resolve, 500));
            downloadFont(i);
        }
        
        console.log('所有文件下载完成！');
    } else {
        // 文件夹模式：创建ZIP文件，保持目录结构
        console.log('🔄 切换到文件夹ZIP下载模式...');
        showZipProgress();
        await downloadFolderAsZip();
    }
}

// 文件夹模式：下载ZIP文件（与本地版本保持一致的目录结构）
async function downloadFolderAsZip() {
    console.log('=== downloadFolderAsZip 调试信息 ===');
    console.log('JSZip类型:', typeof JSZip);
    console.log('folderStructure:', folderStructure);
    console.log('folderStructure.files长度:', folderStructure.files ? folderStructure.files.length : 'undefined');
    console.log('================================');

    if (typeof JSZip === 'undefined') {
        console.error('❌ JSZip库未加载，无法创建ZIP文件');
        showTemporaryMessage('请刷新页面重试，或检查网络连接', 'error');
        return;
    }

    if (!folderStructure.files || folderStructure.files.length === 0) {
        console.error('❌ 没有找到文件夹结构数据，无法创建ZIP');
        console.error(`🔍 调试: folderStructure.files=${folderStructure.files ? folderStructure.files.length : 'null'}, folderMode=${folderMode}`);
        showTemporaryMessage('请重新拖拽文件夹后再试', 'warning');
        return;
    }

    console.log('📦 正在创建ZIP文件，保持目录结构...');
    
    try {
        const zip = new JSZip();
        const outputFolderName = `simplified_${folderStructure.name}`;
        console.log('输出文件夹名称:', outputFolderName);
        
        // 第1步：创建目录结构 (10%)
        updateZipProgress(10, '正在创建目录结构...', `创建 ${folderStructure.directories.size} 个目录`);
        console.log('开始创建目录，总数:', folderStructure.directories.size);
        let dirCount = 0;
        folderStructure.directories.forEach(dirPath => {
            const fullPath = `${outputFolderName}/${dirPath}/`;
            zip.folder(fullPath);
            dirCount++;
            if (dirCount <= 5) { // 只显示前5个目录
                console.log('创建目录:', fullPath);
            }
        });
        console.log(`✅ 完成创建 ${dirCount} 个目录`);
        
        // 第2步：准备字体映射 (20%)
        updateZipProgress(20, '正在准备字体文件...', `映射 ${processedFonts.length} 个处理后的字体`);
        const processedFontMap = new Map();
        processedFonts.forEach(font => {
            const originalName = font.name.replace(/^simplified_/, '');
            processedFontMap.set(originalName, font.data);
            console.log(`映射字体: ${originalName} -> ${font.data ? font.data.byteLength + '字节' : 'null'}`);
        });
        console.log(`✅ 字体映射完成，共 ${processedFontMap.size} 个字体`);
        
        // 第3步：添加文件到ZIP (20% -> 80%)
        console.log('开始添加文件到ZIP，总数:', folderStructure.files.length);
        let addedFiles = 0;
        let skippedFiles = 0;
        const totalFiles = folderStructure.files.length;
        
        for (let i = 0; i < folderStructure.files.length; i++) {
            const fileInfo = folderStructure.files[i];
            const { file, relativePath, isFont } = fileInfo;
            
            // 更新进度 (20% -> 80%)
            const fileProgress = 20 + (i / totalFiles) * 60;
            updateZipProgress(fileProgress, '正在添加文件...', `处理 ${relativePath} (${i + 1}/${totalFiles})`);
            
            try {
                if (isFont) {
                    // 字体文件：使用处理后的数据
                    const processedData = processedFontMap.get(file.name);
                    if (processedData) {
                        zip.file(`${outputFolderName}/${relativePath}`, processedData);
                        console.log(`✅ 添加处理后的字体: ${relativePath} (${processedData.byteLength}字节)`);
                        addedFiles++;
                    } else {
                        console.log(`❌ 未找到处理后的字体数据: ${file.name}`);
                        skippedFiles++;
                    }
                } else {
                    // 非字体文件：直接复制原文件
                    const fileData = await readFileAsArrayBuffer(file);
                    zip.file(`${outputFolderName}/${relativePath}`, fileData);
                    console.log(`✅ 复制原文件: ${relativePath} (${fileData.byteLength}字节)`);
                    addedFiles++;
                }
            } catch (error) {
                console.error(`❌ 处理文件失败 ${relativePath}:`, error);
                skippedFiles++;
            }
        }
        
        console.log(`✅ 文件添加完成: 成功${addedFiles}个, 跳过${skippedFiles}个`);
        console.log(`📦 已添加 ${addedFiles} 个文件到ZIP中`);
        
        // 第4步：生成ZIP文件 (80% -> 95%)
        updateZipProgress(80, '正在生成ZIP文件...', '压缩数据，请稍候...');
        console.log('📦 正在生成ZIP文件...');
        console.log('开始生成ZIP文件...');
        
        // 生成ZIP文件
        const zipBlob = await zip.generateAsync({
            type: 'blob',
            compression: 'DEFLATE',
            compressionOptions: {
                level: 6
            }
        });
        
        console.log(`✅ ZIP文件生成完成，大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log(`📦 ZIP文件大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        
        // 第5步：准备下载 (95% -> 100%)
        updateZipProgress(95, '正在准备下载...', `文件大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`);
        console.log('开始下载ZIP文件...');
        const url = URL.createObjectURL(zipBlob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `${outputFolderName}.zip`;
        
        console.log('下载链接:', url);
        console.log('下载文件名:', `${outputFolderName}.zip`);
        
        // 完成
        updateZipProgress(100, '下载完成！', `${outputFolderName}.zip 已开始下载`);
        
        document.body.appendChild(a);
        console.log('触发下载...');
        a.click();
        console.log('下载已触发');
        
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        
        // 使用已经声明的totalFiles变量
        const fontFiles = folderStructure.fontFiles.length;
        const nonFontFiles = totalFiles - fontFiles;
        
        console.log(`🎉 ZIP文件下载完成！`);
        console.log(`📊 包含: ${fontFiles} 个处理后的字体文件, ${nonFontFiles} 个原始文件`);
        console.log(`📁 完整目录结构已保持，与本地版本处理结果一致`);
        console.log('ZIP下载过程完成');
        
        // 隐藏进度条
        hideZipProgress();
        
    } catch (error) {
        console.error(`❌创建ZIP文件失败: ${error.message}`);
        console.error('ZIP creation error:', error);
        
        // 出错时也要隐藏进度条
        hideZipProgress();
    }
}

// 辅助函数：读取文件为ArrayBuffer
function readFileAsArrayBuffer(file) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = () => resolve(reader.result);
        reader.onerror = () => reject(new Error('文件读取失败'));
        reader.readAsArrayBuffer(file);
    });
}

// ZIP进度条显示和控制函数
function showZipProgress() {
    if (zipProgressContainer) {
        zipProgressContainer.style.display = 'block';
        zipProgressFill.style.width = '0%';
        zipProgressText.textContent = '正在准备ZIP生成...';
        zipProgressDetails.textContent = '初始化中...';
    }
}

function hideZipProgress() {
    if (zipProgressContainer) {
        setTimeout(() => {
            zipProgressContainer.style.display = 'none';
        }, 2000); // 2秒后隐藏，让用户看到完成状态
    }
}

function updateZipProgress(percentage, statusText, detailText) {
    if (zipProgressFill && zipProgressText && zipProgressDetails) {
        zipProgressFill.style.width = `${Math.min(100, Math.max(0, percentage))}%`;
        zipProgressText.textContent = statusText;
        zipProgressDetails.textContent = detailText;
        
        // 添加一点动画效果
        if (percentage >= 100) {
            zipProgressFill.style.background = 'linear-gradient(90deg, #4caf50, #8bc34a)';
            zipProgressText.innerHTML = '<i class="fas fa-check"></i> ' + statusText;
        }
    }
}

// 清理全部已处理的文件
function clearAllProcessedFiles() {
    console.log('🧹 开始清理全部文件和处理结果...');
    
    // 清空已选择的文件数组
    selectedFiles = [];
    
    // 清空已处理的字体数组
    processedFonts = [];
    
    // 重置文件夹模式相关变量
    folderMode = false;
    folderStructure = {
        name: '',
        files: [],
        fontFiles: [],
        directories: new Set()
    };
    
    // 重置文件来源跟踪
    fileSourceTracking = {
        standalone: [],
        fromFolders: []
    };
    
    // 隐藏和重置文件列表
    updateFileList();
    hideScanInfo();
    
    // 隐藏下载区域
    downloadSection.style.display = 'none';
    downloadItems.innerHTML = '';
    downloadControls.style.display = 'none';
    
    // 重置进度条
    resetProgressBar();
    
    // 重置计时显示
    resetTimingDisplay();
    
    // 重置处理按钮状态
    processBtn.disabled = false;
    processBtn.innerHTML = '<i class="fas fa-rocket"></i> 开始处理字体';
    
    // 重置处理开始时间
    processingStartTime = null;
    
    // 清空文件输入框的值
    if (fileInput) {
        fileInput.value = '';
    }
    
    console.log('✅ 完全清理完成！已重置到初始状态');
    
    // 显示清理成功的提示
    showTemporaryMessage('已清理全部文件和处理结果，界面已重置', 'success');
}

// 重置进度条
function resetProgressBar() {
    if (progressContainer) {
        progressContainer.style.display = 'none';
        progressFill.style.width = '0%';
        progressText.textContent = '0%';
    }
}

// 重置计时显示
function resetTimingDisplay() {
    // 清除计时器
    if (timingInterval) {
        clearInterval(timingInterval);
        timingInterval = null;
    }
    
    // 移除计时显示元素
    if (timingText) {
        timingText.remove();
        timingText = null;
    }
}

// 显示临时消息提示
function showTemporaryMessage(message, type = 'info') {
    // 根据类型选择合适的图标
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
    
    // 创建消息元素
    const messageDiv = document.createElement('div');
    messageDiv.className = `temporary-message ${type}`;
    messageDiv.innerHTML = `
        <i class="fas fa-${iconClass}"></i>
        <span>${message}</span>
    `;
    
    // 添加到页面顶部
    document.body.insertBefore(messageDiv, document.body.firstChild);
    
    // 添加动画效果
    setTimeout(() => {
        messageDiv.classList.add('show');
    }, 100);
    
    // 3秒后自动移除
    setTimeout(() => {
        messageDiv.classList.remove('show');
        setTimeout(() => {
            if (messageDiv.parentNode) {
                messageDiv.parentNode.removeChild(messageDiv);
            }
        }, 300);
    }, 3000);
}

// 文件列表滚动进度条
function updateFileScrollProgress() {
    if (!fileItems || fileItems.children.length === 0) {
        fileScrollFill.style.width = '0%';
        return;
    }
    
    const scrollTop = fileItems.scrollTop;
    const scrollHeight = fileItems.scrollHeight;
    const clientHeight = fileItems.clientHeight;
    
    // 如果内容高度小于等于容器高度，则不需要滚动条
    if (scrollHeight <= clientHeight) {
        fileScrollFill.style.width = '100%';
        return;
    }
    
    // 计算滚动百分比
    const scrollPercentage = (scrollTop / (scrollHeight - clientHeight)) * 100;
    fileScrollFill.style.width = Math.min(100, Math.max(0, scrollPercentage)) + '%';
}

// 初始化文件列表滚动监听器
function initFileScrollProgress() {
    if (fileItems) {
        fileItems.addEventListener('scroll', updateFileScrollProgress);
        // 内容变化时也更新进度条
        const observer = new MutationObserver(updateFileScrollProgress);
        observer.observe(fileItems, { childList: true, subtree: true });
    }
}

// 在页面加载完成后初始化
document.addEventListener('DOMContentLoaded', function() {
    initFileScrollProgress();
});

// 错误处理
window.addEventListener('error', function(e) {
    console.error(`发生错误: ${e.message}`);
});

window.addEventListener('unhandledrejection', function(e) {
    console.error(`Promise错误: ${e.reason}`);
    e.preventDefault();
});