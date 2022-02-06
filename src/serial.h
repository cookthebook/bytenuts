#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <termios.h>

int serial_open(const char *path, speed_t bps);

#endif /* _SERIAL_H_ */
