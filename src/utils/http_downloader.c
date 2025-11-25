#include "utils/http_downloader.h"
#include "log.h"
#include <stdio.h>

typedef HRESULT (WINAPI *FuncURLDownloadToFileW)(LPUNKNOWN, LPCWSTR, LPCWSTR, DWORD, LPUNKNOWN);

static HMODULE g_hUrlMon = NULL;
static FuncURLDownloadToFileW pURLDownloadToFileW = NULL;

// DJB2 Hash for string
static unsigned long HashString(const wchar_t* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

BOOL IsHttpUrl(const wchar_t* path) {
    if (!path) return FALSE;
    return (_wcsnicmp(path, L"http://", 7) == 0 || _wcsnicmp(path, L"https://", 8) == 0);
}

void GetLocalCachePath(const wchar_t* url, wchar_t* buffer, size_t maxLen) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    
    unsigned long hash = HashString(url);
    
    // Append "Catime" subfolder
    wchar_t cacheDir[MAX_PATH];
    swprintf(cacheDir, MAX_PATH, L"%sCatimeCache", tempPath);
    CreateDirectoryW(cacheDir, NULL);
    
    // Simple hashing for filename
    // Try to detect extension from URL for better UX (thumbnails in Explorer)
    const wchar_t* ext = L".dat";
    const wchar_t* lastDot = wcsrchr(url, L'.');
    if (lastDot) {
        // Check if extension is reasonable length (e.g. .png, .jpeg)
        // And doesn't contain slash or question mark
        size_t len = wcslen(lastDot);
        if (len >= 4 && len <= 5) { // .png (4) to .jpeg (5)
            BOOL clean = TRUE;
            for (size_t i = 0; i < len; i++) {
                wchar_t c = lastDot[i];
                if (c == L'/' || c == L'\\' || c == L'?' || c == L'&') {
                    clean = FALSE;
                    break;
                }
            }
            if (clean) {
                ext = lastDot;
            }
        }
    }

    swprintf(buffer, maxLen, L"%s\\img_%lu%s", cacheDir, hash, ext);
}

typedef struct {
    wchar_t url[1024];
    wchar_t path[MAX_PATH];
    HWND hwnd;
} DownloadArgs;

static DWORD WINAPI DownloadThread(LPVOID lpParam) {
    DownloadArgs* args = (DownloadArgs*)lpParam;
    
    if (!g_hUrlMon) {
        g_hUrlMon = LoadLibraryA("urlmon.dll");
        if (g_hUrlMon) {
            pURLDownloadToFileW = (FuncURLDownloadToFileW)GetProcAddress(g_hUrlMon, "URLDownloadToFileW");
        }
    }

    if (pURLDownloadToFileW) {
        // Blocking download
        // BINDF_GETNEWESTVERSION = 0x10 to ensure we get fresh content? 
        // Or 0 to use cache. 0 is fine for now.
        HRESULT hr = pURLDownloadToFileW(NULL, args->url, args->path, 0, NULL);
        if (hr == S_OK) {
            LOG_INFO("Download success: %S", args->url);
            if (args->hwnd) {
                // Notify UI to repaint
                InvalidateRect(args->hwnd, NULL, FALSE);
            }
        } else {
            LOG_WARNING("Download failed: 0x%08x", hr);
        }
    } else {
        LOG_ERROR("Failed to load URLDownloadToFileW");
    }
    
    free(args);
    return 0;
}

void DownloadFileAsync(const wchar_t* url, const wchar_t* localPath, HWND hwndNotify) {
    DownloadArgs* args = (DownloadArgs*)malloc(sizeof(DownloadArgs));
    if (args) {
        wcsncpy(args->url, url, 1023);
        args->url[1023] = L'\0';
        
        wcsncpy(args->path, localPath, MAX_PATH - 1);
        args->path[MAX_PATH - 1] = L'\0';
        
        args->hwnd = hwndNotify;
        
        // Create a detached thread
        HANDLE hThread = CreateThread(NULL, 0, DownloadThread, args, 0, NULL);
        if (hThread) {
            CloseHandle(hThread);
        } else {
            free(args);
        }
    }
}
