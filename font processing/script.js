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

// DOM 元素
const uploadArea = document.getElementById('uploadArea');
const fileInput = document.getElementById('fileInput');
const fileList = document.getElementById('fileList');
const fileItems = document.getElementById('fileItems');
const charactersInput = document.getElementById('charactersInput');
const processBtn = document.getElementById('processBtn');
const progressContainer = document.getElementById('progressContainer');
const progressFill = document.getElementById('progressFill');
const progressText = document.getElementById('progressText');
const logContainer = document.getElementById('logContainer');
const downloadSection = document.getElementById('downloadSection');
const downloadItems = document.getElementById('downloadItems');

// ZIP进度条元素（动态获取，因为是在按钮创建后才有的）
let zipProgressContainer = null;
let zipProgressFill = null;
let zipProgressText = null;
let zipProgressDetails = null;

// 初始化
document.addEventListener('DOMContentLoaded', function() {
    initPyodide();
    initDragAndDrop();
    initFileInput();
    logMessage('正在初始化专业Python字体处理引擎...');
    
    // 检查JSZip库是否加载
    setTimeout(() => {
        if (typeof JSZip !== 'undefined') {
            logMessage('✅ JSZip库加载成功，支持文件夹ZIP下载', 'success');
        } else {
            logMessage('❌ JSZip库加载失败，ZIP下载功能将不可用', 'error');
        }
    }, 2000);
});

// 初始化Pyodide (Python in Browser)
async function initPyodide() {
    try {
        logMessage('📦 正在加载Python运行环境，请稍候...');
        
        // 加载Pyodide
        pyodide = await loadPyodide();
        
        logMessage('📚 正在安装fonttools库...');
        
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
            logMessage(`✅ ${test_result}`, 'success');
            
            // 额外测试：确保subset_font函数已定义
            const function_test = pyodide.runPython(`
import inspect
if 'subset_font' in globals():
    sig = inspect.signature(subset_font)
    f"subset_font函数已定义，参数: {list(sig.parameters.keys())}"
else:
    "ERROR: subset_font函数未定义"
            `);
            logMessage(`🔧 ${function_test}`, 'info');
            
        } catch (testError) {
            logMessage(`❌ Python环境测试失败: ${testError.message}`, 'error');
            console.error('Python测试错误:', testError);
        }
        
        pythonReady = true;
        logMessage('🚀 专业Python字体处理引擎初始化完成！', 'success');
        logMessage('系统已准备好处理字体文件，支持与桌面版相同的处理质量。');
        
    } catch (error) {
        logMessage('❌ Python引擎初始化失败，将尝试备用方案...', 'error');
        console.error('Pyodide initialization error:', error);
        await loadFallbackLibrary();
    }
}

// 加载备用库
async function loadFallbackLibrary() {
    try {
        const script = document.createElement('script');
        script.src = 'https://cdnjs.cloudflare.com/ajax/libs/opentype.js/1.3.4/opentype.min.js';
        script.onload = () => {
            logMessage('📋 备用字体处理库已加载，功能有限。', 'warning');
        };
        script.onerror = () => {
            logMessage('❌ 无法加载任何字体处理库。', 'error');
        };
        document.head.appendChild(script);
    } catch (error) {
        logMessage('❌ 备用库加载失败。', 'error');
    }
}

// 初始化拖拽功能
function initDragAndDrop() {
    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        uploadArea.addEventListener(eventName, preventDefaults, false);
        document.body.addEventListener(eventName, preventDefaults, false);
    });

    ['dragenter', 'dragover'].forEach(eventName => {
        uploadArea.addEventListener(eventName, highlight, false);
    });

    ['dragleave', 'drop'].forEach(eventName => {
        uploadArea.addEventListener(eventName, unhighlight, false);
    });

    uploadArea.addEventListener('drop', handleDrop, false);
}

function preventDefaults(e) {
    e.preventDefault();
    e.stopPropagation();
}

function highlight(e) {
    uploadArea.classList.add('drag-over');
}

