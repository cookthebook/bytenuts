#ifndef _BYTENUTES_H_
#define _BYTENUTES_H_

#ifdef __MINGW32__
#  include <curses.h>
#else
#  include <ncurses.h>
#endif
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "serial.h"

/* https://en.wikipedia.org/wiki/Control_character#How_control_characters_map_to_keyboards */
#define CTRL(c) ((c)&31)

typedef struct bytenuts_state_struct {
    char **history; /* commands from previous session */
    int history_len;
    char *buf; /* previous buffer */
    size_t buf_len;
} bytenuts_state_t;

typedef struct bytenuts_config_struct {
    int colors; /* enable terminal colors, default 1 */
    int echo; /* echo user input to the output window, default 0 */
    int no_crlf; /* only send LF line endings, not CRLF, default 0 */
    char escape; /* which character is used for escape sequence */
    long baud; /* baud rate */
    char *config_path; /* config file path */
    char *log_path; /* path to the log file (if it exists) */
    char *serial_path; /* path to the target serial device */
    uint32_t inter_cmd_to; /* inter command timeout in ms */
} bytenuts_config_t;

#define CONFIG_DEFAULT (bytenuts_config_t){                                    \
    .colors = 1,                                                               \
    .echo = 0,                                                                 \
    .no_crlf = 0,                                                              \
    .escape = 'b',                                                             \
    .baud = 115200,                                                            \
    .config_path = NULL,                                                       \
    .log_path = NULL,                                                          \
    .serial_path = NULL,                                                       \
    .inter_cmd_to = 10,                                                        \
}

typedef struct bytenuts_struct {
    serial_t serial_fd;
    bytenuts_config_t config;
    int config_overrides[5];
    int resume;
    bytenuts_state_t state;
    WINDOW *status_win;
    WINDOW *out_win;
    WINDOW *in_win;
    pthread_mutex_t lock;
    pthread_mutex_t term_lock;
    pthread_cond_t stop_cond;
    pthread_t in_thr;
    pthread_t out_thr;
    char *bytenuts_status;
    char *ingest_status;
    char *cheerios_status;
    char *cmdpg_status;
} bytenuts_t;

/* startup the application */
int bytenuts_run(int argc, char **argv);

/* cleanly stop threads */
int bytenuts_stop();

/* perform kill routing for exiting */
void bytenuts_kill();

/* print some bytenuts stats to the output window */
int bytenuts_print_stats();

#define STATUS_BYTENUTS (0)
#define STATUS_INGEST   (1)
#define STATUS_CHEERIOS (2)
#define STATUS_CMDPAGE  (3)
/* set the status for the given thread */
int bytenuts_set_status(int user, const char *fmt, ...);

int bytenuts_update_screen_size();

#endif /* _BYTENUTES_H_ */
