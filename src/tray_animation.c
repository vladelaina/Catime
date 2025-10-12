/**
 * @file tray_animation.c
 * @brief RunCat-like tray icon animation implementation
 */

#include <windows.h> 
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <wctype.h>
#include <wincodec.h>
#include <propvarutil.h>
#include <objbase.h>

#include "../include/tray.h"
#include "../include/config.h"
#include "../include/tray_menu.h"
/** Forward declaration for timer callback used by SetTimer */
static void CALLBACK TrayAnimTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

/**
 * @brief Timer ID for tray animation
 */
#define TRAY_ANIM_TIMER_ID 42420

/** @brief Max frames supported */
#define MAX_TRAY_FRAMES 64

/** @brief Loaded icon frames and state */
static HICON g_trayIcons[MAX_TRAY_FRAMES];
static int g_trayIconCount = 0;
static int g_trayIconIndex = 0;
static UINT g_trayInterval = 0;
static HWND g_trayHwnd = NULL;
static char g_animationName[64] = "cat"; /** current folder under animations */
static BOOL g_isPreviewActive = FALSE; /** preview mode flag */
static HICON g_previewIcons[MAX_TRAY_FRAMES];
static int g_previewCount = 0;
static int g_previewIndex = 0;
static BOOL g_isGifAnimation = FALSE; /** whether current animation source is a single GIF file */
static UINT g_frameDelaysMs[MAX_TRAY_FRAMES]; /** per-frame delay in ms for GIF */
static BOOL g_isPreviewGif = FALSE; /** whether current preview source is a single GIF file */
static UINT g_previewFrameDelaysMs[MAX_TRAY_FRAMES]; /** per-frame delay for GIF preview */

/** @brief Build cat animation folder path: %LOCALAPPDATA%\Catime\resources\animations\cat */
static void BuildAnimationFolder(const char* name, char* path, size_t size) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    size_t len = strlen(base);
    if (len > 0 && (base[len-1] == '/' || base[len-1] == '\\')) {
        snprintf(path, size, "%s%s", base, name);
    } else {
        snprintf(path, size, "%s\\%s", base, name);
    }
}

/** @brief Free all loaded icon frames */
static void FreeTrayIcons(void) {
    for (int i = 0; i < g_trayIconCount; ++i) {
        if (g_trayIcons[i]) {
            DestroyIcon(g_trayIcons[i]);
            g_trayIcons[i] = NULL;
        }
    }
    g_trayIconCount = 0;
    g_trayIconIndex = 0;
    g_isGifAnimation = FALSE;
}

/** @brief Free preview icon frames */
static void FreePreviewIcons(void) {
    for (int i = 0; i < g_previewCount; ++i) {
        if (g_previewIcons[i]) {
            DestroyIcon(g_previewIcons[i]);
            g_previewIcons[i] = NULL;
        }
    }
    g_previewCount = 0;
    g_previewIndex = 0;
    g_isPreviewGif = FALSE;
}

/** @brief Case-insensitive string ends-with helper */
static BOOL EndsWithIgnoreCase(const char* str, const char* suffix) {
    if (!str || !suffix) return FALSE;
    size_t ls = strlen(str), lsuf = strlen(suffix);
    if (lsuf > ls) return FALSE;
    return _stricmp(str + (ls - lsuf), suffix) == 0;
}

/** @brief Detect if current animation name is a single GIF file */
static BOOL IsGifSelection(const char* name) {
    return name && EndsWithIgnoreCase(name, ".gif");
}

