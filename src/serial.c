#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "serial.h"

int
serial_open(const char *path, speed_t bps)
{
    int fd;
    struct termios otio;
    struct termios ntio;

    fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        return -1;
    }

    tcgetattr(fd, &otio);
    tcgetattr(fd, &ntio);

    cfsetispeed(&ntio, bps);
    cfsetospeed(&ntio, bps);

    /* https://www.cmrr.umn.edu/~strupp/serial.html */
    /* 8N1 */
    ntio.c_cflag |= (CLOCAL | CREAD);
    ntio.c_cflag &= ~(CSIZE|PARENB|CSTOPB);
    ntio.c_cflag |=  (CS8); /* Set size after setting CSIZE mask */
    ntio.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXOFF|IXANY);
    ntio.c_oflag &= ~(OPOST);
    ntio.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);

    ntio.c_cc[VINTR]    = 0;     /* Ctrl-c */
    ntio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
    ntio.c_cc[VERASE]   = 0;     /* del */
    ntio.c_cc[VKILL]    = 0;     /* @ */
    ntio.c_cc[VEOF]     = 4;     /* Ctrl-d */
    ntio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
    ntio.c_cc[VMIN]     = 0;     /* non-blocking */
    ntio.c_cc[VSWTC]    = 0;     /* '\0' */
    ntio.c_cc[VSTART]   = 0;     /* Ctrl-q */
    ntio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
    ntio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
    ntio.c_cc[VEOL]     = 0;     /* '\0' */
    ntio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
    ntio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
    ntio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
    ntio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
    ntio.c_cc[VEOL2]    = 0;     /* '\0' */

    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &ntio);

    return fd;
}
