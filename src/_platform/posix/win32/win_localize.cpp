// win_localize.cpp -- localization for the POSIX/Android build.
//
// Ported from src/win32/win_localize.cpp. The original reads localization.txt
// out of the game files through FS_FileOpenReadText, which is the engine's own
// portable file layer, so the whole thing works unchanged -- no registry, no
// Windows path handling. The only edit is the file handle type spelled out:
// the Win32 build wrote _iobuf*, which is MSVCRT's internal name for FILE*.
//
// The language choice therefore comes from the game's own localization.txt,
// exactly as on Windows. That is deliberate: it keeps the language in sync with
// whatever the install actually ships, instead of guessing from LANG and then
// failing to find the matching string table.

#include <universal/q_shared.h>
#include "win_localize.h"
#include <universal/q_parse.h>
#include <stringed/stringed_hooks.h>
#include <qcommon/com_fileaccess.h>

static LocalizationData localization;

#define LANGUAGE_BUF_SIZE 0x1000
static char language_buffer[LANGUAGE_BUF_SIZE];

char *__cdecl Win_CopyLocalizationString(const char *string)
{
    return va("%s", string);
}

char *__cdecl Win_GetLanguage()
{
    if (!localization.language)
        MyAssertHandler(".\\win32\\win_localize.cpp", 145, 0, "%s", "localization.language");
    return localization.language;
}

int __cdecl Win_InitLocalization()
{
    signed int size;
    int sizea;
    FILE *fp;
    int i;
    int lang;

    localization.language = 0;
    localization.strings = 0;
    fp = FS_FileOpenReadText("localization.txt");

    if (!fp)
    {
        // LWSS: no localization.txt means the working directory is wrong or the
        // files were not copied over. Same message the Win32 build gives.
        iassert(0);
        return 0;
    }

    size = FS_FileGetFileSize(fp);

    if (size >= LANGUAGE_BUF_SIZE)
        MyAssertHandler(".\\win32\\win_localize.cpp", 44, 0, "%s", "size < LANGUAGE_BUF_SIZE");

    localization.language = language_buffer;
    sizea = FS_FileRead(language_buffer, size, fp);
    FS_FileClose(fp);
    if (sizea)
    {
        localization.language[sizea] = 0;
        lang = 0;
        for (i = 0; localization.language[i]; ++i)
        {
            if (localization.language[i] == 10)
            {
                localization.language[i] = 0;
                localization.strings = &localization.language[i + 1];
                SEH_GetLanguageIndexForName(localization.language, &lang);
                return lang;
            }
        }
        return lang;
    }
    else
    {
        localization.language = 0;
        return 0;
    }
}

char *__cdecl Win_LocalizeRef(const char *ref)
{
    const char *v1;
    const char *v3;
    const char *strings;
    int useRef;
    const char *token;

    Com_BeginParseSession("localization");
    strings = localization.strings;
    do
    {
        token = (const char *)Com_Parse(&strings);
        if (!*token)
        {
            Com_EndParseSession();
            if (!alwaysfails)
            {
                v1 = va("unlocalized: %s", ref);
                MyAssertHandler(".\\win32\\win_localize.cpp", 117, 0, v1);
            }
            return Win_CopyLocalizationString(ref);
        }
        useRef = strcmp(token, ref) == 0;
        token = (const char *)Com_Parse(&strings);
        if (!*token)
        {
            Com_EndParseSession();
            if (!alwaysfails)
            {
                v3 = va("missing value: %s", ref);
                MyAssertHandler(".\\win32\\win_localize.cpp", 126, 0, v3);
            }
            return Win_CopyLocalizationString(ref);
        }
    } while (!useRef);
    Com_EndParseSession();
    return Win_CopyLocalizationString(token);
}

void __cdecl Win_ShutdownLocalization()
{
    localization.language = 0;
    localization.strings = 0;
}