function unhighlight(e) {
    uploadArea.classList.remove('drag-over');
}

async function handleDrop(e) {
    const dt = e.dataTransfer;
    
    // 重置文件夹结构信息
    folderMode = false;
    folderStructure = {
        name: '',
        files: [],
        fontFiles: [],
        directories: new Set()
    };
    
    // 检查是否支持文件夹拖拽
    if (dt.items && dt.items.length > 0) {
        logMessage('正在扫描拖拽的内容...', 'info');
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
                        folderMode = true;
                        folderStructure.name = entry.name;
                        mainFolderEntry = entry;
                        logMessage(`📁 检测到文件夹模式: ${entry.name}`, 'info');
                        console.log('主文件夹条目:', entry.name);
                        break; // 找到主文件夹后停止，只处理这一个文件夹
                    }
                }
            }
        }
        
        // 只扫描主文件夹，避免扫描额外内容
        if (mainFolderEntry) {
            console.log('开始扫描主文件夹:', mainFolderEntry.name);
            scanPromises.push(scanEntry(mainFolderEntry, files));
        } else {
            // 没有文件夹，处理单个文件
            for (let i = 0; i < dt.items.length; i++) {
                const item = dt.items[i];
                if (item.kind === 'file') {
                    const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : item.getAsEntry();
                    if (entry && entry.isFile) {
                        scanPromises.push(scanEntry(entry, files));
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
            const totalFiles = folderStructure.files.length;
            const nonFontFiles = totalFiles - files.length;
            
            logMessage(`📁 扫描完成，发现 ${totalFiles} 个文件 (${files.length} 个字体文件, ${nonFontFiles} 个其他文件)`, 'info');
            
            if (folderMode) {
                logMessage(`📁 文件夹模式启用: 将保持目录结构并复制所有文件`, 'success');
                logMessage(`🔍 调试: 目录数=${folderStructure.directories.size}, 文件数=${folderStructure.files.length}`, 'info');
            }
            
            handleFiles(files);
        } else {
            logMessage('未在拖拽的内容中找到任何文件', 'warning');
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

// 处理选中的文件
function handleFiles(files) {
    const fontFiles = Array.from(files).filter(file => {
        const extension = file.name.toLowerCase().split('.').pop();
        return ['ttf', 'otf', 'woff', 'woff2'].includes(extension);
    });

    if (fontFiles.length === 0) {
        logMessage('未检测到有效的字体文件，请选择 .ttf、.otf、.woff 或 .woff2 格式的文件。\n💡 提示：可以直接拖拽包含字体文件的文件夹！', 'warning');
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
    
    if (addedCount > 0) {
        logMessage(`✅ 成功添加 ${addedCount} 个字体文件，总计 ${selectedFiles.length} 个文件待处理。`, 'success');
        
        // 如果添加的文件数量比总文件数少，说明有文件夹被扫描
        if (fontFiles.length > addedCount) {
            logMessage(`📁 文件夹模式：已自动扫描并添加字体文件（与本地版本保持一致）`, 'info');
        }
    } else {
        logMessage(`ℹ️ 所有字体文件都已存在，未添加新文件。`, 'info');
    }
}

// 更新文件列表显示
function updateFileList() {
    if (selectedFiles.length === 0) {
        fileList.style.display = 'none';
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
    logMessage('文件已移除。', 'warning');
}

function clearFiles() {
    selectedFiles = [];
    updateFileList();
    logMessage('已清除所有文件。', 'warning');
}

function setCharacters(chars) {
    charactersInput.value = chars;
    logMessage(`已设置要保留的字符: ${chars}`);
}

function formatFileSize(bytes) {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function logMessage(message, type = 'info') {
    const logDiv = document.createElement('div');
    logDiv.className = `log-message ${type}`;
    
    const timestamp = new Date().toLocaleTimeString();
    logDiv.textContent = `[${timestamp}] ${message}`;
    
    logContainer.appendChild(logDiv);
    logContainer.scrollTop = logContainer.scrollHeight;
}

function updateProgress(current, total) {
    const percentage = (current / total) * 100;
    progressFill.style.width = `${percentage}%`;
    progressText.textContent = `${Math.round(percentage)}% (${current}/${total})`;
}

// 开始处理字体
async function startProcessing() {
    if (selectedFiles.length === 0) {
        logMessage('请先选择要处理的字体文件！', 'error');
        return;
    }

    const characters = charactersInput.value.trim();
    if (!characters) {
        logMessage('请输入要保留的字符！', 'error');
        return;
    }

    if (!pythonReady && typeof opentype === 'undefined') {
        logMessage('字体处理引擎尚未就绪，请稍候再试。', 'error');
        return;
    }

    processBtn.disabled = true;
    processBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 处理中...';
    progressContainer.style.display = 'block';
    downloadSection.style.display = 'block'; // 立即显示下载区域
    downloadItems.innerHTML = ''; // 清空现有内容
    
    processedFonts = [];
    
    // 初始化下载区域标题
    const downloadTitle = downloadSection.querySelector('h2');
    downloadTitle.innerHTML = `<i class="fas fa-download"></i> 下载处理后的字体 <span style="font-size: 14px; color: #666; font-weight: normal;">(处理中...)</span>`;
    
    const engineType = pythonReady ? 'Python FontTools' : 'JavaScript OpenType.js';
    logMessage(`开始使用 ${engineType} 处理 ${selectedFiles.length} 个字体文件...`, 'info');
    logMessage(`保留字符: ${characters}`, 'info');

    try {
        for (let i = 0; i < selectedFiles.length; i++) {
            const file = selectedFiles[i];
            logMessage(`正在处理: ${file.name} (${(file.size / 1024 / 1024).toFixed(1)}MB)`);
            
            updateProgress(i, selectedFiles.length);
            
            try {
                const processedFont = await processFont(file, characters);
                processedFonts.push(processedFont);
                logMessage(`✅ 完成: ${file.name}`, 'success');
                
                // 立即添加这个处理完成的文件到下载区域
                addSingleDownloadItem(processedFont, processedFonts.length - 1);
                updateDownloadSectionTitle(); // 更新标题统计
                
                // 在处理大文件后添加小延迟，让浏览器有时间清理内存
                if (file.size > 1024 * 1024) { // 大于1MB的文件
                    await new Promise(resolve => setTimeout(resolve, 100));
                }
                
            } catch (error) {
                logMessage(`❌ 处理失败 ${file.name}: ${error.message}`, 'error');
                console.error('Font processing error:', error);
            }
        }

        updateProgress(selectedFiles.length, selectedFiles.length);
        logMessage(`🎉 所有字体处理完成！成功处理 ${processedFonts.length}/${selectedFiles.length} 个文件`, 'success');
        
        if (processedFonts.length > 0) {
            showDownloadSection();
        }

    } catch (error) {
        logMessage(`处理过程中发生错误: ${error.message}`, 'error');
        console.error('Processing error:', error);
    } finally {
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
                    logMessage(`🔍 ${cleanLine}`, 'info');
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
                    logMessage('❌ 字体文件数据损坏或格式不兼容', 'error');
                    if (error_detail.includes('assert len(data) == self.length')) {
                        logMessage('💡 建议：这可能是Base64编码问题，已自动修复，请重试', 'warning');
                    }
                } else if (error_detail.includes('cmap')) {
                    logMessage('❌ 字体字符映射表(cmap)读取失败', 'error');
                    logMessage('💡 建议：请检查字体文件是否完整或选择其他字体', 'warning');
                } else if (error_detail.includes('Memory')) {
                    logMessage('❌ 内存不足，文件过大', 'error');
                    logMessage('💡 建议：请处理较小的字体文件（<5MB）', 'warning');
                } else if (error_detail.includes('base64')) {
                    logMessage('❌ Base64编码解码失败', 'error');
                    logMessage('💡 建议：文件可能损坏，请重新选择文件', 'warning');
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
            logMessage('  ✅ JavaScript验证：有效的TTF格式字体', 'success');
        } else if (signature === 0x4F54544F) {
            logMessage('  ✅ JavaScript验证：有效的OTF格式字体', 'success');
        } else {
            const hex = signature.toString(16).padStart(8, '0');
            logMessage(`  ⚠️ JavaScript验证：意外的文件签名: 0x${hex}`, 'warning');
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
                logMessage(`  ⚠️ 字体表数量异常: ${numTables}`, 'warning');
            } else {
                logMessage(`  ✅ 字体表数量正常: ${numTables}`, 'success');
            }
        }
        
        logMessage(`  ✅ Python处理成功: ${result.message}`, 'success');
        
        return { buffer: bytes.buffer };
        
    } catch (error) {
        logMessage(`  ❌ Python处理失败: ${error.message}`, 'error');
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
        
        logMessage(`  📋 JavaScript备用处理完成，包含 ${foundChars} 个字符`);
        
        return { buffer };
        
    } catch (error) {
        logMessage(`  ❌ JavaScript处理失败: ${error.message}`, 'error');
        throw error;
    }
}

// 更新下载区域标题统计
function updateDownloadSectionTitle() {
    if (processedFonts.length === 0) return;
    
    const totalOriginalSize = processedFonts.reduce((sum, font) => sum + font.originalSize, 0);
    const totalNewSize = processedFonts.reduce((sum, font) => sum + font.newSize, 0);
    const totalCompressionRatio = ((totalOriginalSize - totalNewSize) / totalOriginalSize * 100).toFixed(1);
    
    const downloadTitle = downloadSection.querySelector('h2');
    downloadTitle.innerHTML = `
        <i class="fas fa-download"></i> 下载处理后的字体 
        <span style="font-size: 14px; color: #666; font-weight: normal;">
            ${formatFileSize(totalOriginalSize)} => ${formatFileSize(totalNewSize)} (压缩了 ${totalCompressionRatio}%)
        </span>
    `;
}

// 添加单个下载项
function addSingleDownloadItem(font, index) {
    const downloadItem = document.createElement('div');
    downloadItem.className = 'download-item';
    
    const compressionRatio = ((font.originalSize - font.newSize) / font.originalSize * 100).toFixed(1);
    
    downloadItem.innerHTML = `
        <div class="download-info">
            <div class="download-name">${font.name}</div>
            <div class="download-size">
                ${formatFileSize(font.originalSize)} => ${formatFileSize(font.newSize)} 
                (压缩了 ${compressionRatio}%)
            </div>
        </div>
        <button class="btn btn-success" onclick="downloadFont(${index})">
            <i class="fas fa-download"></i> 下载
        </button>
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

// 添加批量下载按钮
function addBatchDownloadButton() {
    // 检查是否已存在批量下载按钮
    if (downloadItems.querySelector('.batch-download-div')) {
        return; // 已存在，不重复添加
    }
    
    if (processedFonts.length > 1) {
        const batchDownloadDiv = document.createElement('div');
        batchDownloadDiv.className = 'batch-download-div';
        batchDownloadDiv.style.textAlign = 'center';
        batchDownloadDiv.style.marginTop = '16px';
        const downloadAllText = folderMode ? 
            `<i class="fas fa-archive"></i> 下载完整文件夹 (ZIP)` : 
            `<i class="fas fa-download"></i> 下载所有字体文件`;
            
        const downloadAllHint = folderMode ? 
            `<small style="display: block; margin-top: 5px; color: #666;">包含目录结构和所有非字体文件</small>` : 
            '';
            
        batchDownloadDiv.innerHTML = `
            <button class="btn btn-primary btn-large" onclick="downloadAllFonts()">
                ${downloadAllText}
                ${downloadAllHint}
            </button>
            
            <!-- ZIP生成进度条 -->
            <div class="zip-progress-container" id="zipProgressContainer" style="display: none;">
                <div class="zip-progress-header">
                    <i class="fas fa-archive"></i>
                    <span id="zipProgressText">正在生成ZIP文件...</span>
                </div>
                <div class="zip-progress-bar">
                    <div class="zip-progress-fill" id="zipProgressFill"></div>
                </div>
                <div class="zip-progress-details" id="zipProgressDetails">准备中...</div>
            </div>
        `;
        downloadItems.appendChild(batchDownloadDiv);
    }



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
    
    logMessage(`已下载: ${font.name}`, 'success');
}

async function downloadAllFonts() {
    // 添加调试信息
    console.log('=== downloadAllFonts 调试信息 ===');
    console.log('folderMode:', folderMode);
    console.log('folderStructure:', folderStructure);
    console.log('processedFonts.length:', processedFonts.length);
    console.log('JSZip可用:', typeof JSZip !== 'undefined');
    console.log('================================');
    
    logMessage(`🔍 下载模式: ${folderMode ? '文件夹ZIP模式' : '单文件模式'}`, 'info');
    
    if (!folderMode) {
        // 非文件夹模式：单独下载每个文件
        if (processedFonts.length === 1) {
            downloadFont(0);
            return;
        }

        logMessage('开始下载所有文件...', 'info');
        
        for (let i = 0; i < processedFonts.length; i++) {
            await new Promise(resolve => setTimeout(resolve, 500));
            downloadFont(i);
        }
        
        logMessage('所有文件下载完成！', 'success');
    } else {
        // 文件夹模式：创建ZIP文件，保持目录结构
        logMessage('🔄 切换到文件夹ZIP下载模式...', 'info');
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
        logMessage('❌ JSZip库未加载，无法创建ZIP文件', 'error');
        logMessage('💡 请刷新页面重试，或检查网络连接', 'warning');
        return;
    }

    if (!folderStructure.files || folderStructure.files.length === 0) {
        logMessage('❌ 没有找到文件夹结构数据，无法创建ZIP', 'error');
        logMessage(`🔍 调试: folderStructure.files=${folderStructure.files ? folderStructure.files.length : 'null'}, folderMode=${folderMode}`, 'error');
        logMessage('💡 请重新拖拽文件夹后再试', 'warning');
        return;
    }

    logMessage('📦 正在创建ZIP文件，保持目录结构...', 'info');
    
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
        logMessage(`📦 已添加 ${addedFiles} 个文件到ZIP中`, 'info');
        
        // 第4步：生成ZIP文件 (80% -> 95%)
        updateZipProgress(80, '正在生成ZIP文件...', '压缩数据，请稍候...');
        logMessage('📦 正在生成ZIP文件...', 'info');
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
        logMessage(`📦 ZIP文件大小: ${(zipBlob.size / 1024 / 1024).toFixed(2)}MB`, 'info');
        
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
        
        logMessage(`🎉 ZIP文件下载完成！`, 'success');
        logMessage(`📊 包含: ${fontFiles} 个处理后的字体文件, ${nonFontFiles} 个原始文件`, 'success');
        logMessage(`📁 完整目录结构已保持，与本地版本处理结果一致`, 'success');
        console.log('ZIP下载过程完成');
        
        // 隐藏进度条
        hideZipProgress();
        
    } catch (error) {
        logMessage(`❌创建ZIP文件失败: ${error.message}`, 'error');
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
    // 动态获取进度条元素
    zipProgressContainer = document.getElementById('zipProgressContainer');
    zipProgressFill = document.getElementById('zipProgressFill');
    zipProgressText = document.getElementById('zipProgressText');
    zipProgressDetails = document.getElementById('zipProgressDetails');
    
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

// 错误处理
window.addEventListener('error', function(e) {
    logMessage(`发生错误: ${e.message}`, 'error');
});

window.addEventListener('unhandledrejection', function(e) {
    logMessage(`Promise错误: ${e.reason}`, 'error');
    e.preventDefault();
});