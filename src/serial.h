#ifndef _SERIAL_H_
#define _SERIAL_H_

#ifdef __MINGW32__
#  include <windows.h>
typedef HANDLE serial_t;
#else
typedef int serial_t;
#endif

#define SERIAL_INVALID (serial_t)(-1)

/* Open a serial port with the given speed and return a handle that can be used
 * in the other serial APIs*/
serial_t serial_open(const char *path, long bps);

/* Read up to len bytes (non-blocking), length read is returned */
ssize_t serial_read(serial_t serial, void *buf, size_t len);

/* Read up to len bytes, or until to_ms milliseconds have elapsed, length read
 * is returned */
ssize_t serial_read_to(serial_t serial, void *buf, size_t len, unsigned int to_ms);

/* Write len bytes onto the serial port, actual number of bytes written is
 * returned */
ssize_t serial_write(serial_t serial, const void *buf, size_t len);

/* Close the serial file */
int serial_close(serial_t serial);

#endif /* _SERIAL_H_ */
