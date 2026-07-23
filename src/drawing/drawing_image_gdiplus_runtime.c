/**
 * @file drawing_image_gdiplus_runtime.c
 * @brief Dynamic GDI+ loading, synchronization, and subsystem lifetime
 */
#include "drawing/drawing_image.h"
#include "drawing_image_gdiplus_internal.h"

#include "log.h"
#include "utils/win32_dynamic_loader.h"

#define GDIPLUS_INIT_FAILURE_RETRY_MS 5000
#define LOAD_GDIPLUS_PROC(module, name, target) \
    CATIME_LOAD_PROC_ADDRESS((module), (name), (target))

DrawingImageRuntime g_drawingImageRuntime = {0};

static INIT_ONCE g_imageStateLockOnce = INIT_ONCE_STATIC_INIT;
static CRITICAL_SECTION g_imageStateLock;
static BOOL g_initFailureRecorded = FALSE;
static DWORD g_initFailureCooldownUntil = 0;

static BOOL CALLBACK InitializeStateLock(PINIT_ONCE initOnce,
                                         PVOID parameter,
                                         PVOID* context) {
    UNREFERENCED_PARAMETER(initOnce);
    UNREFERENCED_PARAMETER(parameter);
    UNREFERENCED_PARAMETER(context);
    InitializeCriticalSection(&g_imageStateLock);
    return TRUE;
}

BOOL DrawingImage_LockState(void) {
    if (!InitOnceExecuteOnce(&g_imageStateLockOnce, InitializeStateLock,
                             NULL, NULL)) {
        return FALSE;
    }
    EnterCriticalSection(&g_imageStateLock);
    return TRUE;
}

void DrawingImage_UnlockState(void) {
    LeaveCriticalSection(&g_imageStateLock);
}

static void ResetRuntime(void) {
    ZeroMemory(&g_drawingImageRuntime, sizeof(g_drawingImageRuntime));
}

static BOOL HasRequiredProcedures(void) {
    DrawingImageRuntime* runtime = &g_drawingImageRuntime;
    return runtime->startup && runtime->shutdown && runtime->createFromHdc &&
           runtime->deleteGraphics && runtime->createBitmapFromFile &&
           runtime->disposeImage && runtime->drawImageRect &&
           runtime->getImageWidth && runtime->getImageHeight;
}

static BOOL ShouldRetryInitialization(void) {
    return !g_initFailureRecorded ||
           (LONG)(g_initFailureCooldownUntil - GetTickCount()) <= 0;
}

static void RecordInitializationFailure(void) {
    DWORD retryAt = GetTickCount() + GDIPLUS_INIT_FAILURE_RETRY_MS;
    g_initFailureRecorded = TRUE;
    g_initFailureCooldownUntil = retryAt ? retryAt : 1;
}

static void UnloadFailedRuntime(void) {
    if (g_drawingImageRuntime.module) {
        FreeLibrary(g_drawingImageRuntime.module);
    }
    ResetRuntime();
    RecordInitializationFailure();
}

static void InitializeLocked(void) {
    DrawingImageRuntime* runtime = &g_drawingImageRuntime;
    GdiplusStartupInput input = {1, NULL, FALSE, FALSE};

    if (runtime->module || !ShouldRetryInitialization()) return;
    runtime->module = LoadLibraryW(L"gdiplus.dll");
    if (!runtime->module) {
        LOG_ERROR("Failed to load gdiplus.dll");
        RecordInitializationFailure();
        return;
    }

    LOAD_GDIPLUS_PROC(runtime->module, "GdiplusStartup", runtime->startup);
    LOAD_GDIPLUS_PROC(runtime->module, "GdiplusShutdown", runtime->shutdown);
    LOAD_GDIPLUS_PROC(runtime->module, "GdipCreateFromHDC",
                      runtime->createFromHdc);
    LOAD_GDIPLUS_PROC(runtime->module, "GdipDeleteGraphics",
                      runtime->deleteGraphics);
    LOAD_GDIPLUS_PROC(runtime->module, "GdipCreateBitmapFromFile",
                      runtime->createBitmapFromFile);
    LOAD_GDIPLUS_PROC(runtime->module, "GdipDisposeImage",
                      runtime->disposeImage);
    LOAD_GDIPLUS_PROC(runtime->module, "GdipDrawImageRectI",
                      runtime->drawImageRect);
    LOAD_GDIPLUS_PROC(runtime->module, "GdipGetImageWidth",
                      runtime->getImageWidth);
    LOAD_GDIPLUS_PROC(runtime->module, "GdipGetImageHeight",
                      runtime->getImageHeight);
    if (!HasRequiredProcedures()) {
        LOG_ERROR("Required GDI+ entry points are unavailable");
        UnloadFailedRuntime();
        return;
    }
    if (runtime->startup(&runtime->token, &input, NULL) !=
        GDIPLUS_STATUS_OK) {
        LOG_ERROR("GDI+ startup failed");
        runtime->token = 0;
        UnloadFailedRuntime();
        return;
    }
    g_initFailureRecorded = FALSE;
    g_initFailureCooldownUntil = 0;
}

BOOL DrawingImage_EnsureInitializedLocked(void) {
    if (!g_drawingImageRuntime.token) InitializeLocked();
    return g_drawingImageRuntime.token != 0;
}

void InitDrawingImage(void) {
    if (!DrawingImage_LockState()) return;
    InitializeLocked();
    DrawingImage_UnlockState();
}

void ShutdownDrawingImage(void) {
    if (!DrawingImage_LockState()) return;
    DrawingImageCache_Clear();
    DrawingImageFailure_Reset();
    if (g_drawingImageRuntime.token && g_drawingImageRuntime.shutdown) {
        g_drawingImageRuntime.shutdown(g_drawingImageRuntime.token);
    }
    g_drawingImageRuntime.token = 0;
    if (g_drawingImageRuntime.module) {
        FreeLibrary(g_drawingImageRuntime.module);
    }
    ResetRuntime();
    g_initFailureRecorded = FALSE;
    g_initFailureCooldownUntil = 0;
    DrawingImage_UnlockState();
}
