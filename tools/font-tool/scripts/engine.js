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

function escapeHTML(value) {
    const div = document.createElement('div');
    div.textContent = value == null ? '' : String(value);
    return div.innerHTML;
}

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

    initFileScrollProgress();
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

        if (typeof loadPyodide === 'undefined') {
            await loadExternalScriptOnce('https://cdn.jsdelivr.net/pyodide/v0.24.1/full/pyodide.js', 'pyodide');
        }
        
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

