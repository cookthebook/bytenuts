#include <libgen.h>
#ifdef __MINGW32__
#  include <curses.h>
#else
#  include <ncurses.h>
#endif
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "bytenuts.h"
#include "cheerios.h"
#include "ingest.h"
#include "timer_math.h"

static void *ingest_thread(void *arg);

static ingest_t ingest;

static int mode_normal(int ch);
static int mode_xmodem(int block_sz);
static int mode_hex(int ch);
static int handle_functions(int ch);
static int print_stats(void);
static int auto_complete(void);
static int read_cmd_page(const char *home_dir, int idx);
static void update_cmd_pg_status(void);
static void inter_command_wait(void);

int
ingest_start(bytenuts_t *bytenuts)
{
    char *HOME;

    memset(&ingest, 0, sizeof(ingest));

    ingest.input = bytenuts->in_win;
    ingest.term_lock = &bytenuts->term_lock;
    ingest.config = &bytenuts->config;
    ingest.cmd_pg_cur = -1;
    ingest.xmodem_hist = bstr_history_create();

    HOME = getenv("HOME");
    if (HOME) {
        int idx = 0;
        int name_len;
        pid_t pid;

        while (!read_cmd_page(HOME, idx)) {
            idx++;
        }

        /* ensure that a process has a unique log */
        pid = getpid();
        name_len = snprintf(
            NULL, 0,
            "%s/.config/bytenuts/inbuf.%lld.log",
            HOME, (long long)pid
        );
        ingest.history_filename = calloc(1, name_len + 1);
        snprintf(
            ingest.history_filename, name_len + 1,
            "%s/.config/bytenuts/inbuf.%lld.log",
            HOME, (long long)pid
        );

        ingest.history_fd = fopen(ingest.history_filename, "w");
    }

    if (ingest.cmd_pgs_n > 0) {
        ingest.cmd_pg_cur = 0;
    }
    update_cmd_pg_status();

    clock_gettime(CLOCK_REALTIME, &ingest.cmd_ts);
    /* ensure first command is not delayed */
    timer_sub_ms(&ingest.cmd_ts, ingest.config->inter_cmd_to);

    ingest.running = 1;
    if (pthread_create(&ingest.thr, NULL, ingest_thread, NULL)) {
        return -1;
    }

    return 0;
}

int
ingest_stop()
{
    ingest.running = 0;
    pthread_join(ingest.thr, NULL);
    return 0;
}

int
ingest_refresh()
{
    static int start = 0;
    int startx = 0, starty = 0;
    int cx = -1, cy = -1;
    int win_len = getmaxx(ingest.input) + 1;

    pthread_mutex_lock(ingest.term_lock);

    werase(ingest.input);
    wmove(ingest.input, 0, 0);
    if (ingest.prepend) {
        wprintw(ingest.input, "%s", ingest.prepend);
        getyx(ingest.input, starty, startx);
    }

    if (start > ingest.inpos) {
        start = ingest.inpos;
    }
    /* this is not perfect as some characters are wider than one space, but it
     * makes the algorithm for printing less redundant */
    else if (ingest.inpos > (start + win_len)) {
        start = ingest.inpos - win_len;
    }

    int i = 0;
    int hit_cursor = 0;
    while (1) {
        int p = i + start;
        int iy, ix;
        i++;

        getyx(ingest.input, iy, ix);

        /* print the cursor at this position */
        if (p == ingest.inpos) {
            cy = iy;
            cx = ix;
            hit_cursor = 1;
        }

        /* nothing left to write... */
        if (p >= ingest.inlen)
            break;

        /* we want to leave a blank character for printing the cursor at the end of the line */
        if (ix >= (win_len-2)) {
            /* we printed the cursor, so we can break */
            if (hit_cursor)
                break;
            /* otherwise, we didn't print the cursor, so start over */
            i = 0;
            start++;
            wmove(ingest.input, starty, startx);
            continue;
        }

        waddch(ingest.input, ingest.inbuf[p]);
    }

    if (cx < 0) {
        cx = ingest.inlen < win_len ? ingest.inlen : win_len - 1;
        cy = 0;
    }

    wmove(ingest.input, cy, cx);
    wrefresh(ingest.input);

    pthread_mutex_unlock(ingest.term_lock);

    return 0;
}

