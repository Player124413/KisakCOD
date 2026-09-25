// win_localize.h -- localization table.
//
// The Win32 build reads the language from the registry and loads a .csv of
// translated strings. On POSIX there is no registry, so the language comes from
// the host (GameSettings on Android, LANG on Linux) and the strings are loaded
// from the same game files. The interface is unchanged.

#pragma once

struct LocalizationData // sizeof=0x8
{
    char *language;
    char *strings;
};

char *__cdecl Win_CopyLocalizationString(const char *string);
char *__cdecl Win_GetLanguage();
int __cdecl Win_InitLocalization();
char *__cdecl Win_LocalizeRef(const char *ref);
void __cdecl Win_ShutdownLocalization();
