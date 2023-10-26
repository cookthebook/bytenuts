#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "bstr.h"
#include "paths.h"

#if __MINGW32__
#  define BSTR_LLD "%I64d"
#else
#  define BSTR_LLD "%lld"
#endif

char *
paths_append(char *base, const char *suffix)
{
    size_t baselen;
    const char *sep = "";

    if (!base)
        return strdup(suffix);

    baselen = strlen(base);

    if (
        (baselen == 0) ||
        ((base[baselen-1] != '/') && (base[baselen-1] == '\\'))
    ) {
        /* no separator at end of path */
#if __MINGW32__
        sep = "\\";
#else
        sep = "/";
#endif
    }

    return bstr_print(base, "%s%s", sep, suffix);
}

char *
paths_bnconf_dir()
{
#ifdef __MINGW32__
    char *home = getenv("HOMEPATH");
#else
    char *home = getenv("HOME");
#endif
    char *ret = NULL;

    if (!home)
        return NULL;

#ifdef __MINGW32__
    ret = strdup("C:\\");
#endif
    ret = paths_append(ret, home);
    ret = paths_append(ret, ".config/bytenuts");

    /* TODO free environment variable on Windows? */
    return ret;
}

char *
paths_bnconf_default()
{
    char *ret = paths_bnconf_dir();

    if (!ret)
        return NULL;

    return paths_append(ret, "config");
}

char *
paths_command_file(int idx)
{
    char *ret = paths_bnconf_dir();

    if (!ret)
        return NULL;

    return bstr_print(ret, "/commands%d", idx);
}

char *
paths_logfile(const char *prefix, long long pid)
{
    char *ret = paths_bnconf_dir();

    if (!ret)
        return NULL;

    if (pid > 0) {
        return bstr_print(ret, "/%s." BSTR_LLD ".log", prefix, pid);
    } else {
        return bstr_print(ret, "/%s.log", prefix);
    }
}