int
ingest_set_history(char **history, int history_len)
{
    if (ingest.history) {
        for (int i = 0; ingest.history[i] != NULL; i++) {
            free(ingest.history[i]);
        }
        free(ingest.history);
    }

    ingest.history = calloc(1, sizeof(char *) * (history_len+1));
    for (int i = 0; i < history_len; i++) {
        size_t item_len = strlen(history[i]);
        ingest.history[i] = strdup(history[i]);

        while (
            ingest.history[i][item_len-1] == '\n' ||
            ingest.history[i][item_len-1] == '\r'
        ) {
            item_len--;
            ingest.history[i][item_len] = '\0';
        }

        if (ingest.history_fd) {
            fwrite(ingest.history[i], 1, item_len, ingest.history_fd);
            fwrite("\n", 1, 1, ingest.history_fd);
        }
    }

    ingest.history_len = history_len;
    ingest.history_pos = ingest.history_len;

    return 0;
}

static void *
ingest_thread(void *arg)
{
    int ch;

    memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
    ingest.inpos = 0;

    bytenuts_set_status(STATUS_INGEST, "normal");

    while (ingest.running) {
        pthread_mutex_lock(ingest.term_lock);
        ch = wgetch(ingest.input);
        pthread_mutex_unlock(ingest.term_lock);

        if (ch == ERR) {
            nanosleep(&(struct timespec){ 0, 100000 }, NULL);
            continue;
        }

        if (ch == CTRL(ingest.config->escape)) {
            int should_quit = 0;
            int should_continue = 0;

            bytenuts_set_status(STATUS_INGEST, "control");

            while (1) {
                pthread_mutex_lock(ingest.term_lock);
                ch = wgetch(ingest.input);
                pthread_mutex_unlock(ingest.term_lock);

                if (ch != ERR)
                    break;
                nanosleep(&(struct timespec){ 0, 100000 }, NULL);
            }

            switch (ch) {
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '0':
            {
                bytenuts_set_status(STATUS_INGEST, "normal");
                should_continue = 1;

                if (ingest.cmd_pgs_n == 0)
                    break;

                int idx;
                char *toload;
                size_t toload_len;
                char **cmds = ingest.cmd_pgs[ingest.cmd_pg_cur].cmds;
                int cmds_n = ingest.cmd_pgs[ingest.cmd_pg_cur].cmds_n;

                if (ch == '0')
                    ch += 10;
                idx = ch - '1';

                if (idx >= cmds_n)
                    break;

                toload = cmds[idx];
                toload_len = strlen(toload);

                memcpy(ingest.inbuf, toload, toload_len);
                ingest.inpos = toload_len;
                ingest.inlen = toload_len;
                ingest_refresh();
                break;
            }
            case 'c':
            {
                bytenuts_set_status(STATUS_INGEST, "normal");
                should_continue = 1;

                if (ingest.cmd_pgs_n == 0) {
                    cheerios_insert("No quick commands\r\n", 19);
                    break;
                }

                char **cmds = ingest.cmd_pgs[ingest.cmd_pg_cur].cmds;
                int cmds_n = ingest.cmd_pgs[ingest.cmd_pg_cur].cmds_n;
                char *cmd_str;
                size_t cmd_str_len;

                cmd_str_len = snprintf(
                    NULL, 0,
                    "Quick Commands (pg.%d of %d):\r\n",
                    ingest.cmd_pg_cur + 1,
                    ingest.cmd_pgs_n
                );
                cmd_str = calloc(1, cmd_str_len + 1);
                sprintf(
                    cmd_str,
                    "Quick Commands (pg.%d of %d):\r\n",
                    ingest.cmd_pg_cur + 1,
                    ingest.cmd_pgs_n
                );

                cheerios_insert(cmd_str, cmd_str_len);
                for (int i = 0; i < cmds_n; i++) {

                    cmd_str_len = snprintf(
                        NULL, 0,
                        "%4d: %s\r\n",
                        i+1, cmds[i]
                    );
                    cmd_str = calloc(1, cmd_str_len + 1);
                    sprintf(cmd_str, "%4d: %s\r\n", i+1, cmds[i]);

                    cheerios_insert(cmd_str, cmd_str_len);
                    free(cmd_str);
                }

                break;
            }
            case 'p':
            {
                bytenuts_set_status(STATUS_CMDPAGE, "selecting...");

                while (1) {
                    pthread_mutex_lock(ingest.term_lock);
                    ch = wgetch(ingest.input);
                    pthread_mutex_unlock(ingest.term_lock);

                    if (ch != ERR)
                        break;
                    nanosleep(&(struct timespec){ 0, 100000 }, NULL);
                }

                should_continue = 1;
                bytenuts_set_status(STATUS_INGEST, "normal");

                if (ch >= '1' || ch <= '9') {
                    int pg = ch - '1';

                    if (pg >= 0 && pg < ingest.cmd_pgs_n) {
                        ingest.cmd_pg_cur = pg;
                    }
                }

                update_cmd_pg_status();
                break;
            }
            case 'i':
                print_stats();
                bytenuts_set_status(STATUS_INGEST, "normal");
                should_continue = 1;
                break;
            case 'q':
                bytenuts_stop();
                should_quit = 1;
                break;
            case 'x':
                ingest.mode = INGEST_MODE_XMODEM;
                bytenuts_set_status(STATUS_INGEST, "xmodem");
                break;
            case 'X':
                ingest.mode = INGEST_MODE_XMODEM1K;
                bytenuts_set_status(STATUS_INGEST, "xmodem1k");
                break;
            case 'H':
                if (ingest.mode == INGEST_MODE_NORMAL) {
                    ingest.mode = INGEST_MODE_HEX;
                    bytenuts_set_status(STATUS_INGEST, "hex");
                } else {
                    ingest.mode = INGEST_MODE_NORMAL;
                    bytenuts_set_status(STATUS_INGEST, "normal");
                }
                should_continue = 1;
                break;
            case 'h':
                cheerios_print(
                    "Commands (lead with ctrl-%c):\r\n"
                    "  c: print available quick commands\r\n"
                    "  0-9: load the given quick command (0 is 10)\r\n"
                    "  p: select a different quick commands page\r\n"
                    "  i: view info/stats\r\n"
                    "  x: start XModem upload with 128B payloads\r\n"
                    "  X: start XModem upload with 1024B payloads\r\n"
                    "  H: enter/exit hex buffer mode\r\n"
                    "  h: view this help\r\n"
                    "  q: quit Bytenuts\r\n",
                    ingest.config->escape
                );
                bytenuts_set_status(STATUS_INGEST, "normal");
                should_continue = 1;
                break;
            default:
                bytenuts_set_status(STATUS_INGEST, "normal");
                should_continue = 1;
                break;
            }

            if (should_quit) {
                break;
            } else if (should_continue) {
                continue;
            }
        }

        switch (ingest.mode) {
        case INGEST_MODE_NORMAL:
            mode_normal(ch);
            break;
        case INGEST_MODE_XMODEM:
            mode_xmodem(128);
            break;
        case INGEST_MODE_XMODEM1K:
            mode_xmodem(1024);
            break;
        case INGEST_MODE_HEX:
            mode_hex(ch);
            break;
        default:
            break;
        }

        sched_yield();
    }

    if (ingest.history_fd) {
        char *in_filename;
        int in_filename_len;
        char *home = getenv("HOME");

        /* do not chdir as other threads can still be running */
        in_filename_len = snprintf(
            NULL, 0,
            "%s/.config/bytenuts/inbuf.log",
            home
        );
        in_filename = calloc(1, in_filename_len + 1);
        snprintf(
            in_filename, in_filename_len + 1,
            "%s/.config/bytenuts/inbuf.log",
            home
        );

        /* move this processes history to the path that can be loaded on resumption */
        fclose(ingest.history_fd);
        rename(ingest.history_filename, in_filename);

        free(in_filename);
        free(ingest.history_filename);
    }

    bstr_history_destroy(ingest.xmodem_hist);

    pthread_exit(NULL);
    return NULL;
}

