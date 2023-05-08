#ifndef _CHEERIOS_H_
#define _CHEERIOS_H_

#include <ncurses.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

#include "bytenuts.h"

typedef struct line_buffer_struct {
    uint8_t **lines;
    int *line_lens;
    int n_lines; /* how many lines do we have */
    int pos; /* position of the cursor in the current line */
    int bot; /* index of the bottom line shown */
#define NCOLOR_PAIRS (8)
    /* support 8 color pairs on screen at once.
     * COLOR_PAIR(pair_pos + 2) will get you your color. */
    struct { uint8_t fg; uint8_t bg; } color_pairs[NCOLOR_PAIRS];
    uint8_t enabled_pairs[NCOLOR_PAIRS]; /* which pairs are enabled */
    int color_pos;
} line_buffer_t;

enum cheerios_mode_enum {
    CHEERIOS_MODE_NORMAL = 0,
    CHEERIOS_MODE_PAUSED,
};

typedef struct cheerios_struct {
    pthread_mutex_t lock;
    volatile int running;
    pthread_t thr;
    WINDOW *output;
    pthread_mutex_t *term_lock;
    int ser_fd;
    line_buffer_t lines;
    FILE *log; /* log file which was opened with -l */
    char *backup_filename; /* realpath to the backup outbuf.pid.log */
    FILE *backup; /* backup log file object */
    bytenuts_config_t *config;
    volatile int mode;
    pthread_cond_t cond;
} cheerios_t;

/* startup the output window thread */
int cheerios_start(bytenuts_t *bytenuts);

/* pause reading from the device */
int cheerios_pause();

/* resume reading from the device */
int cheerios_resume();

/* go back in the log history, this also stops the buffer from scrolling down
 * if negative, jump to the back of the log */
int cheerios_goback(int lines);

/* go forward in the log history
 * if a negative number is provided, go back to the start and resume scrolling */
int cheerios_gofwd(int lines);

/* stop the thread and release memory */
int cheerios_stop();

/* send over a line of user input to the output */
int cheerios_input(const char *buf, size_t len);

/* output a line only to the terminal for info/prompt purposes
 * do not terminate with a newline! */
int cheerios_info(const char *line);

/* Like cheerios_insert except using printf formatting */
__attribute__((format(printf, 1, 2)))
int cheerios_print(const char *fmt, ...);

/* directly insert a buffer to the output window */
int cheerios_insert(const char *buf, size_t len);

/* send the file at path over xmodem */
int cheerios_xmodem(const char *path, int block_sz);

int cheerios_print_stats();

/* getter for window height */
int cheerios_getmaxy();
/* getter for window width */
int cheerios_getmaxx();

/* deletes the old window and sets the output window to the new one */
int cheerios_set_window(WINDOW *win);

#endif /* _CHEERIOS_H_ */
