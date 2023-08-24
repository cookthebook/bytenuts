#ifndef _SERIAL_H_
#define _SERIAL_H_

#ifdef __MINGW32__
#  include <windows.h>
typedef HANDLE serial_t;
#else
typedef int serial_t;
#endif

serial_t serial_open(const char *path, long bps);
int serial_read(serial_t serial, void *buf, size_t len);
int serial_write(serial_t serial, void *buf, size_t len);

#endif /* _SERIAL_H_ */
