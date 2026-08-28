// Android port — localization stub (replaces win32/win_localize.h)
#pragma once

#include <cstdint>

void Locale_Init();
void Locale_Shutdown();
const char *Locale_GetString(const char *key);
const char *Locale_GetLanguage();
int Locale_GetLanguageIndex();