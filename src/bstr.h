#ifndef _BSTR_H_
#define _BSTR_H_

#include <stdarg.h>
#include <stdio.h>

/* printf onto the end of base, reallocating and returning that buffer
 * can provide NULL for no base */
__attribute__((format(printf, 2, 3)))
char *bstr_print(char *base, const char *fmt, ...);

#endif /* _BSTR_H_ */