/** @brief Decode an animated GIF into HICON frames with per-frame delays */
static void LoadTrayIconsFromGifPath(const char* utf8Path) {
    FreeTrayIcons();
    if (!utf8Path || !*utf8Path) return;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    IWICBitmapDecoder* pDecoder = NULL;
    hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, wPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (SUCCEEDED(hr) && pDecoder) {
        UINT frameCount = 0;
        if (SUCCEEDED(pDecoder->lpVtbl->GetFrameCount(pDecoder, &frameCount))) {
            for (UINT i = 0; i < frameCount && g_trayIconCount < MAX_TRAY_FRAMES; ++i) {
                IWICBitmapFrameDecode* pFrame = NULL;
                if (FAILED(pDecoder->lpVtbl->GetFrame(pDecoder, i, &pFrame)) || !pFrame) continue;

                /** Read per-frame delay (in 1/100s) from /grctlext/Delay */
                UINT delayMs = 100; /** default 100ms */
                IWICMetadataQueryReader* pMeta = NULL;
                if (SUCCEEDED(pFrame->lpVtbl->GetMetadataQueryReader(pFrame, &pMeta)) && pMeta) {
                    PROPVARIANT var; PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/grctlext/Delay", &var))) {
                        if (var.vt == VT_UI2 || var.vt == VT_I2) {
                            USHORT cs = (var.vt == VT_UI2) ? var.uiVal : (USHORT)var.iVal;
                            /** 0 means no delay, treat as 10cs = 100ms */
                            if (cs == 0) cs = 10;
                            delayMs = (UINT)cs * 10U;
                        } else if (var.vt == VT_UI1) {
                            UCHAR cs = var.bVal; if (cs == 0) cs = 10; delayMs = (UINT)cs * 10U;
                        }
                    }
                    PropVariantClear(&var);
                    pMeta->lpVtbl->Release(pMeta);
                }

                IWICBitmapScaler* pScaler = NULL;
                hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
                if (SUCCEEDED(hr) && pScaler) {
                    hr = pScaler->lpVtbl->Initialize(pScaler, (IWICBitmapSource*)pFrame, cx, cy, WICBitmapInterpolationModeFant);
                    if (SUCCEEDED(hr)) {
                        IWICFormatConverter* pConverter = NULL;
                        hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
                        if (SUCCEEDED(hr) && pConverter) {
                            hr = pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pScaler, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
                            if (SUCCEEDED(hr)) {
                                BITMAPINFO bi; ZeroMemory(&bi, sizeof(bi));
                                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                bi.bmiHeader.biWidth = cx;
                                bi.bmiHeader.biHeight = -cy;
                                bi.bmiHeader.biPlanes = 1;
                                bi.bmiHeader.biBitCount = 32;
                                bi.bmiHeader.biCompression = BI_RGB;
                                VOID* pvBits = NULL;
                                HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                                if (hbmColor && pvBits) {
                                    UINT stride = (UINT)(cx * 4);
                                    UINT bufSize = (UINT)(cy * stride);
                                    if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, stride, bufSize, (BYTE*)pvBits))) {
                                        ICONINFO ii; ZeroMemory(&ii, sizeof(ii));
                                        ii.fIcon = TRUE;
                                        ii.hbmColor = hbmColor;
                                        ii.hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
                                        HICON hIcon = CreateIconIndirect(&ii);
                                        if (ii.hbmMask) DeleteObject(ii.hbmMask);
                                        if (hIcon) {
                                            g_trayIcons[g_trayIconCount] = hIcon;
                                            g_frameDelaysMs[g_trayIconCount] = delayMs;
                                            g_trayIconCount++;
                                        }
                                    }
                                    DeleteObject(hbmColor);
                                }
                            }
                            pConverter->lpVtbl->Release(pConverter);
                        }
                    }
                    pScaler->lpVtbl->Release(pScaler);
                }

                pFrame->lpVtbl->Release(pFrame);
            }
        }
        pDecoder->lpVtbl->Release(pDecoder);
    }
    if (pFactory) pFactory->lpVtbl->Release(pFactory);
    if (SUCCEEDED(hrInit)) CoUninitialize();

    if (g_trayIconCount > 0) {
        g_isGifAnimation = TRUE;
        g_trayIconIndex = 0;
    }
}

