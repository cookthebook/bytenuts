#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "xmodem.h"

int _xmodem_wait(int fd, uint8_t *ret, int timeout_ms);

uint16_t _xmodem_crc(void *buf, size_t len);
uint8_t _xmodem_csum(void *buf, size_t len);
void _xmodem_status_print(size_t sent, size_t total);

int
xmodem_send(
    int dest_fd,
    int in_fd,
    size_t sz,
    int block_sz,
    void (*status_cb)(size_t sent, size_t total)
)
{
    uint8_t start_byte;
    size_t sent;
    uint8_t *buf;
    size_t buf_sz;
    uint8_t packet_num;
    int retries;
    int ret = 0;

    if (block_sz != 128 && block_sz != 1024) {
        return -1;
    }

    /* wait for start byte, 'C' for CRC, NAK for CSUM */
    for (int i = 0; i < 11; i++) {
        if (i == 10) {
            return XMODEM_ERR_TIMEOUT;
        }

        if (
            !_xmodem_wait(dest_fd, &start_byte, XMODEM_TIMEOUT) &&
            (start_byte == XMODEM_CRC || start_byte == XMODEM_NAK)
        ) {
            break;
        }
    }

    /* start sendin' bytes */
    buf_sz = 3 + block_sz; /* SOH/STX | idx | ~idx | payload */
    if (start_byte == XMODEM_NAK) {
        buf_sz += 1;
    } else {
        buf_sz += 2;
    }
    buf = malloc(buf_sz);
    sent = 0;
    packet_num = 1; /* yes, this starts at 1 */

    while (sent < sz) {
        size_t read_len;

        if (block_sz == 128) {
            buf[0] = XMODEM_SOH;
        } else {
            buf[0] = XMODEM_STX;
        }

        buf[1] = packet_num;
        buf[2] = ~packet_num;

        if ((sz - sent) >= block_sz) {
            read_len = block_sz;
        } else {
            /* back fill payload with pad */
            read_len = sz - sent;
            for (int i = read_len; i < block_sz; i++) {
                buf[i] = XMODEM_PAD;
            }
        }

        if (read(in_fd, &buf[3], read_len) != read_len) {
            return XMODEM_ERR_FILEIO;
        }

        if (start_byte == XMODEM_CRC) {
            /* write CRC big endian */
            uint16_t crc = _xmodem_crc(&buf[3], block_sz);
            buf[buf_sz-2] = (uint8_t)((0xFF00 & crc) >> 8);
            buf[buf_sz-1] = (uint8_t)(0x00FF & crc);
        } else {
            buf[buf_sz-1] = _xmodem_csum(&buf[3], block_sz);
        }

        /* try to send this packet until we get an ACK */
        retries = 0;
        while (1) {
            uint8_t recv_char;

            if (retries == 10) {
                ret = XMODEM_ERR_TIMEOUT;
                goto end_of_transmission;
            }

            if (write(dest_fd, buf, buf_sz) != buf_sz){
                return XMODEM_ERR_WRITE;
            }

            if (_xmodem_wait(dest_fd, &recv_char, XMODEM_TIMEOUT)) {
                return XMODEM_ERR_READ;
            }

            if (recv_char == XMODEM_ACK) {
                break;
            }

            retries++;
        }

        sent += read_len;
        packet_num++; /* expect overflow */
        status_cb(sent, sz);
    }

end_of_transmission:
    retries = 0;
    while (1) {
        uint8_t ack_char;

        if (retries == 10) {
            return XMODEM_ERR_TIMEOUT;
        }

        write(dest_fd, &(uint8_t){XMODEM_EOT}, 1);

        if (
            !_xmodem_wait(dest_fd, &ack_char, XMODEM_TIMEOUT) &&
            ack_char == XMODEM_ACK
        ) {
            break;
        }

        retries++;
    }

    return ret;
}

int
_xmodem_wait(int fd, uint8_t *ret, int timeout_ms)
{
    struct pollfd pfd;

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    if (poll(&pfd, 1, timeout_ms) <= 0) {
        return -1;
    }

    read(fd, ret, 1);

    return 0;
}

uint16_t
_xmodem_crc(void *buf, size_t len)
{
    uint16_t crc;
    char i;

    crc = 0;
    while (--len >= 0)
    {
        crc = crc ^ (uint16_t) (*(uint8_t *)buf++) << 8;
        i = 8;
        do
        {
            if (crc & 0x8000)
                crc = crc << 1 ^ 0x1021;
            else
                crc = crc << 1;
        } while(--i);
    }

    return (crc);
}

uint8_t
_xmodem_csum(void *buf, size_t len)
{
    uint8_t ret = 0;

    for (size_t i = 0; i < len; i++) {
        ret += ((uint8_t *)buf)[i];
    }

    return ret;
}