/* Add an item to the input history, doing nothing if the line is the same as
 * the previous, and re-arranging history if the line was seen previously. */
static void
history_add(const char *line)
{
    ingest.history_pos = ingest.history_len;

    if (*line == '\0') {
        /* Line is empty. */
        return;
    }

    if (ingest.history_len != 0) {
        if (!strcmp(ingest.history[ingest.history_len - 1], line)) {
            /* Line is a repeat of the previous history item. */
            return;
        }

        for (unsigned i = 0; i < ingest.history_len - 1; i++) {
            if (!strcmp(ingest.history[i], line)) {
                /* Re-arrange history to move item to the front */
                char *history_item = ingest.history[i];
                memcpy(&ingest.history[i], &ingest.history[i+1],
                       (ingest.history_len - (i + 1)) * sizeof(ingest.history[0]));
                ingest.history[ingest.history_len - 1] = history_item;

                if (ingest.history_fd) {
                    fseek(ingest.history_fd, 0, SEEK_SET);
                    for (unsigned i = 0; i < ingest.history_len; i++) {
                        fwrite(ingest.history[i], 1, strlen(ingest.history[i]), ingest.history_fd);
                        fwrite("\n", 1, 1, ingest.history_fd);
                    }
                }

                return;
            }
        }
    }

    ingest.history_len++;
    ingest.history = realloc(ingest.history, ingest.history_len * sizeof(char *));
    ingest.history[ingest.history_pos] = strdup(ingest.inbuf);
    ingest.history_pos++;
    if (ingest.history_fd) {
        fwrite(line, 1, strlen(line), ingest.history_fd);
        fwrite("\n", 1, 1, ingest.history_fd);
    }
}

