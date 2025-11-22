#ifndef OLE_DROP_TARGET_H
#define OLE_DROP_TARGET_H

#include <windows.h>
#include <ole2.h>

/**
 * @brief Initialize OLE and register drag drop for window
 */
void InitializeOleDropTarget(HWND hwnd);

/**
 * @brief Revoke drag drop and uninitialize OLE
 */
void CleanupOleDropTarget(HWND hwnd);

#endif // OLE_DROP_TARGET_H