/** @brief Decode an animated GIF into preview HICON frames with per-frame delays */
static void LoadPreviewIconsFromGifPath(const char* utf8Path) {
    FreePreviewIcons();
    if (!utf8Path || !*utf8Path) return;

    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, utf8Path, -1, wPath, MAX_PATH);

    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);

    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory);
    if (FAILED(hr) || !pFactory) {
        if (SUCCEEDED(hrInit)) CoUninitialize();
        return;
    }

    IWICBitmapDecoder* pDecoder = NULL;
    hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, wPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (SUCCEEDED(hr) && pDecoder) {
        UINT frameCount = 0;
        if (SUCCEEDED(pDecoder->lpVtbl->GetFrameCount(pDecoder, &frameCount))) {
            for (UINT i = 0; i < frameCount && g_previewCount < MAX_TRAY_FRAMES; ++i) {
                IWICBitmapFrameDecode* pFrame = NULL;
                if (FAILED(pDecoder->lpVtbl->GetFrame(pDecoder, i, &pFrame)) || !pFrame) continue;

                UINT delayMs = 100; /** default */
                IWICMetadataQueryReader* pMeta = NULL;
                if (SUCCEEDED(pFrame->lpVtbl->GetMetadataQueryReader(pFrame, &pMeta)) && pMeta) {
                    PROPVARIANT var; PropVariantInit(&var);
                    if (SUCCEEDED(pMeta->lpVtbl->GetMetadataByName(pMeta, L"/grctlext/Delay", &var))) {
                        if (var.vt == VT_UI2 || var.vt == VT_I2) {
                            USHORT cs = (var.vt == VT_UI2) ? var.uiVal : (USHORT)var.iVal;
                            if (cs == 0) cs = 10; delayMs = (UINT)cs * 10U;
                        } else if (var.vt == VT_UI1) {
                            UCHAR cs = var.bVal; if (cs == 0) cs = 10; delayMs = (UINT)cs * 10U;
                        }
                    }
                    PropVariantClear(&var);
                    pMeta->lpVtbl->Release(pMeta);
                }

                IWICBitmapScaler* pScaler = NULL;
                hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
                if (SUCCEEDED(hr) && pScaler) {
                    hr = pScaler->lpVtbl->Initialize(pScaler, (IWICBitmapSource*)pFrame, cx, cy, WICBitmapInterpolationModeFant);
                    if (SUCCEEDED(hr)) {
                        IWICFormatConverter* pConverter = NULL;
                        hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
                        if (SUCCEEDED(hr) && pConverter) {
                            hr = pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pScaler, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
                            if (SUCCEEDED(hr)) {
                                BITMAPINFO bi; ZeroMemory(&bi, sizeof(bi));
                                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                bi.bmiHeader.biWidth = cx;
                                bi.bmiHeader.biHeight = -cy;
                                bi.bmiHeader.biPlanes = 1;
                                bi.bmiHeader.biBitCount = 32;
                                bi.bmiHeader.biCompression = BI_RGB;
                                VOID* pvBits = NULL;
                                HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                                if (hbmColor && pvBits) {
                                    UINT stride = (UINT)(cx * 4);
                                    UINT bufSize = (UINT)(cy * stride);
                                    if (SUCCEEDED(pConverter->lpVtbl->CopyPixels(pConverter, NULL, stride, bufSize, (BYTE*)pvBits))) {
                                        ICONINFO ii; ZeroMemory(&ii, sizeof(ii));
                                        ii.fIcon = TRUE;
                                        ii.hbmColor = hbmColor;
                                        ii.hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
                                        HICON hIcon = CreateIconIndirect(&ii);
                                        if (ii.hbmMask) DeleteObject(ii.hbmMask);
                                        if (hIcon) {
                                            g_previewIcons[g_previewCount] = hIcon;
                                            g_previewFrameDelaysMs[g_previewCount] = delayMs;
                                            g_previewCount++;
                                        }
                                    }
                                    DeleteObject(hbmColor);
                                }
                            }
                            pConverter->lpVtbl->Release(pConverter);
                        }
                    }
                    pScaler->lpVtbl->Release(pScaler);
                }
                pFrame->lpVtbl->Release(pFrame);
            }
        }
        pDecoder->lpVtbl->Release(pDecoder);
    }
    if (pFactory) pFactory->lpVtbl->Release(pFactory);
    if (SUCCEEDED(hrInit)) CoUninitialize();

    if (g_previewCount > 0) {
        g_isPreviewGif = TRUE;
        g_previewIndex = 0;
    }
}