static int
mode_normal(int ch)
{
    if (handle_functions(ch))
        return 0;

    switch (ch) {
    case '\n': { /* send inputs */
        size_t inlen = strlen(ingest.inbuf);
        const char *ending;

        if (ingest.config->no_crlf)
            ending = "\n";
        else
            ending = "\r\n";

        inter_command_wait();
        cheerios_input(ingest.inbuf, inlen);
        cheerios_input(ending, strlen(ending));
        if (ingest.config->echo) {
            cheerios_insert(">> ", 3);
            cheerios_insert(ingest.inbuf, inlen);
            cheerios_insert("\r\n", 2);
        }

        /* store in history */
        history_add(ingest.inbuf);

        memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
        ingest.inpos = 0;
        ingest.inlen = 0;
        ingest_refresh();

        break;
    }
    case KEY_UP: /* load in history */
        if (ingest.history_pos == 0) { /* top of history */
            break;
        } else if (ingest.history_pos == ingest.history_len) { /* we have not loaded any history */
            strcpy(ingest.tmp_history, ingest.inbuf);
        }

        ingest.history_pos--;
        memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
        strcpy(ingest.inbuf, ingest.history[ingest.history_pos]);
        ingest.inlen = strlen(ingest.inbuf);
        ingest.inpos = ingest.inlen;
        ingest_refresh();

        break;
    case KEY_DOWN: /* load in history */
        if (ingest.history_pos == ingest.history_len) { /* no history loaded */
            break;
        } else if (ingest.history_pos == ingest.history_len - 1) { /* load back temp storage */
            ingest.history_pos++;
            memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
            strcpy(ingest.inbuf, ingest.tmp_history);
            ingest.inlen = strlen(ingest.inbuf);
            ingest.inpos = ingest.inlen;
        } else {
            ingest.history_pos++;
            memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
            strcpy(ingest.inbuf, ingest.history[ingest.history_pos]);
            ingest.inlen = strlen(ingest.inbuf);
            ingest.inpos = ingest.inlen;
        }

        ingest_refresh();

        break;
    default:
    {
        char tmp[1024];
        int tmp_len;

        if (ingest.inpos == sizeof(ingest.inbuf) - 1)
            break;

        tmp_len = ingest.inlen - ingest.inpos;
        memcpy(tmp, &ingest.inbuf[ingest.inpos], tmp_len);
        ingest.inbuf[ingest.inpos] = (char)ch;
        ingest.inpos++;
        ingest.inlen++;
        memcpy(&ingest.inbuf[ingest.inpos], tmp, tmp_len);

        if (ingest.inlen == sizeof(ingest.inbuf)) {
            ingest.inbuf[ingest.inlen - 1] = '\0';
            ingest.inlen--;
        }

        ingest_refresh();

        break;
    }
    }

    return 0;
}

