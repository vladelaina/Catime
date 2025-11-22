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
    BOOL isPreviewing;
    char originalFontName[MAX_PATH];
    char originalAnimationName[MAX_PATH];
    char currentPreviewPath[MAX_PATH];
    int resourceType; /* 1=Font, 2=Animation */
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

static void RestoreOriginalState(OleDropTarget* target) {
    if (!target->isPreviewing) return;

    if (target->resourceType == 1) { /* Font */
        CancelFontPreview();
        LOG_INFO("Restored original font");
    } else if (target->resourceType == 2) { /* Animation */
        CancelAnimationPreview();
        LOG_INFO("Restored original animation");
    }

    target->isPreviewing = FALSE;
    target->resourceType = 0;
}

static void StartPreview(OleDropTarget* target, const wchar_t* filePath) {
    if (target->isPreviewing) return;

    char pathUtf8[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, pathUtf8, MAX_PATH, NULL, NULL);

    if (IsFontFile(filePath)) {
        target->resourceType = 1;
        /* Preview font */
        if (PreviewFont(GetModuleHandle(NULL), pathUtf8)) {
            target->isPreviewing = TRUE;
            InvalidateRect(target->hwnd, NULL, TRUE);
            LOG_INFO("Previewing font: %s", pathUtf8);
        }
    } else if (IsAnimationFile(filePath)) {
        target->resourceType = 2;
        /* Preview animation */
        extern void PreviewAnimationFromFile(HWND hwnd, const char* filePath);
        PreviewAnimationFromFile(target->hwnd, pathUtf8);
        target->isPreviewing = TRUE;
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
        
        if (count == 1) {
            wchar_t filePath[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, filePath, MAX_PATH)) {
                *pdwEffect = DROPEFFECT_COPY;
                
                /* Start preview if single file */
                StartPreview(target, filePath);
            }
        } else {
            *pdwEffect = DROPEFFECT_COPY; /* Allow multi-drop without preview */
        }
        
        ReleaseStgMedium(&stg);
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }
    return S_OK;
}

STDMETHODIMP DragOver(IDropTarget* this, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    (void)this; (void)grfKeyState; (void)pt;
    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}

STDMETHODIMP DragLeave(IDropTarget* this) {
    OleDropTarget* target = (OleDropTarget*)this;
    RestoreOriginalState(target);
    return S_OK;
}

STDMETHODIMP Drop(IDropTarget* this, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    (void)grfKeyState; (void)pt;
    OleDropTarget* target = (OleDropTarget*)this;
    
    /* If we were previewing, we want to apply. 
     * The previewed state is technically already "active" visually,
     * but we need to finalize it (move file, save config).
     * 
     * However, HandleDropFiles logic does the move and apply.
     * If we call HandleDropFiles now, it might flicker or reload.
     * 
     * Strategy:
     * 1. If previewing, ApplyFontPreview() or equivalent is needed.
     * 2. Move the file to resources.
     * 3. Update config.
     * 
     * OR simply call HandleDropFiles and let it handle everything,
     * but we must ensure HandleDropFiles knows to "keep" the current look 
     * or just re-apply it cleanly.
     */
    
    /* Restore first to clear "preview" flags, so HandleDropFiles can do a clean apply */
    /* Actually, for font preview, ApplyFontPreview in font_manager.c logic 
     * assumes we want to commit the previewed font.
     * But HandleDropFiles moves the file first.
     */
    
    RestoreOriginalState(target); /* Revert to clean state first to avoid conflicts */
    
    FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg;

    if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stg) == S_OK) {
        HDROP hDrop = (HDROP)stg.hGlobal;
        
        /* Reuse existing logic to move files and configure */
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
    g_dropTarget.isPreviewing = FALSE;
    g_dropTarget.resourceType = 0;

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
