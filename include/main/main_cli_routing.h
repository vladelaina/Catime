/**
 * @file main_cli_routing.h
 * @brief CLI command routing and forwarding
 */

#ifndef MAIN_CLI_ROUTING_H
#define MAIN_CLI_ROUTING_H

#include <windows.h>

/**
 * Try to forward simple CLI commands to existing instance
 * @param hwndExisting Handle to existing instance
 * @param lpCmdLine Command line string
 * @return TRUE if command was forwarded, FALSE otherwise
 */
BOOL TryForwardSimpleCliToExisting(HWND hwndExisting, const wchar_t* lpCmdLine);

#endif /* MAIN_CLI_ROUTING_H */