static int
mode_xmodem(int block_sz)
{
    int quit_flag = 0;

    strcpy(ingest.tmp_history, ingest.inbuf);
    memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
    ingest.inlen = 0;
    ingest.inpos = 0;
    if (ingest.prepend)
        free(ingest.prepend);
    ingest.prepend = strdup("Give me a path (ctrl-c to stop): ");
    ingest_refresh();

    while (ingest.running) {
        int ch;

        pthread_mutex_lock(ingest.term_lock);
        ch = wgetch(ingest.input);
        pthread_mutex_unlock(ingest.term_lock);

        if (ch == ERR || handle_functions(ch)) {
            nanosleep(&(struct timespec){ 0, 100000 }, NULL);
            continue;
        }

        switch (ch) {
        case '\n':
            bstr_history_new_entry(ingest.xmodem_hist, ingest.inbuf);

            pthread_mutex_lock(ingest.term_lock);
            werase(ingest.input);
            wmove(ingest.input, 0, 0);
            wprintw(ingest.input, "Sending...");
            wrefresh(ingest.input);
            pthread_mutex_unlock(ingest.term_lock);

            cheerios_gofwd(-1);
            cheerios_xmodem(ingest.inbuf, block_sz);
            quit_flag = 1;
            break;
        case CTRL('c'):
            quit_flag = 1;
            break;
        case '\t': /* tab complete */
            auto_complete();
            ingest_refresh();
            break;
        case KEY_BACKSPACE:
        case KEY_UP:
            /* fall-through */
        case KEY_DOWN:
            {
                const char *filename;

                if (ch == KEY_UP) {
                    bstr_history_older(ingest.xmodem_hist);
                } else {
                    bstr_history_newer(ingest.xmodem_hist);
                }

                filename = bstr_history_atpos(ingest.xmodem_hist);
                if (!filename) {
                    memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
                    ingest.inpos = 0;
                    ingest.inlen = 0;
                    ingest_refresh();
                    break;
                }

                strncpy(ingest.inbuf, filename, sizeof(ingest.inbuf));
                ingest.inpos = strlen(ingest.inbuf);
                ingest.inlen = ingest.inpos;
                ingest_refresh();
                break;
            }
        default:
            {
                char tmp[1024];
                int tmp_len;

                if (ingest.inpos == sizeof(ingest.inbuf) - 1)
                    break;

                tmp_len = ingest.inlen - ingest.inpos;
                memcpy(tmp, &ingest.inbuf[ingest.inpos], tmp_len);
                ingest.inbuf[ingest.inpos] = (char)ch;
                ingest.inpos++;
                ingest.inlen++;
                memcpy(&ingest.inbuf[ingest.inpos], tmp, tmp_len);

                if (ingest.inlen == sizeof(ingest.inbuf)) {
                    ingest.inbuf[ingest.inlen - 1] = '\0';
                    ingest.inlen--;
                }

                ingest_refresh();

                break;
            }
        }

        if (quit_flag) {
            memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
            strcpy(ingest.inbuf, ingest.tmp_history);
            ingest.inlen = strlen(ingest.inbuf);
            ingest.inpos = ingest.inlen;
            ingest.mode = INGEST_MODE_NORMAL;
            free(ingest.prepend);
            ingest.prepend = NULL;

            bstr_history_unset_pos(ingest.xmodem_hist);

            bytenuts_set_status(STATUS_INGEST, "normal");
            ingest_refresh();
            return 0;
        }
    }

    return 0;
}

