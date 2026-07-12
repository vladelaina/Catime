/*
 * Build MinGW's official StrSafe implementation inside Catime so section GC
 * and LTO can retain only the StringCch/StringCb workers the application uses.
 */
#define __CRT_STRSAFE_IMPL
#include <strsafe.h>
