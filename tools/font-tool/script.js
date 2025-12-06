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
    console.log('DOM Loaded, initializing');
    
    const overlay = document.getElementById('dragOverlay');
    console.log('dragOverlay element:', overlay);
    
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
            console.log('✅ JSZip loaded successfully, folder ZIP download supported');
        } else {
            console.log('❌ JSZip failed to load, ZIP download will be unavailable');
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
    
    console.log('🎨 Font processing engine loading status shown');
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
    
    console.log('🎨 Font processing engine loading status hidden');
}

function updateEngineLoadingStatus(message) {
    if (engineLoadingStatus) {
        engineLoadingStatus.textContent = message;
    }
    console.log(`⚙️ ${message}`);
}

async function initPyodideAsync() {
    try {
        updateEngineLoadingStatus('Loading processing engine...');
        
        pyodide = await loadPyodide();
        
        updateEngineLoadingStatus('Installing core libraries...');
        
        await pyodide.loadPackage(['micropip']);
        
        updateEngineLoadingStatus('Configuring font processing components...');
        
        await pyodide.runPythonAsync(`
            import micropip
            await micropip.install(['fonttools'])
        `);
        
        updateEngineLoadingStatus('Initializing font processing engine...');
        
        await loadPythonFontProcessor();
        
        await testPythonEnvironment();
        
        pythonReady = true;
        updateEngineLoadingStatus('Font processing engine ready!');
        
        setTimeout(() => {
            hideEngineLoadingStatus();
        }, 1000);
        
        console.log('🚀 Professional font processing engine initialized!');
        
    } catch (error) {
        console.error('❌ Engine initialization failed, attempting fallback...', error);
        updateEngineLoadingStatus('Engine load failed, using fallback...');
        
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
    Stricter font subsetting - Thorough cleanup of extra characters and composite glyphs
    """
    try:
        from fontTools.ttLib import TTFont
        from fontTools.subset import Subsetter, Options
        import base64
        import io
        
        print(f"[DEBUG] Starting strict font processing, characters to keep: {characters_to_keep}")
        print(f"[DEBUG] Base64 data length: {len(font_data_base64)} chars")
        
        font_data = base64.b64decode(font_data_base64)
        print(f"[DEBUG] Decoded font data size: {len(font_data)} bytes")
        
        if len(font_data) >= 12:
            original_header = font_data[:12]
            header_hex = ' '.join(f'{b:02x}' for b in original_header)
            print(f"[DEBUG] Original font header: {header_hex}")
            
            signature = int.from_bytes(font_data[:4], 'big')
            if signature == 0x00010000:
                print("[DEBUG] Original file: Valid TTF format")
            elif signature == 0x4F54544F:
                print("[DEBUG] Original file: Valid OTF format") 
            else:
                print(f"[DEBUG] Original file: Unknown format 0x{signature:08x}")
        
        font_io = io.BytesIO(font_data)
        font = TTFont(font_io)
        
        print(f"[DEBUG] Font loaded successfully")
        print(f"[DEBUG] Original table count: {len(font.keys())}")
        print(f"[DEBUG] Original table list: {sorted(list(font.keys()))}")
        
        if 'head' in font:
            head = font['head']
            print(f"[DEBUG] unitsPerEm: {head.unitsPerEm}")
            print(f"[DEBUG] Font created: {head.created}")
        
        if 'cmap' in font:
            cmap = font.getBestCmap()
            print(f"[DEBUG] Char map count: {len(cmap) if cmap else 0}")
            
            found_chars = []
            for char in characters_to_keep:
                char_code = ord(char)
                if cmap and char_code in cmap:
                    found_chars.append(char)
                    print(f"[DEBUG] Found char '{char}' (U+{char_code:04X}) -> Glyph{cmap[char_code]}")
                else:
                    print(f"[DEBUG] Char not found '{char}' (U+{char_code:04X})")
            
            if not found_chars:
                raise Exception(f'No specified characters found in font. Font contains range: U+{min(cmap.keys()):04X} - U+{max(cmap.keys()):04X}')
        
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
        print(f"[DEBUG] Strict subsetter created with cleanup options")
        
        print(f"[DEBUG] Strict mode: Keeping only {repr(characters_to_keep)}")
        subsetter.populate(text=characters_to_keep)
        print(f"[DEBUG] Char setup complete: {repr(characters_to_keep)} (Strict mode)")
        
        print(f"[DEBUG] Starting strict subsetting...")
        subsetter.subset(font)
        print(f"[DEBUG] Strict subsetting complete")
        
        print(f"[DEBUG] Post-process table count: {len(font.keys())}")
        print(f"[DEBUG] Post-process table list: {sorted(list(font.keys()))}")
        
        critical_tables = ['cmap', 'head', 'hhea', 'hmtx', 'maxp', 'name']
        for table in critical_tables:
            if table in font:
                print(f"[DEBUG] ✓ Critical table '{table}' exists")
            else:
                print(f"[DEBUG] ✗ Critical table '{table}' missing")
        
        if 'cmap' in font:
            new_cmap = font.getBestCmap()
            print(f"[DEBUG] Post-process char map count: {len(new_cmap) if new_cmap else 0}")
            if new_cmap:
                has_space = 32 in new_cmap
                has_null = 0 in new_cmap
                print(f"[DEBUG] Critical char check: space={has_space}, null={has_null}")
                
                for char_code, glyph_id in new_cmap.items():
                    char = chr(char_code) if 32 <= char_code <= 126 else f"U+{char_code:04X}"
                    print(f"[DEBUG] Kept mapping: {char} -> Glyph{glyph_id}")
        
        if 'glyf' in font:
            glyf_table = font['glyf']
            print(f"[DEBUG] Glyph table contains {len(glyf_table)} glyphs")
            
            if '.notdef' in glyf_table:
                print(f"[DEBUG] ✓ .notdef glyph exists")
            else:
                print(f"[DEBUG] ✗ .notdef glyph missing")
                
            glyph_names = list(glyf_table.keys())[:20]  
            print(f"[DEBUG] Glyph list (first 20): {glyph_names}")
        
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
            print(f"[DEBUG] Font Family: {font_family}")
        
        if 'OS/2' in font:
            os2_table = font['OS/2']
            print(f"[DEBUG] OS/2 Version: {os2_table.version}")
            print(f"[DEBUG] Weight Class: {os2_table.usWeightClass}")
        
        if 'maxp' in font:
            maxp_table = font['maxp']
            print(f"[DEBUG] Max Glyphs: {maxp_table.numGlyphs}")
            if hasattr(maxp_table, 'maxPoints'):
                print(f"[DEBUG] Max Points: {maxp_table.maxPoints}")
            if hasattr(maxp_table, 'maxContours'):
                print(f"[DEBUG] Max Contours: {maxp_table.maxContours}")
        
        output_io = io.BytesIO()
        print(f"[DEBUG] Saving font...")
        font.save(output_io)
        print(f"[DEBUG] Font saved")
        
        font.close()
        
        output_data = output_io.getvalue()
        print(f"[DEBUG] Generated font size: {len(output_data)} bytes")
        
        if len(output_data) < 100:
            raise Exception(f'Generated font file too small ({len(output_data)} bytes)')
        
        if len(output_data) >= 12:
            output_header = output_data[:12]
            header_hex = ' '.join(f'{b:02x}' for b in output_header)
            print(f"[DEBUG] Output font header: {header_hex}")
            
            signature = int.from_bytes(output_data[:4], 'big')
            if signature == 0x00010000:
                print("[DEBUG] Output file: Valid TTF format")
            elif signature == 0x4F54544F:
                print("[DEBUG] Output file: Valid OTF format")
            else:
                print(f"[DEBUG] Output file: Abnormal format 0x{signature:08x}")
        
        try:
            print(f"[DEBUG] Verifying generated font...")
            verify_io = io.BytesIO(output_data)
            verify_font = TTFont(verify_io)
            verify_cmap = verify_font.getBestCmap()
            print(f"[DEBUG] Verification success! Generated font contains {len(verify_cmap) if verify_cmap else 0} char mappings")
            
            verify_glyf = verify_font.get('glyf')
            if verify_glyf:
                print(f"[DEBUG] Glyph table contains {len(verify_glyf)} glyphs")
            
            verify_font.close()
        except Exception as verify_error:
            print(f"[ERROR] Generated font verification failed: {verify_error}")
            import traceback
            print(f"[ERROR] Verification error detail: {traceback.format_exc()}")
            
        print(f"[INFO] === Strict Cleanup Complete ===")
        print(f"[INFO] Mode: Strict Subsetting + Thorough Composite Glyph Cleanup")
        print(f"[INFO] Options: Removed GSUB/GPOS tables, stripped composite glyph info")
        print(f"[INFO] Input Chars: {repr(characters_to_keep)}")
        print(f"[INFO] Output Size: {len(output_data)} bytes")
        print(f"[INFO] Thoroughly cleaned extra chars and composite glyphs")
        print(f"[INFO] =====================================")
        
        result_base64 = base64.b64encode(output_data).decode('utf-8')
        print(f"[DEBUG] Base64 encoding complete, length: {len(result_base64)} chars")
        
        return {
            'success': True,
            'data': result_base64,
            'size': len(output_data),
            'message': f'Strict cleanup complete, kept {len(characters_to_keep)} specified characters'
        }
        
    except Exception as e:
        import traceback
        error_detail = traceback.format_exc()
        print(f"[ERROR] Processing failed: {str(e)}")
        print(f"[ERROR] Detailed error: {error_detail}")
        return {
            'success': False,
            'error': str(e),
            'error_detail': error_detail,
            'message': f'Processing failed: {str(e)}'
        }

def test_fonttools():
    return "FontTools library ready"
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
    f"subset_font defined, params: {list(sig.parameters.keys())}"
else:
    "ERROR: subset_font not defined"
        `);
        console.log(`🔧 ${function_test}`);
        
    } catch (testError) {
        console.error(`❌ Processing environment test failed: ${testError.message}`, testError);
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
            console.log('📋 Fallback font library loaded, limited functionality.');
        };
        script.onerror = () => {
            console.error('❌ Failed to load any font library.');
        };
        document.head.appendChild(script);
    } catch (error) {
        console.error('❌ Fallback load failed.', error);
    }
}

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
                const relativePath = dirPath.replace(new RegExp(`^${folderStructure.name}/?`), '');
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
                const flattenedPath = relativePath.replace(new RegExp(`^${folderStructure.name}/?`), '');
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
        msgEl.classList.remove('show');
        setTimeout(() => {
            msgEl.remove();
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
    console.error(`Error occurred: ${e.message}`);
});

window.addEventListener('unhandledrejection', function(e) {
    console.error(`Promise error: ${e.reason}`);
    e.preventDefault();
});