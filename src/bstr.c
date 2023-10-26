#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bstr.h"

char *
bstr_print(char *base, const char *fmt, ...)
{
    char *new_buf;
    size_t new_len;
    va_list ap;

    va_start(ap, fmt);
    new_len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    new_buf = calloc(1, new_len + 1);

    va_start(ap, fmt);
    vsprintf(new_buf, fmt, ap);
    va_end(ap);

    if (!base) {
        return new_buf;
    } else {
        size_t baselen = strlen(base);

        base = realloc(base, baselen + new_len + 1);
        memcpy(&base[baselen], new_buf, new_len);
        base[baselen + new_len] = '\0';

        free(new_buf);
        return base;
    }
}
