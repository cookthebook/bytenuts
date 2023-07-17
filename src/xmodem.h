#ifndef _XMODEM_H_
#define _XMODEM_H_

#include <stdio.h>

#include "serial.h"

#define XMODEM_SOH           (0x01)
#define XMODEM_STX           (0x02)
#define XMODEM_EOT           (0x04)
#define XMODEM_ACK           (0x06)
#define XMODEM_NAK           (0x15)
#define XMODEM_ETB           (0x17)
#define XMODEM_CAN           (0x18)
#define XMODEM_CRC           (0x43) /* 'C' */
#define XMODEM_PAD           (0x1A)

#define XMODEM_TIMEOUT       (10000) /* in ms */

#define XMODEM_ERR_TIMEOUT   (1) /* ACK timeout */
#define XMODEM_ERR_WRITE     (2) /* Failed to write to file descriptor */
#define XMODEM_ERR_FILEIO    (3) /* Error on reading from file descriptor */
#define XMODEM_ERR_READ      (4) /* Failed to read from the serial connection */
#define XMODEM_ERR_CANCEL    (5) /* Sender cancelled the transmission */
#define XMODEM_ERR_DONE      (6) /* Sender finished the transmission */

/*
 * Following this setup: https://pythonhosted.org/xmodem/xmodem.html
 * Choose to send `sz` bytes from `in_fd` to `dest_fd` via XModem with a payload
 * length of `block_sz`.
 */
int xmodem_send(
    int dest_fd,
    int in_fd,
    size_t sz,
    int block_sz,
    void (*status_cb)(size_t sent, size_t total, int ack_fails)
);

#if 0
/* Receive an xmodem message
 *
 * If sz is non-negative, then only that many bytes will actually be written
 * to fd. But the entire xmodem transfer will be completed */
int xmodem_recv(int in_fd, int dest_fd, ssize_t sz);
#endif

#endif /* _XMODEM_H_ */