static int
mode_hex(int ch)
{
    if (handle_functions(ch))
        return 0;

    switch (ch) {
    case '\n': { /* send inputs */
        char hexbuf[sizeof(ingest.inbuf)/2];
        size_t inbuflen = strlen(ingest.inbuf);
        size_t hexlen = 0;
        /* assume leading 0 on odd length buffers */
        if (inbuflen % 2 != 0) {
            memmove(&ingest.inbuf[1], ingest.inbuf, inbuflen);
            ingest.inbuf[0] = '0';
            inbuflen++;
        }

        for (int i = 0; i < inbuflen; i += 2) {
            char tmp = ingest.inbuf[i+2];
            char *inval;
            long ch;

            ingest.inbuf[i+2] = '\0';
            ch = strtol(&ingest.inbuf[i], &inval, 16);
            ingest.inbuf[i+2] = tmp;

            /* strtol could not parse a character */
            if (inval != &ingest.inbuf[i+2])
                break;

            hexbuf[i/2] = ch;
            hexlen++;
        }

        inter_command_wait();
        cheerios_input(hexbuf, hexlen);
        if (ingest.config->echo) {
            cheerios_insert(">> (hex)\r\n", 10);
            for (int i = 0; i < hexlen; i++) {
                if (
                    i > 0 &&
                    i != hexlen-1 &&
                    i % 16 == 0
                ) {
                    cheerios_insert("\r\n", 2);
                }
                cheerios_insert(&ingest.inbuf[i*2], 2);
                cheerios_insert(" ", 1);
            }
            cheerios_insert("\r\n", 2);
        }

        /* store in history */
        ingest.history_pos = ingest.history_len;
        if (ingest.inlen > 0) {
            ingest.history_len++;
            ingest.history = realloc(ingest.history, ingest.history_len * sizeof(char *));
            ingest.history[ingest.history_pos] = strdup(ingest.inbuf);
            ingest.history_pos++;
            if (ingest.history_fd) {
                fwrite(ingest.inbuf, 1, strlen(ingest.inbuf), ingest.history_fd);
                fwrite("\n", 1, 1, ingest.history_fd);
            }
        }

        memset(ingest.inbuf, 0, sizeof(ingest.inbuf));
        ingest.inpos = 0;
        ingest.inlen = 0;
        ingest_refresh();

        break;
    }

    case KEY_UP: /* load in history */
    case KEY_DOWN: /* load in history */
    default:
        mode_normal(ch);
        break;
    }

    return 0;
}

