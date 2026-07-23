#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include "window_procedure/ole_drop_target.h"
#include "window_procedure/window_drop_target.h" /* Reuse existing helpers */
#include "window_procedure/window_commands.h"
#include "ole_drop_scan.h"
#include "font/font_manager.h"
#include "tray/tray_animation_core.h"
#include "config.h"
#include "log.h"
#include "font.h"
typedef struct {
    IDropTargetVtbl* lpVtbl;
    LONG refCount;
    HWND hwnd;
    BOOL isPreviewingFont;
    BOOL isPreviewingAnim;
    BOOL isValidDrop;
} OleDropTarget;
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
static void RestoreOriginalState(OleDropTarget* target, BOOL reloadOriginal) {
    BOOL restoredPreview = target->isPreviewingFont || target->isPreviewingAnim;
    if (target->isPreviewingFont) {
        if (reloadOriginal) {
            CancelFontPreview();
        } else {
            UnloadCurrentFontResource();
            IS_PREVIEWING = FALSE;
            PREVIEW_FONT_NAME[0] = '\0';
            PREVIEW_INTERNAL_NAME[0] = '\0';
        }
        target->isPreviewingFont = FALSE;
    }
    if (target->isPreviewingAnim) {
        CancelAnimationPreview();
        target->isPreviewingAnim = FALSE;
    }
    if (restoredPreview && reloadOriginal) {
        RefreshCustomTextDisplayDialogFont();
        InvalidateRect(target->hwnd, NULL, TRUE);
    }
}
static void StartPreview(OleDropTarget* target, const wchar_t* filePath) {
    char pathUtf8[MAX_PATH];
    if (!WideCharToMultiByte(CP_UTF8, 0, filePath, -1, pathUtf8, MAX_PATH, NULL, NULL)) {
        return;
    }
    if (!target->isPreviewingFont && OleDrop_IsFontFile(filePath)) {
        if (PreviewFont(GetModuleHandle(NULL), pathUtf8)) {
            target->isPreviewingFont = TRUE;
            RefreshCustomTextDisplayDialogFont();
            InvalidateRect(target->hwnd, NULL, TRUE);
        }
    }
    else if (!target->isPreviewingAnim && OleDrop_IsAnimationFile(filePath)) {
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
        UnloadCurrentFontResource();
        target->isPreviewingFont = FALSE;
    }
    if (target->isPreviewingAnim) {
        CancelAnimationPreview();
        target->isPreviewingAnim = FALSE;
    }
}
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
STDMETHODIMP DragEnter(IDropTarget* this, IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) {
    (void)grfKeyState; (void)pt;
    OleDropTarget* target = (OleDropTarget*)this;
    FORMATETC fmt = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM stg;
    if (pDataObj->lpVtbl->GetData(pDataObj, &fmt, &stg) == S_OK) {
        HDROP hDrop = (HDROP)stg.hGlobal;
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
        ResourceScanResult scan = {0};
        for (UINT i = 0; i < count; i++) {
            wchar_t filePath[MAX_PATH];
            if (!OleDrop_QueryFilePathExactW(hDrop, i, filePath, MAX_PATH)) {
                scan.truncated = TRUE;
                break;
            }
            OleDrop_ScanPathForResources(filePath, &scan);
            if (OleDrop_IsResourceScanResolved(&scan)) break;
        }
        if (!scan.truncated && scan.fontCount == 1) {
            StartPreview(target, scan.fontPath);
        }
        if (!scan.truncated && scan.animCount == 1) {
            StartPreview(target, scan.animPath);
        }
        if (scan.truncated) {
            LOG_DEBUG("Drag preview resource scan truncated after %lu entries", scan.scannedEntries);
        }
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
    ReleasePreviewResourcesForDrop(target);
    target->isValidDrop = FALSE;
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