/** @brief Load sequential icon frames from .ico and .png files */
static void LoadTrayIcons(void) {
    FreeTrayIcons();

    char folder[MAX_PATH] = {0};
    BuildAnimationFolder(g_animationName, folder, sizeof(folder));

    /** If selection is a single GIF file under animations root, decode frames from it */
    if (IsGifSelection(g_animationName)) {
        LoadTrayIconsFromGifPath(folder);
        return;
    }

    /** Enumerate all .ico and .png files, pick those with numeric in name, sort ascending */
    wchar_t wFolder[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, folder, -1, wFolder, MAX_PATH);

    typedef struct { int hasNum; int num; wchar_t name[MAX_PATH]; wchar_t path[MAX_PATH]; } AnimFile;
    AnimFile files[MAX_TRAY_FRAMES];
    int fileCount = 0;
    
    void AddFilesWithPattern(const wchar_t* pattern) {
        WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(pattern, &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
                if (!dot) continue;
                size_t nameLen = (size_t)(dot - ffd.cFileName);
                if (nameLen == 0 || nameLen >= MAX_PATH) continue;

                int hasNum = 0; int numVal = 0;
                for (size_t i = 0; i < nameLen; ++i) {
                    if (iswdigit(ffd.cFileName[i])) {
                        hasNum = 1; numVal = 0;
                        while (i < nameLen && iswdigit(ffd.cFileName[i])) { numVal = numVal * 10 + (ffd.cFileName[i] - L'0'); i++; }
                        break;
                    }
                }

                if (fileCount < MAX_TRAY_FRAMES) {
                    files[fileCount].hasNum = hasNum;
                    files[fileCount].num = numVal;
                    wcsncpy(files[fileCount].name, ffd.cFileName, nameLen);
                    files[fileCount].name[nameLen] = L'\0';
                    _snwprintf_s(files[fileCount].path, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolder, ffd.cFileName);
                    fileCount++;
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
    }

    wchar_t wSearchIco[MAX_PATH] = {0};
    _snwprintf_s(wSearchIco, MAX_PATH, _TRUNCATE, L"%s\\*.ico", wFolder);
    AddFilesWithPattern(wSearchIco);

    wchar_t wSearchPng[MAX_PATH] = {0};
    _snwprintf_s(wSearchPng, MAX_PATH, _TRUNCATE, L"%s\\*.png", wFolder);
    AddFilesWithPattern(wSearchPng);

    wchar_t wSearchBmp[MAX_PATH] = {0};
    _snwprintf_s(wSearchBmp, MAX_PATH, _TRUNCATE, L"%s\\*.bmp", wFolder);
    AddFilesWithPattern(wSearchBmp);

    wchar_t wSearchJpg[MAX_PATH] = {0};
    _snwprintf_s(wSearchJpg, MAX_PATH, _TRUNCATE, L"%s\\*.jpg", wFolder);
    AddFilesWithPattern(wSearchJpg);

    wchar_t wSearchJpeg[MAX_PATH] = {0};
    _snwprintf_s(wSearchJpeg, MAX_PATH, _TRUNCATE, L"%s\\*.jpeg", wFolder);
    AddFilesWithPattern(wSearchJpeg);

    wchar_t wSearchGif[MAX_PATH] = {0};
    _snwprintf_s(wSearchGif, MAX_PATH, _TRUNCATE, L"%s\\*.gif", wFolder);
    AddFilesWithPattern(wSearchGif);

    wchar_t wSearchTif[MAX_PATH] = {0};
    _snwprintf_s(wSearchTif, MAX_PATH, _TRUNCATE, L"%s\\*.tif", wFolder);
    AddFilesWithPattern(wSearchTif);

    wchar_t wSearchTiff[MAX_PATH] = {0};
    _snwprintf_s(wSearchTiff, MAX_PATH, _TRUNCATE, L"%s\\*.tiff", wFolder);
    AddFilesWithPattern(wSearchTiff);

    if (fileCount == 0) {
        return;
    }

    int cmpAnimFile(const void* a, const void* b) {
        const AnimFile* fa = (const AnimFile*)a;
        const AnimFile* fb = (const AnimFile*)b;
        if (fa->hasNum && fb->hasNum) {
            if (fa->num < fb->num) return -1;
            if (fa->num > fb->num) return 1;
            /** tie-breaker by name */
            return _wcsicmp(fa->name, fb->name);
        }
        /** fallback: case-insensitive name compare */
        return _wcsicmp(fa->name, fb->name);
    }

    qsort(files, (size_t)fileCount, sizeof(AnimFile), cmpAnimFile);

    for (int i = 0; i < fileCount; ++i) {
        HICON hIcon = NULL;
        const wchar_t* ext = wcsrchr(files[i].path, L'.');
        if (ext && (_wcsicmp(ext, L".ico") == 0)) {
            hIcon = (HICON)LoadImageW(NULL, files[i].path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        } else if (ext && (_wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".tif") == 0 || _wcsicmp(ext, L".tiff") == 0)) {
            int cx = GetSystemMetrics(SM_CXSMICON);
            int cy = GetSystemMetrics(SM_CYSMICON);
            /** WIC load png to HICON */
            IWICImagingFactory* pFactory = NULL;
            HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory);
            if (SUCCEEDED(hr) && pFactory) {
                IWICBitmapDecoder* pDecoder = NULL;
                hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, files[i].path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
                if (SUCCEEDED(hr) && pDecoder) {
                    IWICBitmapFrameDecode* pFrame = NULL;
                    hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);
                    if (SUCCEEDED(hr) && pFrame) {
                        IWICBitmapScaler* pScaler = NULL;
                        hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
                        if (SUCCEEDED(hr) && pScaler) {
                            hr = pScaler->lpVtbl->Initialize(pScaler, (IWICBitmapSource*)pFrame, cx, cy, WICBitmapInterpolationModeFant);
                            if (SUCCEEDED(hr)) {
                                IWICFormatConverter* pConverter = NULL;
                                hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
                                if (SUCCEEDED(hr) && pConverter) {
                                    hr = pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pScaler, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
                                    if (SUCCEEDED(hr)) {
                                        BITMAPINFO bi;
                                        ZeroMemory(&bi, sizeof(bi));
                                        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                        bi.bmiHeader.biWidth = cx;
                                        bi.bmiHeader.biHeight = -cy; /** top-down */
                                        bi.bmiHeader.biPlanes = 1;
                                        bi.bmiHeader.biBitCount = 32;
                                        bi.bmiHeader.biCompression = BI_RGB;
                                        VOID* pvBits = NULL;
                                        HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                                        if (hbmColor && pvBits) {
                                            UINT stride = (UINT)(cx * 4);
                                            UINT bufSize = (UINT)(cy * stride);
                                            hr = pConverter->lpVtbl->CopyPixels(pConverter, NULL, stride, bufSize, (BYTE*)pvBits);
                                            if (SUCCEEDED(hr)) {
                                                ICONINFO ii;
                                                ZeroMemory(&ii, sizeof(ii));
                                                ii.fIcon = TRUE;
                                                ii.hbmColor = hbmColor;
                                                ii.hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
                                                hIcon = CreateIconIndirect(&ii);
                                                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                                            }
                                            DeleteObject(hbmColor);
                                        }
                                    }
                                    pConverter->lpVtbl->Release(pConverter);
                                }
                            }
                            pScaler->lpVtbl->Release(pScaler);
                        }
                        pFrame->lpVtbl->Release(pFrame);
                    }
                    pDecoder->lpVtbl->Release(pDecoder);
                }
                pFactory->lpVtbl->Release(pFactory);
            }
            if (SUCCEEDED(hrInit)) CoUninitialize();
        }
        if (hIcon) {
            g_trayIcons[g_trayIconCount++] = hIcon;
        }
    }
}

