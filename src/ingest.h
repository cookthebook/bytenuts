#ifndef _INGEST_H_
#define _INGEST_H_

#ifdef __MINGW32__
#  include <curses.h>
#else
#  include <ncurses.h>
#endif
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include "bstr.h"
#include "bytenuts.h"

enum ingest_mode_enum {
    INGEST_MODE_NORMAL = 0,
    INGEST_MODE_XMODEM,
    INGEST_MODE_XMODEM1K,
    INGEST_MODE_HEX,
};

typedef struct ingest_struct {
    WINDOW *input;
    pthread_mutex_t *term_lock;
    pthread_t thr;
    volatile int running;
    char *prepend; /* string to be prepended in the prompt */
    char inbuf[4096];
    int inpos; /* cursor position within the inbuf itself */
    int inlen; /* length of the input buffer */
    char **history; /* array of strings representing the command history */
    char *history_filename; /* realpath of inbuf.pid.log */
    FILE *history_fd;
    char tmp_history[4096]; /* copy of inbuf before we load in history */
    int history_len;
    int history_pos; /* where we currently reside in the history */
    int mode;
    bytenuts_config_t *config;
#define INGEST_COMMAND_PGSZ (10)
    /* command pages loaded on startup */
    struct {
        char *cmds[INGEST_COMMAND_PGSZ];
        int cmds_n;
    } *cmd_pgs;
    int cmd_pgs_n;
    /* what command page is currently selected */
    int cmd_pg_cur;
    struct timespec cmd_ts; /* last command sent timestamp */
    bstr_history_handle xmodem_hist; /* xmodem filename transfer history */
} ingest_t;

/* startup the input window thread */
int ingest_start(bytenuts_t *bytenuts);

/* stop the input window thread */
int ingest_stop();

/* refresh the inbuf window */
int ingest_refresh();

/* Set the the command history directly. Values are copied */
int ingest_set_history(char **history, int history_len);

#endif /* _INGEST_H_ */
