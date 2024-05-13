#ifndef _BSTR_H_
#define _BSTR_H_

#include <stdarg.h>
#include <stdio.h>

typedef struct bstr_history_struct * bstr_history_handle;


/* printf onto the end of base, reallocating and returning that buffer
 * can provide NULL for no base */
__attribute__((format(printf, 2, 3)))
char *bstr_print(char *base, const char *fmt, ...);

/* Create a new bstr_history struct. The position of the history will start as
 * unset. */
bstr_history_handle bstr_history_create(void);

/* return the string at the current history position (NULL if empty or position
 * is not set) */
const char *bstr_history_atpos(bstr_history_handle hist);

/* Put a new entry into the history */
void bstr_history_new_entry(bstr_history_handle hist, const char *str);

/* Move the postition index one entry older. If the history position is unset,
 * it is set to the newest entry. */
void bstr_history_older(bstr_history_handle hist);

/* Move the position index one entry newer. If the position is at the newest
 * entry, it becomes unset. If the position is unset, it stays unset. */
void bstr_history_newer(bstr_history_handle hist);

/* Unset the history position index */
void bstr_history_unset_pos(bstr_history_handle hist);

/* destroy/free a bstr_history */
void bstr_history_destroy(bstr_history_handle hist);

#endif /* _BSTR_H_ */
