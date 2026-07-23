    for (int i = 1; i < entryCount; i++) {
        if (CompareFileTime(&entries[newest].lastWriteTime,
                            &entries[i].lastWriteTime) < 0) {
            newest = i;
        }
    }
    return newest;
}

static BOOL AddImageCachePruneEntry(ImageCachePruneEntry** entries,
                                    int* entryCount,
                                    int* entryCapacity,
                                    int* newestEntry,
                                    const wchar_t* path,
                                    const FILETIME* lastWriteTime,
                                    ULONGLONG size) {
    if (!entries || !entryCount || !entryCapacity || !newestEntry ||
        !path || !lastWriteTime) {
        return FALSE;
    }

    if (*entryCount < IMAGE_CACHE_PRUNE_SCAN_LIMIT) {
        if (*entryCount == *entryCapacity) {
            int newCapacity = *entryCapacity == 0 ? 64 : *entryCapacity * 2;
            if (newCapacity > IMAGE_CACHE_PRUNE_SCAN_LIMIT) {
                newCapacity = IMAGE_CACHE_PRUNE_SCAN_LIMIT;
            }
            ImageCachePruneEntry* newEntries =
                (ImageCachePruneEntry*)realloc(*entries,
                    (size_t)newCapacity * sizeof(ImageCachePruneEntry));
            if (!newEntries) {
                return FALSE;
            }
            *entries = newEntries;
            *entryCapacity = newCapacity;
        }

        ImageCachePruneEntry* entry = &(*entries)[(*entryCount)++];
        wcscpy_s(entry->path, MAX_PATH, path);
        entry->lastWriteTime = *lastWriteTime;
        entry->size = size;
        if (*entryCount == 1 ||
            CompareFileTime(&(*entries)[*newestEntry].lastWriteTime,
                            lastWriteTime) < 0) {
            *newestEntry = *entryCount - 1;
        }
        return TRUE;
    }

    if (*entryCount <= 0) {
        return FALSE;
    }

    if (CompareFileTime(lastWriteTime,
                        &(*entries)[*newestEntry].lastWriteTime) >= 0) {
        return TRUE;
    }

    ImageCachePruneEntry* entry = &(*entries)[*newestEntry];
    wcscpy_s(entry->path, MAX_PATH, path);
    entry->lastWriteTime = *lastWriteTime;
    entry->size = size;
    *newestEntry = FindNewestImageCachePruneEntry(*entries, *entryCount);
    return TRUE;
}

static void PruneImageCacheDirectory(const wchar_t* cacheDir, const wchar_t* keepPath) {
    if (!cacheDir || !*cacheDir) return;

    wchar_t searchPath[MAX_PATH];
    if (_snwprintf_s(searchPath, MAX_PATH, _TRUNCATE, L"%s\\*", cacheDir) < 0) {
        return;
    }

    int totalRemoved = 0;

    for (;;) {
        ImageCachePruneEntry* entries = NULL;
        int entryCount = 0;
        int entryCapacity = 0;
        int newestEntry = 0;
        int cacheFileCount = 0;
        ULONGLONG cacheBytes = 0;

        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath, &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            break;
        }

        do {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                continue;
            }
            if (!IsGeneratedImageCacheFileName(findData.cFileName)) {
                continue;
            }

            wchar_t fullPath[MAX_PATH];
            if (_snwprintf_s(fullPath, MAX_PATH, _TRUNCATE, L"%s\\%s",
                             cacheDir, findData.cFileName) < 0) {
                continue;
            }

            ULONGLONG fileSize = ((ULONGLONG)findData.nFileSizeHigh << 32) |
                                 (ULONGLONG)findData.nFileSizeLow;
            cacheFileCount++;
            cacheBytes = AddImageCacheBytesSaturated(cacheBytes, fileSize);

            if (keepPath && _wcsicmp(fullPath, keepPath) == 0) {
                continue;
            }

            if (!AddImageCachePruneEntry(&entries, &entryCount, &entryCapacity,
                                         &newestEntry, fullPath,
                                         &findData.ftLastWriteTime, fileSize)) {
                break;
            }
        } while (FindNextFileW(hFind, &findData));

        FindClose(hFind);

        if ((cacheBytes <= IMAGE_CACHE_MAX_BYTES &&
             cacheFileCount <= IMAGE_CACHE_MAX_FILES) ||
            !entries || entryCount == 0) {
            free(entries);
            break;
        }

        qsort(entries, (size_t)entryCount, sizeof(entries[0]),
              CompareImageCachePruneEntryByAge);

        int removedThisPass = 0;
        for (int i = 0;
             i < entryCount &&
             (cacheBytes > IMAGE_CACHE_MAX_BYTES ||
              cacheFileCount > IMAGE_CACHE_MAX_FILES);
             i++) {
            if (DeleteFileW(entries[i].path)) {
                if (entries[i].size <= cacheBytes) {
                    cacheBytes -= entries[i].size;
                } else {
                    cacheBytes = 0;
                }
                if (cacheFileCount > 0) {
                    cacheFileCount--;
                }
                removedThisPass++;
            }
        }

        free(entries);

        if (removedThisPass == 0) {
            break;
        }
        totalRemoved += removedThisPass;
    }

    if (totalRemoved > 0) {
        LOG_INFO("Pruned %d cached markdown image file(s)", totalRemoved);
    }
}

