#ifndef FONT_MANAGER_INTERNAL_H
#define FONT_MANAGER_INTERNAL_H

#include "font/font_manager.h"
#include "font/font_ttf_parser.h"
#include "font/font_path_manager.h"
#include "font/font_config.h"
#include "utils/string_convert.h"
#include "utils/path_utils.h"
#ifdef CATIME_COMPRESSED_EMBEDDED_RESOURCES
#include "utils/compressed_resource.h"
#endif
#include "config.h"
#include "log.h"
#include "../../resource/resource.h"

#include <stdio.h>
#include <string.h>

BOOL FontManager_CopyStringExact(
    const char* source, char* output, size_t outputSize);
BOOL FontManager_ShouldAttemptAutoFix(const char* fontFileName);
BOOL FontManager_ReloadFromConfigName(
    HINSTANCE hInstance, const char* fontName,
    char* outInternalName, size_t outInternalNameSize);
BOOL FontManager_WriteDataToFile(
    const void* fontData, size_t fontLength, const char* outputPath);

#endif
