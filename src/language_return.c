#include "language_internal.h"
#ifdef CATIME_USE_WIN32_FLS
#include "utils/thread_local_buffer.h"
#endif

#ifdef CATIME_USE_WIN32_FLS
typedef struct {
    wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    unsigned int nextSlot;
} LocalizedReturnStorage;
static ThreadLocalBuffer g_localizedReturnStorage =
    THREAD_LOCAL_BUFFER_STATIC_INIT(sizeof(LocalizedReturnStorage));
#endif

const wchar_t* Language_CopyReturnValue(const wchar_t* value) {
    if (!value) return L"";
#if defined(CATIME_USE_WIN32_FLS)
    LocalizedReturnStorage* storage =
        (LocalizedReturnStorage*)ThreadLocalBuffer_Get(&g_localizedReturnStorage);
    if (!storage) return value;
    wchar_t* slot = storage->buffers[
        storage->nextSlot++ % LOCALIZED_RETURN_SLOT_COUNT];
#elif defined(_MSC_VER)
    __declspec(thread) static wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    __declspec(thread) static unsigned int nextSlot = 0;
    wchar_t* slot = buffers[nextSlot++ % LOCALIZED_RETURN_SLOT_COUNT];
#elif defined(__GNUC__)
    static __thread wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    static __thread unsigned int nextSlot = 0;
    wchar_t* slot = buffers[nextSlot++ % LOCALIZED_RETURN_SLOT_COUNT];
#else
    static wchar_t buffers[LOCALIZED_RETURN_SLOT_COUNT][MAX_STRING_LENGTH];
    static unsigned int nextSlot = 0;
    wchar_t* slot = buffers[nextSlot++ % LOCALIZED_RETURN_SLOT_COUNT];
#endif
    wcsncpy_s(slot, MAX_STRING_LENGTH, value, _TRUNCATE);
    return slot;
}
