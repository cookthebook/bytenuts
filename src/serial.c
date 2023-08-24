#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "serial.h"

#ifdef __MINGW32__
#  include <windows.h>
serial_t
serial_open(const char *comport, long bps)
{
    DCB dcb = { 0 };
    HANDLE serial = CreateFile(
        comport,
        GENERIC_READ | GENERIC_WRITE,
        0,
        0,
        OPEN_EXISTING,
        0,
        0
    );

    if (serial == INVALID_HANDLE_VALUE) {
        return -1;
    }

    /* TODO
     * reference: https://github.com/waynix/SPinGW/tree/master */
    return serial;
}
#else
#  include <termios.h>
#  include <poll.h>

static speed_t long_to_speed(long bps);

serial_t
serial_open(const char *path, long bps)
{
    speed_t speed = long_to_speed(bps);
    int fd;
    struct termios otio;
    struct termios ntio;

    fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        return -1;
    }

    tcgetattr(fd, &otio);
    tcgetattr(fd, &ntio);

    cfsetispeed(&ntio, speed);
    cfsetospeed(&ntio, speed);

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

int
serial_read(serial_t serial, void *buf, size_t len)
{
    struct pollfd fds;
    fds.fd = serial;
    fds.events = POLLIN;
    fds.revents = 0;

    if (poll(&fds, 1, 1) == 0) {
        return 0;
    }

    return (int)read(serial, buf, len);
}

int
serial_write(serial_t serial, void *buf, size_t len)
{
    return (int)write(serial, buf, len);
}

static speed_t
long_to_speed(long bps)
{
    switch (bps) {
    case 50:
        return B50;
    case 75:
        return B75;
    case 110:
        return B110;
    case 134:
        return B134;
    case 150:
        return B150;
    case 200:
        return B200;
    case 300:
        return B300;
    case 600:
        return B600;
    case 1200:
        return B1200;
    case 1800:
        return B1800;
    case 2400:
        return B2400;
    case 4800:
        return B4800;
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 500000:
        return B500000;
    case 576000:
        return B576000;
    case 921600:
        return B921600;
    case 1000000:
        return B1000000;
    case 1152000:
        return B1152000;
    case 1500000:
        return B1500000;
    case 2000000:
        return B2000000;
    case 2500000:
        return B2500000;
    case 3000000:
        return B3000000;
    case 3500000:
        return B3500000;
    case 4000000:
        return B4000000;
    default:
        return B0;
    }
}

#endif /* __MINGW32__ */