/** @brief Advance to next icon frame and apply to tray */
static void AdvanceTrayFrame(void) {
    if (!g_trayHwnd) return;
    int count = g_isPreviewActive ? g_previewCount : g_trayIconCount;
    if (count <= 0) return;
    if (g_isPreviewActive) {
        if (g_previewIndex >= g_previewCount) g_previewIndex = 0;
    } else {
        if (g_trayIconIndex >= g_trayIconCount) g_trayIconIndex = 0;
    }

    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayHwnd;
    nid.uID = CLOCK_ID_TRAY_APP_ICON;
    nid.uFlags = NIF_ICON;
    nid.hIcon = g_isPreviewActive ? g_previewIcons[g_previewIndex] : g_trayIcons[g_trayIconIndex];

    Shell_NotifyIconW(NIM_MODIFY, &nid);

    if (g_isPreviewActive) {
        g_previewIndex = (g_previewIndex + 1) % g_previewCount;

        /** For GIF preview, honor per-frame delay */
        if (g_isPreviewGif && g_trayHwnd) {
            int nextPrev = g_previewIndex;
            UINT delayPrev = g_previewFrameDelaysMs[nextPrev];
            if (delayPrev == 0) delayPrev = g_trayInterval > 0 ? g_trayInterval : 150;
            KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
            SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, delayPrev, (TIMERPROC)TrayAnimTimerProc);
        }
    } else {
        g_trayIconIndex = (g_trayIconIndex + 1) % g_trayIconCount;
    }

    /** If current animation is GIF, adjust timer to next frame delay */
    if (!g_isPreviewActive && g_isGifAnimation && g_trayHwnd) {
        int nextIndex = g_trayIconIndex;
        UINT delay = g_frameDelaysMs[nextIndex];
        if (delay == 0) delay = g_trayInterval > 0 ? g_trayInterval : 150;
        KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
        SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, delay, (TIMERPROC)TrayAnimTimerProc);
    }
}

/** @brief Window-proc level timer callback shim */
static void CALLBACK TrayAnimTimerProc(HWND hwnd, UINT msg, UINT_PTR id, DWORD time) {
    (void)msg; (void)id; (void)time;
    AdvanceTrayFrame();
}

void StartTrayAnimation(HWND hwnd, UINT intervalMs) {
    g_trayHwnd = hwnd;
    g_trayInterval = intervalMs > 0 ? intervalMs : 150; /** default ~6-7 fps */
    g_isPreviewActive = FALSE;
    g_previewCount = 0;
    g_previewIndex = 0;

    /** Read current animation name from config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char nameBuf[64] = {0};
    ReadIniString(INI_SECTION_OPTIONS, "ANIMATION_NAME", "%LOCALAPPDATA%\\Catime\\resources\\animations\\cat", nameBuf, sizeof(nameBuf), config_path);
    if (nameBuf[0] != '\0') {
        const char* prefix = "%LOCALAPPDATA%\\Catime\\resources\\animations\\";
        if (_strnicmp(nameBuf, prefix, (int)strlen(prefix)) == 0) {
            const char* rel = nameBuf + strlen(prefix);
            if (*rel) {
                strncpy(g_animationName, rel, sizeof(g_animationName) - 1);
                g_animationName[sizeof(g_animationName) - 1] = '\0';
            }
        } else {
            strncpy(g_animationName, nameBuf, sizeof(g_animationName) - 1);
            g_animationName[sizeof(g_animationName) - 1] = '\0';
        }
    }

    LoadTrayIcons();

    if (g_trayIconCount > 0) {
        AdvanceTrayFrame();
        /** For GIF, honor first frame delay if available */
        UINT firstDelay = (g_isGifAnimation && g_frameDelaysMs[0] > 0) ? g_frameDelaysMs[0] : g_trayInterval;
        SetTimer(hwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
    }
}

void StopTrayAnimation(HWND hwnd) {
    KillTimer(hwnd, TRAY_ANIM_TIMER_ID);
    FreeTrayIcons();
    FreePreviewIcons();
    g_trayHwnd = NULL;
}

/**
 * @brief Get current animation folder name
 */
const char* GetCurrentAnimationName(void) {
    return g_animationName;
}

/**
 * @brief Set and persist current animation folder; reload frames
 */