static int
handle_functions(int ch)
{
    // char st[128];
    // sprintf(st, "%d", ch);
    // cheerios_info(st);

    switch (ch) {
    case KEY_PPAGE: /* page up */
        cheerios_goback(cheerios_getmaxy() / 2);
        return 1;
    case KEY_NPAGE: /* page down */
        cheerios_gofwd(cheerios_getmaxy() / 2);
        return 1;
    case KEY_SHOME: /* to back of log */
        cheerios_goback(-1);
        return 1;
    case KEY_SEND: /* go to front of log */
        cheerios_gofwd(-1);
        return 1;
    case 566: /* ctrl + up arrow */
        cheerios_goback(1);
        return 1;
    case 525: /* ctrl + down arrow */
        cheerios_gofwd(1);
        return 1;
    case KEY_RESIZE:
        bytenuts_update_screen_size();
        return 1;
    case KEY_BACKSPACE:
    {
        if (ingest.inpos == 0) {
            return 1;
        } else if (ingest.inpos == ingest.inlen) {
            ingest.inpos--;
            ingest.inlen--;
            ingest.inbuf[ingest.inlen] = '\0';
            ingest_refresh();
            return 1;
        }

        char tmp[1024];
        int tmp_len = ingest.inlen - ingest.inpos;

        memcpy(tmp, &ingest.inbuf[ingest.inpos], ingest.inlen - ingest.inpos);
        ingest.inpos--;
        ingest.inlen--;
        memcpy(&ingest.inbuf[ingest.inpos], tmp, tmp_len);
        ingest.inbuf[ingest.inlen] = '\0';
        ingest_refresh();

        return 1;
    }
    case KEY_DC: /* delete key */
    {
        char tmp[1024];
        int tmp_len;

        if (ingest.inpos == ingest.inlen) {
            return 1;
        } else if (ingest.inpos == ingest.inlen - 1) {
            ingest.inbuf[ingest.inpos] = '\0';
            ingest.inlen--;
            ingest_refresh();
            return 1;
        }

        tmp_len = ingest.inlen - ingest.inpos - 1;
        memcpy(tmp, &ingest.inbuf[ingest.inpos + 1], tmp_len);
        ingest.inlen--;
        memcpy(&ingest.inbuf[ingest.inpos], tmp, tmp_len);
        ingest.inbuf[ingest.inlen] = '\0';
        ingest_refresh();

        return 1;
    }
    case KEY_LEFT:
        if (ingest.inpos == 0)
            return 1;
        ingest.inpos--;
        ingest_refresh();
        return 1;
    case KEY_RIGHT:
        if (ingest.inpos == ingest.inlen)
            return 1;
        ingest.inpos++;
        ingest_refresh();
        return 1;
    case KEY_END:
        ingest.inpos = ingest.inlen;
        ingest_refresh();
        return 1;
    case KEY_HOME:
        ingest.inpos = 0;
        ingest_refresh();
        return 1;
#if 0
    case KEY_MOUSE:
    {
        MEVENT ev;
        if (getmouse(&ev) !=  OK)
            return 1;

        if (ev.bstate & BUTTON4_PRESSED) { /* scroll up */
            cheerios_goback(1);
        } else if (ev.bstate & BUTTON5_PRESSED) { /* scroll down */
            cheerios_gofwd(1);
        }

        return 1;
    }
#endif
    default:
        return 0;
    }
}

static int
print_stats()
{
    char st_line[128];

    sprintf(st_line, "\r\nSTATS\r\n");
    cheerios_insert(st_line, strlen(st_line));
    sprintf(st_line, "input line count: %d\r\n", ingest.history_len);
    cheerios_insert(st_line, strlen(st_line));

    cheerios_print_stats();
    bytenuts_print_stats();

    return 0;
}

