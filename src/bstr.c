#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "bstr.h"

typedef struct bstr_history_struct {
    char **strs; /* strings in our history, in reverse chronological order */
    int len; /* number of strings in the history */
    int pos; /* current position index in the history */
} bstr_history_t;

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

bstr_history_handle
bstr_history_create(void)
{
    bstr_history_t *ret = calloc(1, sizeof(bstr_history_t));
    ret->pos = -1;
    return ret;
}

const char *
bstr_history_atpos(bstr_history_handle hist)
{
    if ((hist->len == 0) || (hist->pos < 0)) {
        return NULL;
    } else {
        return hist->strs[hist->pos];
    }
}

void
bstr_history_new_entry(bstr_history_handle hist, const char *str)
{
    /* stored in reverse chronological order */
    hist->len++;
    hist->strs = realloc(hist->strs, sizeof(char *) * hist->len);
    hist->strs[hist->len-1] = strdup(str);
}

void
bstr_history_older(bstr_history_handle hist)
{
    if (hist->len == 0)
        return;

    if (hist->pos == -1) {
        hist->pos = hist->len - 1;
    } else {
        hist->pos--;
    }

    if (hist->pos < 0)
        hist->pos = 0;
}

void
bstr_history_newer(bstr_history_handle hist)
{
    if ((hist->len == 0) || (hist->pos == -1))
        return;

    hist->pos++;
    if (hist->pos >= hist->len)
        hist->pos = -1;
}

void
bstr_history_unset_pos(bstr_history_handle hist)
{
    hist->pos = -1;
}

void
bstr_history_destroy(bstr_history_handle hist)
{
    if (!hist)
        return;

    if (hist->len > 0) {
        for (int i = 0; i < hist->len; i++) {
            free(hist->strs[i]);
        }
        free(hist->strs);
    }
}
