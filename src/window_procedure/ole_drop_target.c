/**
 * @file ole_drop_target.c
 * @brief OLE Drag and Drop implementation with live preview
 */

#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include "window_procedure/ole_drop_target.h"
#include "window_procedure/window_drop_target.h" /* Reuse existing helpers */
#include "window_procedure/window_commands.h"
#include "font/font_manager.h"
#include "tray/tray_animation_core.h"
#include "config.h"
#include "log.h"
#include "font.h"

/* Reuse helpers from window_drop_target.c by declaring them here 
 * Note: In a cleaner refactor, these should be in a shared header/source
 */
/* We will reimplement necessary logic or expose them if possible. 
 * For now, I'll reimplement the resource detection logic to keep it self-contained or 
 * better yet, I'll assume window_drop_target.c logic is still useful for the final "Apply" step,
 * but for preview we need custom logic.
 */

typedef struct {
    IDropTargetVtbl* lpVtbl;
    LONG refCount;
    HWND hwnd;
    BOOL isPreviewingFont;
    BOOL isPreviewingAnim;
    BOOL isValidDrop;
} OleDropTarget;

/* Forward declarations */
STDMETHODIMP QueryInterface(IDropTarget* this, REFIID riid, void** ppvObject);
STDMETHODIMP_(ULONG) AddRef(IDropTarget* this);
STDMETHODIMP_(ULONG) Release(IDropTarget* this);
STDMETHODIMP DragEnter(IDropTarget* this, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
STDMETHODIMP DragOver(IDropTarget* this, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);
STDMETHODIMP DragLeave(IDropTarget* this);
STDMETHODIMP Drop(IDropTarget* this, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect);

static IDropTargetVtbl vtbl = {
    QueryInterface,
    AddRef,
    Release,
    DragEnter,
    DragOver,
    DragLeave,
    Drop
};

static OleDropTarget g_dropTarget;
static BOOL g_oleInitialized = FALSE;
static BOOL g_dropTargetRegistered = FALSE;
static HWND g_registeredDropTargetHwnd = NULL;

#define DROP_PREVIEW_SCAN_ENTRY_BUDGET 512u
#define DROP_PREVIEW_SCAN_DEPTH_LIMIT 8u

typedef struct {
    int fontCount;
    int animCount;
    wchar_t fontPath[MAX_PATH];
    wchar_t animPath[MAX_PATH];
    DWORD scannedEntries;
    BOOL truncated;
} ResourceScanResult;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static BOOL IsFontFile(const wchar_t* filename) {
    const wchar_t* ext = wcsrchr(filename, L'.');
    if (!ext) return FALSE;
    return (_wcsicmp(ext, L".ttf") == 0 || _wcsicmp(ext, L".otf") == 0 || _wcsicmp(ext, L".ttc") == 0);
}

static BOOL IsAnimationFile(const wchar_t* filename) {
    const wchar_t* ext = wcsrchr(filename, L'.');
    if (!ext) return FALSE;
    return (_wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".webp") == 0 ||
            _wcsicmp(ext, L".ani") == 0 ||
            _wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 ||
            _wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 ||
            _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".tif") == 0 ||
            _wcsicmp(ext, L".tiff") == 0);
}