BOOL SetCurrentAnimationName(const char* name) {
    if (!name || !*name) return FALSE;

    /** Validate selection: either a folder with images, or a single .gif file existing */
    char folder[MAX_PATH] = {0};
    BuildAnimationFolder(name, folder, sizeof(folder));
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, folder, -1, wPath, MAX_PATH);

    DWORD attrs = GetFileAttributesW(wPath);
    BOOL valid = FALSE;
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
            wchar_t wSearch[MAX_PATH] = {0};
            _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wPath);
            WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    wchar_t* ext = wcsrchr(ffd.cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".tif") == 0 || _wcsicmp(ext, L".tiff") == 0)) {
                        valid = TRUE; break;
                    }
                } while (FindNextFileW(hFind, &ffd));
                FindClose(hFind);
            }
        } else if (IsGifSelection(name)) {
            valid = TRUE; /** a valid single GIF file */
        }
    }
    if (!valid) return FALSE;

    strncpy(g_animationName, name, sizeof(g_animationName) - 1);
    g_animationName[sizeof(g_animationName) - 1] = '\0';

    /** Persist to config */
    char config_path[MAX_PATH] = {0};
    GetConfigPath(config_path, sizeof(config_path));
    char animPath[MAX_PATH];
    snprintf(animPath, sizeof(animPath), "%%LOCALAPPDATA%%\\Catime\\resources\\animations\\%s", g_animationName);
    WriteIniString(INI_SECTION_OPTIONS, "ANIMATION_NAME", animPath, config_path);

    /** Reload frames and reset index; ensure timer is running */
    LoadTrayIcons();
    g_trayIconIndex = 0;
    if (g_trayHwnd && g_trayIconCount > 0) {
        AdvanceTrayFrame();
        if (!IsWindow(g_trayHwnd)) return TRUE;
        KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
        UINT firstDelay = (g_isGifAnimation && g_frameDelaysMs[0] > 0) ? g_frameDelaysMs[0] : g_trayInterval;
        SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
    }
    return TRUE;
}


