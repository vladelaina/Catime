#ifndef TEXT_EFFECT_H
#define TEXT_EFFECT_H

#include <stddef.h>
#include <windows.h>
#include "../resource/resource.h"

#define TEXT_EFFECT_CONFIG_VALUES "NONE/GLOW/GLASS/NEON/HOLOGRAPHIC/LIQUID/AQUA/RETRO"

#define TEXT_EFFECT_DEFINITION_LIST(X) \
    X(GLOW,        "GLOW",        L"Glow Effect",        FALSE, TRUE, TRUE) \
    X(GLASS,       "GLASS",       L"Optical Prism",      FALSE, TRUE, TRUE) \
    X(NEON,        "NEON",        L"Neon Tube",          FALSE, TRUE, TRUE) \
    X(HOLOGRAPHIC, "HOLOGRAPHIC", L"Holographic Effect", FALSE, TRUE, TRUE) \
    X(LIQUID,      "LIQUID",      L"Liquid Flow",        TRUE,  TRUE, TRUE) \
    X(AQUA,        "AQUA",        L"Aqua Shimmer",       TRUE,  FALSE, TRUE) \
    X(RETRO,       "RETRO",       L"Retro Shadow",       FALSE, TRUE, FALSE)

typedef enum {
    TEXT_EFFECT_NONE = 0,
#define X(Suffix, ConfigValue, MenuLabel, NeedsRenderTimer, UsesAnimatedTextColor, UsesSharedEffectBuffer) TEXT_EFFECT_##Suffix,
    TEXT_EFFECT_DEFINITION_LIST(X)
#undef X
    TEXT_EFFECT_COUNT,

    EFFECT_TYPE_NONE = TEXT_EFFECT_NONE,
#define X(Suffix, ConfigValue, MenuLabel, NeedsRenderTimer, UsesAnimatedTextColor, UsesSharedEffectBuffer) EFFECT_TYPE_##Suffix = TEXT_EFFECT_##Suffix,
    TEXT_EFFECT_DEFINITION_LIST(X)
#undef X
} TextEffectType;

typedef TextEffectType EffectType;

typedef struct {
    TextEffectType type;
    const char* configValue;
    const wchar_t* menuLabelKey;
    UINT menuId;
    BOOL needsRenderTimer;
    BOOL usesAnimatedTextColor;
    BOOL usesSharedEffectBuffer;
} TextEffectDefinition;

size_t TextEffect_GetCount(void);
const TextEffectDefinition* TextEffect_GetByIndex(size_t index);
const TextEffectDefinition* TextEffect_GetByType(TextEffectType type);
const TextEffectDefinition* TextEffect_GetByMenuId(UINT menuId);
BOOL TextEffect_IsSelectable(TextEffectType type);
BOOL TextEffect_IsMenuId(UINT menuId);
TextEffectType TextEffect_FromMenuId(UINT menuId);
TextEffectType TextEffect_FromConfigString(const char* value);
const char* TextEffect_ToConfigString(TextEffectType type);
BOOL TextEffect_NeedsRenderTimer(TextEffectType type);
BOOL TextEffect_UsesAnimatedTextColor(TextEffectType type);
BOOL TextEffect_UsesSharedEffectBuffer(TextEffectType type);

#endif /* TEXT_EFFECT_H */
