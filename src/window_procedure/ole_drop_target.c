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
            _wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".jpg") == 0 ||
            _wcsicmp(ext, L".ico") == 0);
}

static void ScanDirectoryForResources(const wchar_t* dirPath, int* fontCount, int* animCount, 
                                    wchar_t* lastFontPath, wchar_t* lastAnimPath) {
    WIN32_FIND_DATAW findData;
    wchar_t searchPath[MAX_PATH];
    
    swprintf_s(searchPath, MAX_PATH, L"%s\\*", dirPath);
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }
        
        wchar_t fullPath[MAX_PATH];
        swprintf_s(fullPath, MAX_PATH, L"%s\\%s", dirPath, findData.cFileName);
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ScanDirectoryForResources(fullPath, fontCount, animCount, lastFontPath, lastAnimPath);
        } else {
            if (IsFontFile(fullPath)) {
                if (*fontCount == 0 && lastFontPath) wcscpy_s(lastFontPath, MAX_PATH, fullPath);
                (*fontCount)++;
            } else if (IsAnimationFile(fullPath)) {
                if (*animCount == 0 && lastAnimPath) wcscpy_s(lastAnimPath, MAX_PATH, fullPath);
                (*animCount)++;
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

static void RestoreOriginalState(OleDropTarget* target, BOOL reloadOriginal) {
    if (target->isPreviewingFont) {
        if (reloadOriginal) {
            CancelFontPreview();
            LOG_INFO("Restored original font");
        } else {
            /* Unload preview font but skip reloading original 
             * (because we expect HandleDropFiles to load a new one immediately)
             */
            UnloadCurrentFontResource();
            IS_PREVIEWING = FALSE;
            PREVIEW_FONT_NAME[0] = '\0';
            PREVIEW_INTERNAL_NAME[0] = '\0';
            LOG_INFO("Unloaded preview font (skipping reload for drop)");
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
        LOG_INFO("Restored original animation");
    }
    
    if ((target->isPreviewingFont || target->isPreviewingAnim) && reloadOriginal) {
        /* Force repaint if we restored anything */
        InvalidateRect(target->hwnd, NULL, TRUE);
    }
}

static void StartPreview(OleDropTarget* target, const wchar_t* filePath) {
    char pathUtf8[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, pathUtf8, MAX_PATH, NULL, NULL);

    if (!target->isPreviewingFont && IsFontFile(filePath)) {
        /* Preview font */
        if (PreviewFont(GetModuleHandle(NULL), pathUtf8)) {
            target->isPreviewingFont = TRUE;
            InvalidateRect(target->hwnd, NULL, TRUE);
            LOG_INFO("Previewing font: %s", pathUtf8);
        }
    } 
    else if (!target->isPreviewingAnim && IsAnimationFile(filePath)) {
        /* Preview animation */
        extern void PreviewAnimationFromFile(HWND hwnd, const char* filePath);
        PreviewAnimationFromFile(target->hwnd, pathUtf8);
        target->isPreviewingAnim = TRUE;
        LOG_INFO("Previewing animation: %s", pathUtf8);
    }
}

/* ============================================================================
 * IUnknown Implementation
 * ============================================================================ */

STDMETHODIMP QueryInterface(IDropTarget* this, REFIID riid, void** ppvObject) {
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
        
        int fontCount = 0;
        int animCount = 0;
        wchar_t fontPath[MAX_PATH] = {0};
        wchar_t animPath[MAX_PATH] = {0};
        
        /* Count resources including those in subdirectories */
        for (UINT i = 0; i < count; i++) {
            wchar_t filePath[MAX_PATH];
            if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                DWORD attrs = GetFileAttributesW(filePath);
                if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    ScanDirectoryForResources(filePath, &fontCount, &animCount, fontPath, animPath);
                } else {
                    if (IsFontFile(filePath)) {
                        if (fontCount == 0) wcscpy_s(fontPath, MAX_PATH, filePath);
                        fontCount++;
                    } else if (IsAnimationFile(filePath)) {
                        if (animCount == 0) wcscpy_s(animPath, MAX_PATH, filePath);
                        animCount++;
                    }
                }
            }
        }
        
        /* Second pass: Apply previews only if unambiguous (count == 1) */
        if (fontCount == 1) {
            StartPreview(target, fontPath);
        }
        
        if (animCount == 1) {
            StartPreview(target, animPath);
        }
        
        /* Update validity flag for DragOver */
        target->isValidDrop = (fontCount > 0 || animCount > 0);
        
        ReleaseStgMedium(&stg);
        
        if (target->isValidDrop) {
             *pdwEffect = DROPEFFECT_COPY;
        } else {
             *pdwEffect = DROPEFFECT_MOVE;
        }
    } else {
        target->isValidDrop = FALSE;
        *pdwEffect = DROPEFFECT_NONE;
    }
    return S_OK;
}

STDMETHODIMP DragOver(IDropTarget* this, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    (void)grfKeyState; (void)pt;
    OleDropTarget* target = (OleDropTarget*)this;
    
    if (target->isValidDrop) {
        *pdwEffect = DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_MOVE;
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
    
    /* Determine if we will likely apply a font (skip reload) */
    BOOL willApplyFont = FALSE;
    FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg;
    
    /* Peek at data to count fonts */
    if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stg) == S_OK) {
        HDROP hDrop = (HDROP)stg.hGlobal;
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        int fontCount = 0;
        int animCount = 0; /* Unused here but required for helper */
        
        for (UINT i = 0; i < count; i++) {
            wchar_t filePath[MAX_PATH];
            if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                DWORD attrs = GetFileAttributesW(filePath);
                if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    /* Pass NULL for paths as we only care about counts here */
                    ScanDirectoryForResources(filePath, &fontCount, &animCount, NULL, NULL);
                } else {
                    if (IsFontFile(filePath)) fontCount++;
                }
            }
        }
        if (fontCount == 1) willApplyFont = TRUE;
        ReleaseStgMedium(&stg);
    }

    /* Restore state (skip reload if we are about to apply new font) */
    RestoreOriginalState(target, !willApplyFont);
    
    /* Process Drop */
    if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stg) == S_OK) {
        HDROP hDrop = (HDROP)stg.hGlobal;
        HandleDropFiles(target->hwnd, hDrop);
        ReleaseStgMedium(&stg);
        *pdwEffect = DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }
    
    return S_OK;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void InitializeOleDropTarget(HWND hwnd) {
    if (OleInitialize(NULL) != S_OK) {
        LOG_ERROR("OleInitialize failed");
        return;
    }

    g_dropTarget.lpVtbl = &vtbl;
    g_dropTarget.refCount = 1;
    g_dropTarget.hwnd = hwnd;
    g_dropTarget.isPreviewingFont = FALSE;
    g_dropTarget.isPreviewingAnim = FALSE;
    g_dropTarget.isValidDrop = FALSE;

    if (RegisterDragDrop(hwnd, (IDropTarget*)&g_dropTarget) != S_OK) {
        LOG_ERROR("RegisterDragDrop failed");
    } else {
        LOG_INFO("OLE Drag & Drop registered successfully");
    }
}

void CleanupOleDropTarget(HWND hwnd) {
    RevokeDragDrop(hwnd);
    OleUninitialize();
}