/** Load preview icons for folder and enable preview mode (no persistence) */
void StartAnimationPreview(const char* name) {
    if (!name || !*name) return;
    /** If preview target is a .gif file, decode frames from the single file */
    if (IsGifSelection(name)) {
        char gifPath[MAX_PATH] = {0};
        BuildAnimationFolder(name, gifPath, sizeof(gifPath));
        LoadPreviewIconsFromGifPath(gifPath);
        if (g_previewCount > 0) {
            g_isPreviewActive = TRUE;
            g_previewIndex = 0;
            if (g_trayHwnd) {
                AdvanceTrayFrame();
                if (g_isPreviewGif) {
                    UINT firstDelay = g_previewFrameDelaysMs[0] > 0 ? g_previewFrameDelaysMs[0] : (g_trayInterval ? g_trayInterval : 150);
                    KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
                    SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
                }
            }
        }
        return;
    }

    /** Build and enumerate preview icons from a folder */
    char folder[MAX_PATH] = {0};
    BuildAnimationFolder(name, folder, sizeof(folder));

    wchar_t wFolder[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, folder, -1, wFolder, MAX_PATH);

    /** Collect and sort like LoadTrayIcons */
    typedef struct { int hasNum; int num; wchar_t name[MAX_PATH]; wchar_t path[MAX_PATH]; } AnimFile;
    AnimFile files[MAX_TRAY_FRAMES];
    int fileCount = 0;

    void AddPreviewWithPattern(const wchar_t* pattern) {
        WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(pattern, &ffd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                wchar_t* dot = wcsrchr(ffd.cFileName, L'.');
                if (!dot) continue;
                size_t nameLen = (size_t)(dot - ffd.cFileName);
                if (nameLen == 0 || nameLen >= MAX_PATH) continue;

                int hasNum = 0; int numVal = 0;
                for (size_t i = 0; i < nameLen; ++i) {
                    if (iswdigit(ffd.cFileName[i])) {
                        hasNum = 1; numVal = 0;
                        while (i < nameLen && iswdigit(ffd.cFileName[i])) { numVal = numVal * 10 + (ffd.cFileName[i]-L'0'); i++; }
                        break;
                    }
                }
                if (fileCount < MAX_TRAY_FRAMES) {
                    files[fileCount].hasNum = hasNum;
                    files[fileCount].num = numVal;
                    wcsncpy(files[fileCount].name, ffd.cFileName, nameLen);
                    files[fileCount].name[nameLen] = L'\0';
                    _snwprintf_s(files[fileCount].path, MAX_PATH, _TRUNCATE, L"%s\\%s", wFolder, ffd.cFileName);
                    fileCount++;
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }
    }

    wchar_t wSearchIco[MAX_PATH] = {0};
    _snwprintf_s(wSearchIco, MAX_PATH, _TRUNCATE, L"%s\\*.ico", wFolder);
    AddPreviewWithPattern(wSearchIco);
    wchar_t wSearchPng[MAX_PATH] = {0};
    _snwprintf_s(wSearchPng, MAX_PATH, _TRUNCATE, L"%s\\*.png", wFolder);
    AddPreviewWithPattern(wSearchPng);
    wchar_t wSearchBmp[MAX_PATH] = {0};
    _snwprintf_s(wSearchBmp, MAX_PATH, _TRUNCATE, L"%s\\*.bmp", wFolder);
    AddPreviewWithPattern(wSearchBmp);
    wchar_t wSearchJpg[MAX_PATH] = {0};
    _snwprintf_s(wSearchJpg, MAX_PATH, _TRUNCATE, L"%s\\*.jpg", wFolder);
    AddPreviewWithPattern(wSearchJpg);
    wchar_t wSearchJpeg[MAX_PATH] = {0};
    _snwprintf_s(wSearchJpeg, MAX_PATH, _TRUNCATE, L"%s\\*.jpeg", wFolder);
    AddPreviewWithPattern(wSearchJpeg);
    wchar_t wSearchGif[MAX_PATH] = {0};
    _snwprintf_s(wSearchGif, MAX_PATH, _TRUNCATE, L"%s\\*.gif", wFolder);
    AddPreviewWithPattern(wSearchGif);
    wchar_t wSearchTif[MAX_PATH] = {0};
    _snwprintf_s(wSearchTif, MAX_PATH, _TRUNCATE, L"%s\\*.tif", wFolder);
    AddPreviewWithPattern(wSearchTif);
    wchar_t wSearchTiff[MAX_PATH] = {0};
    _snwprintf_s(wSearchTiff, MAX_PATH, _TRUNCATE, L"%s\\*.tiff", wFolder);
    AddPreviewWithPattern(wSearchTiff);

    if (fileCount == 0) return;

    int cmpAnimFile(const void* a, const void* b) {
        const AnimFile* fa = (const AnimFile*)a;
        const AnimFile* fb = (const AnimFile*)b;
        if (fa->hasNum && fb->hasNum) {
            if (fa->num < fb->num) return -1;
            if (fa->num > fb->num) return 1;
            return _wcsicmp(fa->name, fb->name);
        }
        return _wcsicmp(fa->name, fb->name);
    }
    qsort(files, (size_t)fileCount, sizeof(AnimFile), cmpAnimFile);

    FreePreviewIcons();
    for (int i = 0; i < fileCount; ++i) {
        HICON hIcon = NULL;
        const wchar_t* ext = wcsrchr(files[i].path, L'.');
        if (ext && (_wcsicmp(ext, L".ico") == 0)) {
            hIcon = (HICON)LoadImageW(NULL, files[i].path, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        } else if (ext && (_wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".bmp") == 0 || _wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".gif") == 0 || _wcsicmp(ext, L".tif") == 0 || _wcsicmp(ext, L".tiff") == 0)) {
            int cx = GetSystemMetrics(SM_CXSMICON);
            int cy = GetSystemMetrics(SM_CYSMICON);
            IWICImagingFactory* pFactory = NULL;
            HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&pFactory);
            if (SUCCEEDED(hr) && pFactory) {
                IWICBitmapDecoder* pDecoder = NULL;
                hr = pFactory->lpVtbl->CreateDecoderFromFilename(pFactory, files[i].path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
                if (SUCCEEDED(hr) && pDecoder) {
                    IWICBitmapFrameDecode* pFrame = NULL;
                    hr = pDecoder->lpVtbl->GetFrame(pDecoder, 0, &pFrame);
                    if (SUCCEEDED(hr) && pFrame) {
                        IWICBitmapScaler* pScaler = NULL;
                        hr = pFactory->lpVtbl->CreateBitmapScaler(pFactory, &pScaler);
                        if (SUCCEEDED(hr) && pScaler) {
                            hr = pScaler->lpVtbl->Initialize(pScaler, (IWICBitmapSource*)pFrame, cx, cy, WICBitmapInterpolationModeFant);
                            if (SUCCEEDED(hr)) {
                                IWICFormatConverter* pConverter = NULL;
                                hr = pFactory->lpVtbl->CreateFormatConverter(pFactory, &pConverter);
                                if (SUCCEEDED(hr) && pConverter) {
                                    hr = pConverter->lpVtbl->Initialize(pConverter, (IWICBitmapSource*)pScaler, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
                                    if (SUCCEEDED(hr)) {
                                        BITMAPINFO bi;
                                        ZeroMemory(&bi, sizeof(bi));
                                        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                        bi.bmiHeader.biWidth = cx;
                                        bi.bmiHeader.biHeight = -cy;
                                        bi.bmiHeader.biPlanes = 1;
                                        bi.bmiHeader.biBitCount = 32;
                                        bi.bmiHeader.biCompression = BI_RGB;
                                        VOID* pvBits = NULL;
                                        HBITMAP hbmColor = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
                                        if (hbmColor && pvBits) {
                                            UINT stride = (UINT)(cx * 4);
                                            UINT bufSize = (UINT)(cy * stride);
                                            hr = pConverter->lpVtbl->CopyPixels(pConverter, NULL, stride, bufSize, (BYTE*)pvBits);
                                            if (SUCCEEDED(hr)) {
                                                ICONINFO ii;
                                                ZeroMemory(&ii, sizeof(ii));
                                                ii.fIcon = TRUE;
                                                ii.hbmColor = hbmColor;
                                                ii.hbmMask = CreateBitmap(cx, cy, 1, 1, NULL);
                                                hIcon = CreateIconIndirect(&ii);
                                                if (ii.hbmMask) DeleteObject(ii.hbmMask);
                                            }
                                            DeleteObject(hbmColor);
                                        }
                                    }
                                    pConverter->lpVtbl->Release(pConverter);
                                }
                            }
                            pScaler->lpVtbl->Release(pScaler);
                        }
                        pFrame->lpVtbl->Release(pFrame);
                    }
                    pDecoder->lpVtbl->Release(pDecoder);
                }
                pFactory->lpVtbl->Release(pFactory);
            }
            if (SUCCEEDED(hrInit)) CoUninitialize();
        }
        if (hIcon) {
            g_previewIcons[g_previewCount++] = hIcon;
        }
    }

    if (g_previewCount > 0) {
        g_isPreviewActive = TRUE;
        g_previewIndex = 0;
        if (g_trayHwnd) {
            AdvanceTrayFrame();
        }
    }
}

void CancelAnimationPreview(void) {
    if (!g_isPreviewActive) return;
    g_isPreviewActive = FALSE;
    FreePreviewIcons();
    g_previewIndex = 0;
    if (g_trayHwnd) {
        AdvanceTrayFrame();
        /** Restore timer for normal animation if needed */
        if (g_isGifAnimation) {
            UINT firstDelay = g_frameDelaysMs[g_trayIconIndex] > 0 ? g_frameDelaysMs[g_trayIconIndex] : (g_trayInterval ? g_trayInterval : 150);
            KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
            SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, firstDelay, (TIMERPROC)TrayAnimTimerProc);
        } else {
            KillTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID);
            SetTimer(g_trayHwnd, TRAY_ANIM_TIMER_ID, g_trayInterval ? g_trayInterval : 150, (TIMERPROC)TrayAnimTimerProc);
        }
    }
}