static int
auto_complete()
{
    char cmd[1024];
    char line[1024];
    char **matches = NULL;
    int nmatches = 0;
    FILE *sh = NULL;
    char *inbuf_cpy = NULL;
    char *inbuf_basename = NULL;
    size_t inbuf_basename_len;

    /* retrieve directory listing with ls */
    inbuf_cpy = strdup(ingest.inbuf);
    if (ingest.inlen > 0 && ingest.inbuf[ingest.inlen - 1] == '/') {
        snprintf(
            cmd, sizeof(cmd),
            "cd %s 2> /dev/null && ls -1 2> /dev/null",
            ingest.inbuf
        );
        inbuf_basename = "";
    } else {
        snprintf(
            cmd, sizeof(cmd),
            "cd $(dirname \"%s\") 2> /dev/null && ls -1 2> /dev/null",
            ingest.inbuf
        );
        inbuf_basename = basename(inbuf_cpy);
    }
    inbuf_basename_len = strlen(inbuf_basename);

    sh = popen(cmd, "r");
    while (fgets(line, sizeof(line), sh)) {
        size_t line_len = strlen(line);
        while (line[line_len - 1] == '\n' || line[line_len - 1] == '\r') {
            line[line_len - 1] = '\0';
            line_len = strlen(line);
        }

        if (!memcmp(inbuf_basename, line, inbuf_basename_len)) {
            nmatches++;
            matches = realloc(matches, sizeof(char *) * nmatches);
            matches[nmatches - 1] = strdup(line);
        }
    }

    /* the cd/ls just failed, ignore output */
    int ret = pclose(sh);
    if (ret)
        goto auto_complete_cleanup;

    if (nmatches == 0)
        goto auto_complete_cleanup;

    /* check if we can fill the inbuf */
    int offset = inbuf_basename_len;

    while (1) {
        char add = matches[0][offset];

        for (int i = 1; i < nmatches; i++) {
            if (add != matches[i][offset]) {
                add = '\0';
                break;
            }
        }

        if (add == '\0')
            break;

        offset++;
    }

    if (offset > inbuf_basename_len) {
        size_t add_len = offset - inbuf_basename_len;

        if (add_len + ingest.inlen >= sizeof(ingest.inbuf))
            add_len = sizeof(ingest.inbuf) - ingest.inlen - 1;

        memcpy(&ingest.inbuf[ingest.inlen], &matches[0][inbuf_basename_len], add_len);
        ingest.inlen += add_len;
        ingest.inpos = ingest.inlen;
    }

    /* auto complete directory '/' */
    if (nmatches == 1) {
        struct stat st;

        stat(ingest.inbuf, &st);
        if (
            S_ISDIR(st.st_mode) &&
            ingest.inlen < sizeof(ingest.inbuf) - 1 &&
            (ingest.inpos == 0 || ingest.inbuf[ingest.inpos - 1] != '/')
        ) {
            ingest.inbuf[ingest.inpos] = '/';
            ingest.inpos++;
            ingest.inlen++;
        }
    }

auto_complete_cleanup:
    if (matches) {
        for (int i = 0; i < nmatches; i++) {
            free(matches[i]);
        }
        free(matches);
    }

    if (inbuf_cpy)
        free(inbuf_cpy);

    return 0;
}

static int
read_cmd_page(const char *home_dir, int idx)
{
    char fname[128];
    FILE *fd;
    char line[1024];

    snprintf(
        fname, sizeof(fname),
        "%s/.config/bytenuts/commands%d",
        home_dir, idx+1
    );

    fd = fopen(fname, "r");
    if (!fd)
        return -1;

    ingest.cmd_pgs_n++;
    ingest.cmd_pgs = realloc(
        ingest.cmd_pgs,
        sizeof(*ingest.cmd_pgs) * ingest.cmd_pgs_n
    );
    memset(&ingest.cmd_pgs[idx], 0, sizeof(*ingest.cmd_pgs));

    while (fgets(line, sizeof(line), fd)) {
        size_t line_len = strlen(line);

        /* strip CRLF */
        while (line_len > 0 && (line[line_len-1] == '\n' || line[line_len-1] == '\r')) {
            line_len--;
            line[line_len] = '\0';
        }

        ingest.cmd_pgs[idx].cmds_n++;
        ingest.cmd_pgs[idx].cmds[ingest.cmd_pgs[idx].cmds_n-1] = strdup(line);
    }

    fclose(fd);
    return 0;
}

static void
update_cmd_pg_status()
{
    if (ingest.cmd_pgs_n > 0) {
        bytenuts_set_status(STATUS_CMDPAGE, "cmd pg.%d", ingest.cmd_pg_cur+1);
    } else {
        bytenuts_set_status(STATUS_CMDPAGE, "n/a");
    }
}

static void
inter_command_wait()
{
    struct timespec target;
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);
    memcpy(&target, &ingest.cmd_ts, sizeof(struct timespec));

    timer_add_ms(&target, ingest.config->inter_cmd_to);
    timer_sub(&target, &now);

    nanosleep(&target, NULL);
    clock_gettime(CLOCK_REALTIME, &ingest.cmd_ts);
}
