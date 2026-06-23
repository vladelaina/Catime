#include "text_effect.h"
#include <string.h>

static const TextEffectDefinition TEXT_EFFECT_DEFINITIONS[] = {
#define X(Suffix, ConfigValue, MenuLabel, NeedsRenderTimer, UsesAnimatedTextColor, UsesSharedEffectBuffer) \
    {TEXT_EFFECT_##Suffix, ConfigValue, MenuLabel, CLOCK_IDM_TEXT_EFFECT_BASE + TEXT_EFFECT_##Suffix, NeedsRenderTimer, UsesAnimatedTextColor, UsesSharedEffectBuffer},
    TEXT_EFFECT_DEFINITION_LIST(X)
#undef X
};

size_t TextEffect_GetCount(void) {
    return sizeof(TEXT_EFFECT_DEFINITIONS) / sizeof(TEXT_EFFECT_DEFINITIONS[0]);
}

const TextEffectDefinition* TextEffect_GetByIndex(size_t index) {
    if (index >= TextEffect_GetCount()) {
        return NULL;
    }
    return &TEXT_EFFECT_DEFINITIONS[index];
}

const TextEffectDefinition* TextEffect_GetByType(TextEffectType type) {
    for (size_t i = 0; i < TextEffect_GetCount(); ++i) {
        if (TEXT_EFFECT_DEFINITIONS[i].type == type) {
            return &TEXT_EFFECT_DEFINITIONS[i];
        }
    }
    return NULL;
}

const TextEffectDefinition* TextEffect_GetByMenuId(UINT menuId) {
    for (size_t i = 0; i < TextEffect_GetCount(); ++i) {
        if (TEXT_EFFECT_DEFINITIONS[i].menuId == menuId) {
            return &TEXT_EFFECT_DEFINITIONS[i];
        }
    }
    return NULL;
}

BOOL TextEffect_IsSelectable(TextEffectType type) {
    return TextEffect_GetByType(type) != NULL;
}

BOOL TextEffect_IsMenuId(UINT menuId) {
    return TextEffect_GetByMenuId(menuId) != NULL;
}

TextEffectType TextEffect_FromMenuId(UINT menuId) {
    const TextEffectDefinition* effect = TextEffect_GetByMenuId(menuId);
    return effect ? effect->type : TEXT_EFFECT_NONE;
}

TextEffectType TextEffect_FromConfigString(const char* value) {
    if (!value || !value[0] || _stricmp(value, "NONE") == 0) {
        return TEXT_EFFECT_NONE;
    }

    for (size_t i = 0; i < TextEffect_GetCount(); ++i) {
        if (_stricmp(TEXT_EFFECT_DEFINITIONS[i].configValue, value) == 0) {
            return TEXT_EFFECT_DEFINITIONS[i].type;
        }
    }
    return TEXT_EFFECT_NONE;
}

const char* TextEffect_ToConfigString(TextEffectType type) {
    const TextEffectDefinition* effect = TextEffect_GetByType(type);
    return effect ? effect->configValue : "NONE";
}

BOOL TextEffect_NeedsRenderTimer(TextEffectType type) {
    const TextEffectDefinition* effect = TextEffect_GetByType(type);
    return effect ? effect->needsRenderTimer : FALSE;
}

BOOL TextEffect_UsesAnimatedTextColor(TextEffectType type) {
    const TextEffectDefinition* effect = TextEffect_GetByType(type);
    return effect ? effect->usesAnimatedTextColor : FALSE;
}

BOOL TextEffect_UsesSharedEffectBuffer(TextEffectType type) {
    const TextEffectDefinition* effect = TextEffect_GetByType(type);
    return effect ? effect->usesSharedEffectBuffer : FALSE;
}