static void OpenAnimationsFolder(void) {
    char base[MAX_PATH] = {0};
    GetAnimationsFolderPath(base, sizeof(base));
    wchar_t wPath[MAX_PATH] = {0};
    MultiByteToWideChar(CP_UTF8, 0, base, -1, wPath, MAX_PATH);
    ShellExecuteW(NULL, L"open", wPath, NULL, NULL, SW_SHOWNORMAL);
}

BOOL HandleAnimationMenuCommand(HWND hwnd, UINT id) {
    if (id == CLOCK_IDM_ANIMATIONS_OPEN_DIR) {
        OpenAnimationsFolder();
        return TRUE;
    }
    if (id >= CLOCK_IDM_ANIMATIONS_BASE && id < CLOCK_IDM_ANIMATIONS_BASE + 1000) {
        char animRootUtf8[MAX_PATH] = {0};
        GetAnimationsFolderPath(animRootUtf8, sizeof(animRootUtf8));
        wchar_t wRoot[MAX_PATH] = {0};
        MultiByteToWideChar(CP_UTF8, 0, animRootUtf8, -1, wRoot, MAX_PATH);
        wchar_t wSearch[MAX_PATH] = {0};
        _snwprintf_s(wSearch, MAX_PATH, _TRUNCATE, L"%s\\*", wRoot);

        WIN32_FIND_DATAW ffd; HANDLE hFind = FindFirstFileW(wSearch, &ffd);
        UINT nextId = CLOCK_IDM_ANIMATIONS_BASE;
        BOOL changed = FALSE;
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;
                if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (nextId == id) {
                        char folderUtf8[MAX_PATH] = {0};
                        WideCharToMultiByte(CP_UTF8, 0, ffd.cFileName, -1, folderUtf8, MAX_PATH, NULL, NULL);
                        changed = SetCurrentAnimationName(folderUtf8);
                        FindClose(hFind);
                        return changed ? TRUE : FALSE;
                    }
                    nextId++;
                }
            } while (FindNextFileW(hFind, &ffd));
            FindClose(hFind);
        }

        /** Second pass: match .gif files with IDs */
        WIN32_FIND_DATAW ffd2; HANDLE hFind2 = FindFirstFileW(wSearch, &ffd2);
        if (hFind2 != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(ffd2.cFileName, L".") == 0 || wcscmp(ffd2.cFileName, L"..") == 0) continue;
                if (!(ffd2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    wchar_t* ext = wcsrchr(ffd2.cFileName, L'.');
                    if (ext && (_wcsicmp(ext, L".gif") == 0)) {
                        if (nextId == id) {
                            char fileUtf8[MAX_PATH] = {0};
                            WideCharToMultiByte(CP_UTF8, 0, ffd2.cFileName, -1, fileUtf8, MAX_PATH, NULL, NULL);
                            changed = SetCurrentAnimationName(fileUtf8);
                            FindClose(hFind2);
                            return changed ? TRUE : FALSE;
                        }
                        nextId++;
                    }
                }
            } while (FindNextFileW(hFind2, &ffd2));
            FindClose(hFind2);
        }
        return changed ? TRUE : FALSE;
    }
    return FALSE;
}