static BOOL QueryDropFilePathExactW(HDROP hDrop, UINT index,
                                    wchar_t* outPath, UINT outPathCount) {
    if (!hDrop || !outPath || outPathCount == 0) return FALSE;
    outPath[0] = L'\0';

    UINT pathLen = DragQueryFileW(hDrop, index, NULL, 0);
    if (pathLen == 0 || pathLen >= outPathCount) {
        return FALSE;
    }

    UINT copied = DragQueryFileW(hDrop, index, outPath, outPathCount);
    if (copied != pathLen) {
        outPath[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}

static BOOL IsResourceScanResolved(const ResourceScanResult* scan) {
    return scan->truncated || (scan->fontCount > 1 && scan->animCount > 1);
}

static void RecordScannedResource(ResourceScanResult* scan, const wchar_t* filePath) {
    if (IsFontFile(filePath)) {
        if (scan->fontCount == 0) {
            wcscpy_s(scan->fontPath, MAX_PATH, filePath);
        }
        if (scan->fontCount < 2) scan->fontCount++;
    } else if (IsAnimationFile(filePath)) {
        if (scan->animCount == 0) {
            wcscpy_s(scan->animPath, MAX_PATH, filePath);
        }
        if (scan->animCount < 2) scan->animCount++;
    }
}

static void ScanDirectoryForResourcesLimited(const wchar_t* dirPath, ResourceScanResult* scan, unsigned depth) {
    if (!dirPath || !scan || IsResourceScanResolved(scan)) return;
    if (depth >= DROP_PREVIEW_SCAN_DEPTH_LIMIT) {
        scan->truncated = TRUE;
        return;
    }

    WIN32_FIND_DATAW findData;
    wchar_t searchPath[MAX_PATH];

    if (_snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", dirPath) < 0) {
        scan->truncated = TRUE;
        return;
    }

    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        if (scan->scannedEntries >= DROP_PREVIEW_SCAN_ENTRY_BUDGET) {
            scan->truncated = TRUE;
            break;
        }
        scan->scannedEntries++;

        wchar_t fullPath[MAX_PATH];
        if (_snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s", dirPath, findData.cFileName) < 0) {
            scan->truncated = TRUE;
            break;
        }

        BOOL isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (isDirectory) {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                ScanDirectoryForResourcesLimited(fullPath, scan, depth + 1);
            }
        } else {
            RecordScannedResource(scan, fullPath);
        }
    } while (!IsResourceScanResolved(scan) && FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

static void ScanDropPathForResources(const wchar_t* filePath, ResourceScanResult* scan) {
    if (!filePath || !scan || IsResourceScanResolved(scan)) return;

    DWORD attrs = GetFileAttributesW(filePath);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        ScanDirectoryForResourcesLimited(filePath, scan, 0);
    } else {
        RecordScannedResource(scan, filePath);
    }
}

static void RestoreOriginalState(OleDropTarget* target, BOOL reloadOriginal) {
    BOOL restoredPreview = target->isPreviewingFont || target->isPreviewingAnim;

    if (target->isPreviewingFont) {
        if (reloadOriginal) {
            CancelFontPreview();
        } else {
            /* Unload preview font but skip reloading original
             * (because we expect HandleDropFiles to load a new one immediately)
             */
            UnloadCurrentFontResource();
            IS_PREVIEWING = FALSE;
            PREVIEW_FONT_NAME[0] = '\0';
            PREVIEW_INTERNAL_NAME[0] = '\0';
        }
        target->isPreviewingFont = FALSE;
    }
    
    if (target->isPreviewingAnim) {
        /* Animations are always managed by tray_animation_core, 
         * which handles path switching gracefully. 
         * If we are applying, SetCurrentAnimationName will be called.
         * So we should probably cancel preview to restore original state 
         * ONLY if we are NOT applying?
         * But SetCurrentAnimationName overwrites anyway.
         * Let's just cancel to be safe/clean.
        */
        CancelAnimationPreview();
        target->isPreviewingAnim = FALSE;
    }

    if (restoredPreview && reloadOriginal) {
        /* Force repaint if we restored anything */
        RefreshCustomTextDisplayDialogFont();
        InvalidateRect(target->hwnd, NULL, TRUE);
    }
}

static void StartPreview(OleDropTarget* target, const wchar_t* filePath) {
    char pathUtf8[MAX_PATH];
    if (!WideCharToMultiByte(CP_UTF8, 0, filePath, -1, pathUtf8, MAX_PATH, NULL, NULL)) {
        return;
    }

    if (!target->isPreviewingFont && IsFontFile(filePath)) {
        /* Preview font */
        if (PreviewFont(GetModuleHandle(NULL), pathUtf8)) {
            target->isPreviewingFont = TRUE;
            RefreshCustomTextDisplayDialogFont();
            InvalidateRect(target->hwnd, NULL, TRUE);
        }
    } 
    else if (!target->isPreviewingAnim && IsAnimationFile(filePath)) {
        /* Preview animation */
        if (PreviewAnimationFromFile(target->hwnd, pathUtf8)) {
            target->isPreviewingAnim = TRUE;
        }
    }
}

static void ClearFontPreviewStateAfterDropApply(void) {
    IS_PREVIEWING = FALSE;
    PREVIEW_FONT_NAME[0] = '\0';
    PREVIEW_INTERNAL_NAME[0] = '\0';
}

static void ReleasePreviewResourcesForDrop(OleDropTarget* target) {
    if (!target) return;

    if (target->isPreviewingFont) {
        /*
         * Release the preview font before importing. Keep IS_PREVIEWING intact
         * so CancelFontPreview can restore the original font if the drop fails.
         */
        UnloadCurrentFontResource();
        target->isPreviewingFont = FALSE;
    }

    if (target->isPreviewingAnim) {
        CancelAnimationPreview();
        target->isPreviewingAnim = FALSE;
    }
}

/* ============================================================================
 * IUnknown Implementation
 * ============================================================================ */

STDMETHODIMP QueryInterface(IDropTarget* this, REFIID riid, void** ppvObject) {
    if (!ppvObject) {
        return E_POINTER;
    }

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) {
        *ppvObject = this;
        AddRef(this);
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) AddRef(IDropTarget* this) {
    return InterlockedIncrement(&((OleDropTarget*)this)->refCount);
}

STDMETHODIMP_(ULONG) Release(IDropTarget* this) {
    ULONG count = InterlockedDecrement(&((OleDropTarget*)this)->refCount);
    return count;
}

/* ============================================================================
 * IDropTarget Implementation
 * ============================================================================ */

STDMETHODIMP DragEnter(IDropTarget* this, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    (void)grfKeyState; (void)pt;
    OleDropTarget* target = (OleDropTarget*)this;
    
    FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg;

    if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stg) == S_OK) {
        HDROP hDrop = (HDROP)stg.hGlobal;
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        
        ResourceScanResult scan = {0};

        /* Preview scanning runs on the drag UI path, so keep it bounded. */
        for (UINT i = 0; i < count; i++) {
            wchar_t filePath[MAX_PATH];
            if (!QueryDropFilePathExactW(hDrop, i, filePath, MAX_PATH)) {
                scan.truncated = TRUE;
                break;
            }
            ScanDropPathForResources(filePath, &scan);
            if (IsResourceScanResolved(&scan)) break;
        }

        /* Apply previews only when the bounded scan proved the drop is unambiguous. */
        if (!scan.truncated && scan.fontCount == 1) {
            StartPreview(target, scan.fontPath);
        }

        if (!scan.truncated && scan.animCount == 1) {
            StartPreview(target, scan.animPath);
        }

        if (scan.truncated) {
            LOG_DEBUG("Drag preview resource scan truncated after %lu entries", scan.scannedEntries);
        }

        /* If the preview scan was truncated, still allow dropping; Drop does the full import. */
        target->isValidDrop = (scan.truncated || scan.fontCount > 0 || scan.animCount > 0);
        
        ReleaseStgMedium(&stg);
        
        if (target->isValidDrop) {
             *pdwEffect = DROPEFFECT_COPY;
        } else {
             *pdwEffect = DROPEFFECT_NONE;
        }
    } else {
        target->isValidDrop = FALSE;
        *pdwEffect = DROPEFFECT_NONE;
    }
    return S_OK;
}