/* ============================================================================
 * Network Image Download
 * ============================================================================ */

static BOOL DownloadImageToCacheForGeneration(const wchar_t* url, wchar_t* localPath,
                                              LONG generation) {
    if (!url || !localPath) return FALSE;
    localPath[0] = L'\0';
    if (IsDownloadCanceled(generation)) return FALSE;

    /* Get cache directory */
    wchar_t cacheDir[MAX_PATH];
    if (!GetImageCacheDirectory(cacheDir, MAX_PATH)) {
        LOG_ERROR("Failed to get image cache directory");
        return FALSE;
    }

    /* Generate cache filename */
    wchar_t filename[64];
    GenerateCacheFilename(url, filename, 64);

    /* Build full cache path */
    int pathWritten = _snwprintf_s(localPath, MAX_PATH, _TRUNCATE, L"%s\\%s", cacheDir, filename);
    if (pathWritten < 0) {
        localPath[0] = L'\0';
        return FALSE;
    }

    /* Check if already cached */
    if (IsUsableCachedImageFileW(localPath)) {
        return TRUE;
    }
    RemoveInvalidImageCacheEntryW(localPath);
    if (GetFileAttributesW(localPath) != INVALID_FILE_ATTRIBUTES) {
        localPath[0] = L'\0';
        return FALSE;
    }

    /* Open Internet session */
    HINTERNET hInternet = InternetOpenW(L"Catime/1.0", INTERNET_OPEN_TYPE_DIRECT,
                                         NULL, NULL, 0);
    if (!hInternet) {
        if (!IsDownloadCanceled(generation)) {
            LOG_ERROR("Failed to open Internet session");
        }
        return FALSE;
    }
    if (!TrackDownloadHandle(hInternet, generation)) {
        InternetCloseHandle(hInternet);
        return FALSE;
    }
    if (IsDownloadCanceled(generation)) {
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    DWORD timeoutMs = IMAGE_DOWNLOAD_TIMEOUT_MS;
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    /* Open URL */
    HINTERNET hUrl = InternetOpenUrlW(hInternet, url, NULL, 0,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        if (!IsDownloadCanceled(generation)) {
            LOG_ERROR("Failed to open URL: %ls", url);
        }
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }
    if (!TrackDownloadHandle(hUrl, generation)) {
        InternetCloseHandle(hUrl);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                        &statusCode, &statusCodeSize, NULL) ||
        statusCode < 200 || statusCode >= 300) {
        if (!IsDownloadCanceled(generation)) {
            LOG_WARNING("Image download returned HTTP status %lu: %ls",
                        statusCode, url);
        }
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }

    if (IsDownloadCanceled(generation)) {
        CloseTrackedDownloadHandle(&hUrl, generation);
        CloseTrackedDownloadHandle(&hInternet, generation);
        return FALSE;
    }