STDMETHODIMP DragOver(IDropTarget* this, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    (void)grfKeyState; (void)pt;
    const OleDropTarget* target = (const OleDropTarget*)this;
    
    if (target->isValidDrop) {
        *pdwEffect = DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }
    
    return S_OK;
}

STDMETHODIMP DragLeave(IDropTarget* this) {
    OleDropTarget* target = (OleDropTarget*)this;
    RestoreOriginalState(target, TRUE);
    return S_OK;
}

STDMETHODIMP Drop(IDropTarget* this, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    (void)grfKeyState; (void)pt;
    OleDropTarget* target = (OleDropTarget*)this;
    BOOL hadFontPreview = target->isPreviewingFont;
    FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg;

    if (!target->isValidDrop) {
        RestoreOriginalState(target, TRUE);
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    /* Release preview-owned resources before moving dropped files. */
    ReleasePreviewResourcesForDrop(target);
    target->isValidDrop = FALSE;

    /* Process Drop */
    if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stg) == S_OK) {
        HDROP hDrop = (HDROP)stg.hGlobal;
        DropImportResult result = HandleDropFiles(target->hwnd, hDrop);
        ReleaseStgMedium(&stg);
        if (hadFontPreview) {
            if (result.fontApplied) {
                ClearFontPreviewStateAfterDropApply();
            } else {
                CancelFontPreview();
                RefreshCustomTextDisplayDialogFont();
                InvalidateRect(target->hwnd, NULL, TRUE);
            }
        }
        *pdwEffect = (result.movedCount > 0 || result.fontApplied || result.animationApplied)
            ? DROPEFFECT_COPY
            : DROPEFFECT_NONE;
    } else {
        if (hadFontPreview) {
            CancelFontPreview();
            RefreshCustomTextDisplayDialogFont();
            InvalidateRect(target->hwnd, NULL, TRUE);
        }
        *pdwEffect = DROPEFFECT_NONE;
    }

    return S_OK;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void InitializeOleDropTarget(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG_WARNING("InitializeOleDropTarget called with invalid window handle");
        return;
    }

    if (g_dropTargetRegistered) {
        if (g_registeredDropTargetHwnd == hwnd) {
            return;
        }

        CleanupOleDropTarget(g_registeredDropTargetHwnd);
    } else if (g_oleInitialized) {
        OleUninitialize();
        g_oleInitialized = FALSE;
    }

    HRESULT hr = OleInitialize(NULL);
    if (FAILED(hr)) {
        LOG_ERROR("OleInitialize failed (hr=0x%08lX)", (unsigned long)hr);
        return;
    }
    g_oleInitialized = TRUE;

    g_dropTarget.lpVtbl = &vtbl;
    g_dropTarget.refCount = 1;
    g_dropTarget.hwnd = hwnd;
    g_dropTarget.isPreviewingFont = FALSE;
    g_dropTarget.isPreviewingAnim = FALSE;
    g_dropTarget.isValidDrop = FALSE;

    hr = RegisterDragDrop(hwnd, (IDropTarget*)&g_dropTarget);
    if (FAILED(hr)) {
        LOG_ERROR("RegisterDragDrop failed (hr=0x%08lX)", (unsigned long)hr);
        OleUninitialize();
        g_oleInitialized = FALSE;
        g_registeredDropTargetHwnd = NULL;
        return;
    }

    g_dropTargetRegistered = TRUE;
    g_registeredDropTargetHwnd = hwnd;
}

void CleanupOleDropTarget(HWND hwnd) {
    if (g_dropTargetRegistered) {
        HWND registeredHwnd = g_registeredDropTargetHwnd;
        if (!registeredHwnd) {
            registeredHwnd = hwnd;
        }

        if (!hwnd || hwnd == registeredHwnd) {
            RestoreOriginalState(&g_dropTarget, TRUE);
            g_dropTarget.isValidDrop = FALSE;

            HRESULT hr = RevokeDragDrop(registeredHwnd);
            if (FAILED(hr)) {
                LOG_WARNING("RevokeDragDrop failed (hr=0x%08lX)", (unsigned long)hr);
            }
        } else {
            LOG_WARNING("CleanupOleDropTarget called for non-registered window");
            return;
        }

        g_dropTargetRegistered = FALSE;
        g_registeredDropTargetHwnd = NULL;
    }

    if (g_oleInitialized) {
        OleUninitialize();
        g_oleInitialized = FALSE;
    }
}
