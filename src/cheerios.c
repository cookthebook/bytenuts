#include <ncurses.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cheerios.h"
#include "xmodem.h"

static cheerios_t cheerios;

static void *cheerios_thread(void *arg);
static int insert_buf(line_buffer_t *lines, const char *buf, size_t len);
static int write_lines(line_buffer_t *lines);
static int handle_color(line_buffer_t *lines, int line_idx, int *pos);
short curs_color(int fg);
static int newline(line_buffer_t *lines);

int
cheerios_start(bytenuts_t *bytenuts)
{
    memset(&cheerios, 0, sizeof(cheerios_t));

    cheerios.output = bytenuts->out_win;
    cheerios.term_lock = &bytenuts->term_lock;
    cheerios.ser_fd = bytenuts->serial_fd;
    cheerios.lines.bot = -1;

    cheerios.config = &bytenuts->config;

    if (cheerios.config->log_path) {
        cheerios.log = fopen(cheerios.config->log_path, "w");
        if (!cheerios.log) {
            return -1;
        }
    }

    pthread_cond_init(&cheerios.cond, NULL);
    pthread_mutex_init(&cheerios.lock, NULL);
    cheerios.running = 1;
    pthread_create(&cheerios.thr, NULL, cheerios_thread, NULL);

    if (cheerios.config->colors) {
        start_color();
        use_default_colors();
    }

    return 0;
}

int
cheerios_pause()
{
    pthread_mutex_lock(&cheerios.lock);
    cheerios.mode = CHEERIOS_MODE_PAUSED;
    pthread_mutex_unlock(&cheerios.lock);

    cheerios_info("Paused");
    return 0;
}

int
cheerios_resume()
{
    pthread_mutex_lock(&cheerios.lock);
    cheerios.mode = CHEERIOS_MODE_NORMAL;
    pthread_cond_signal(&cheerios.cond);
    pthread_mutex_unlock(&cheerios.lock);

    cheerios_info("Resumed");
    return 0;
}

int
cheerios_goback(int lines)
{
    pthread_mutex_lock(&cheerios.lock);

    if (lines < 0) { /* go back as far as we can */
        cheerios.lines.bot = getmaxy(cheerios.output) % cheerios.lines.n_lines;
    }
    else { /* just increment the bot pointer */
        if (cheerios.lines.bot < 0)
            cheerios.lines.bot = cheerios.lines.n_lines - 1;

        cheerios.lines.bot -= lines;
        if (cheerios.lines.bot < 0)
            cheerios.lines.bot = 0;
    }

    write_lines(&cheerios.lines);

    pthread_mutex_unlock(&cheerios.lock);

    return 0;
}

int
cheerios_gofwd(int lines)
{
    pthread_mutex_lock(&cheerios.lock);

    if (lines < 0) { /* go to front */
        cheerios.lines.bot = -1;
    }
    else { /* just increment the bot pointer */
        if (cheerios.lines.bot < 0)
            cheerios.lines.bot = cheerios.lines.n_lines - 1;

        cheerios.lines.bot += lines;
        if (cheerios.lines.bot >= cheerios.lines.n_lines)
            cheerios.lines.bot = -1;
    }

    write_lines(&cheerios.lines);

    pthread_mutex_unlock(&cheerios.lock);

    return 0;
}

int
cheerios_stop()
{
    cheerios.running = 0;
    pthread_join(cheerios.thr, NULL);

    return 0;
}

int
cheerios_input(const char *line)
{
    size_t line_len = strlen(line);

    pthread_mutex_lock(&cheerios.lock);

    if (cheerios.config->echo) {
        char *line_parsed = calloc(1, line_len + 5 + 1);
        sprintf(line_parsed, ">> %s\r\n", line);
        insert_buf(&cheerios.lines, line_parsed, strlen(line_parsed));
        free(line_parsed);
    }

    write(cheerios.ser_fd, line, strlen(line));
    if (cheerios.config->no_crlf)
        write(cheerios.ser_fd, "\n", 1);
    else
        write(cheerios.ser_fd, "\r\n", 2);

    pthread_mutex_unlock(&cheerios.lock);
    return 0;
}

int
cheerios_info(const char *line)
{
    size_t line_len = strlen(line);
    char *line_parsed = calloc(1, line_len + 12 + 1);
    sprintf(line_parsed, "BYTENUTS: %s\r\n", line);

    pthread_mutex_lock(&cheerios.lock);

    if (cheerios.lines.pos != 0) {
        insert_buf(&cheerios.lines, "\r\n", 2);
    }
    insert_buf(&cheerios.lines, line_parsed, strlen(line_parsed));

    pthread_mutex_unlock(&cheerios.lock);
    return 0;
}

int
cheerios_insert(const char *buf, size_t len)
{
    pthread_mutex_lock(&cheerios.lock);
    insert_buf(&cheerios.lines, buf, len);
    pthread_mutex_unlock(&cheerios.lock);

    return 0;
}


static void
__xmodem_callback(size_t sent, size_t total)
{
    char status_str[1024];


    snprintf(
        status_str, sizeof(status_str),
        "\r  %.02f/%.02fKB (%.01f%%)",
        (sent / 1024.0),
        (total / 1024.0),
        (sent * 100.0 / total)
    );

    cheerios_insert(status_str, strlen(status_str));
}

int
cheerios_xmodem(const char *path, int block_sz)
{
    struct stat st = { 0 };
    FILE *fd;
    char info[128];

    stat(path, &st);

    snprintf(
        info, sizeof(info),
        "Sending %s (%ld) with %dB payloads",
        path,
        st.st_size,
        block_sz
    );
    cheerios_info(info);

    fd = fopen(path, "r");
    if (!fd) {
        cheerios_info("Failed to open file!");
        return -1;
    }

    cheerios_pause();
    if (xmodem_send(
            cheerios.ser_fd,
            fileno(fd),
            st.st_size,
            block_sz,
            __xmodem_callback
    )) {
        fclose(fd);
        cheerios_resume();
        return -1;
    }

    fclose(fd);
    cheerios_resume();
    return 0;
}

int
cheerios_print_stats()
{
    char st_line[128];

    sprintf(st_line, "output line count: %d\r\n", cheerios.lines.n_lines);
    cheerios_insert(st_line, strlen(st_line));

    return 0;
}

int
cheerios_getmaxy()
{
    int y, x;
    getmaxyx(cheerios.output, y, x);
    return y;
}

int
cheerios_getmaxx()
{
    int y, x;
    getmaxyx(cheerios.output, y, x);
    return x;
}

int
cheerios_set_window(WINDOW *win)
{
    pthread_mutex_lock(&cheerios.lock);

    delwin(cheerios.output);
    cheerios.output = win;
    write_lines(&cheerios.lines);

    pthread_mutex_unlock(&cheerios.lock);

    return 0;
}

static void *
cheerios_thread(void *arg)
{
    char buf[1024];
    ssize_t read_ret = 0;
    struct pollfd fds;

    while (cheerios.running) {
        pthread_mutex_lock(&cheerios.lock);

        while (cheerios.mode == CHEERIOS_MODE_PAUSED) {
            pthread_cond_wait(&cheerios.cond, &cheerios.lock);
        }

        fds.fd = cheerios.ser_fd;
        fds.events = POLLIN;
        fds.revents = 0;

        if (poll(&fds, 1, 1) == 0) {
            pthread_mutex_unlock(&cheerios.lock);
            nanosleep(&(struct timespec){ 0, 100000 }, NULL);
            continue;
        }

        read_ret = read(cheerios.ser_fd, buf, sizeof(buf));
        if (read_ret > 0) {
            insert_buf(&cheerios.lines, buf, read_ret);
        }

        pthread_mutex_unlock(&cheerios.lock);
        nanosleep(&(struct timespec){ 0, 100000 }, NULL);
    }

    if (cheerios.log)
        fclose(cheerios.log);
    pthread_exit(NULL);
    return NULL;
}

static int
insert_buf(line_buffer_t *lines, const char *buf, size_t len)
{
    if (lines->n_lines == 0)
        newline(lines);

    for (size_t i = 0; i < len; i++) {
        if (cheerios.log && cheerios.mode == CHEERIOS_MODE_NORMAL)
            fwrite(&buf[i], 1, 1, cheerios.log);

        /* line feed starts a new row */
        if (buf[i] == '\n') {
            newline(lines);
        }
        /* carriage return just sets pos to 0 */
        else if (buf[i] == '\r') {
            lines->pos = 0;
        }
        else {
            if (lines->pos >= lines->line_lens[lines->n_lines - 1]) {
                lines->line_lens[lines->n_lines - 1] = lines->pos + 1;
                lines->lines[lines->n_lines - 1] = realloc(
                    lines->lines[lines->n_lines - 1],
                    sizeof(uint8_t) * lines->line_lens[lines->n_lines - 1]
                );
            }

            lines->lines[lines->n_lines - 1][lines->pos] = buf[i];
            lines->pos++;
        }
    }

    write_lines(lines);
    return 0;
}

static int
write_lines(line_buffer_t *lines)
{
    int row = lines->bot;
    int rows_printed = 0;
    int window_height = getmaxy(cheerios.output);

    if (row < 0)
        row = lines->n_lines - 1;

    pthread_mutex_lock(cheerios.term_lock);

    curs_set(0);
    werase(cheerios.output);

    while (row >= 0 && rows_printed < window_height) {
        wmove(cheerios.output, window_height - rows_printed - 1, 0);

        for (int i = 0; i < lines->line_lens[row]; i++) {
            handle_color(lines, row, &i);

            if (i >= getmaxx(cheerios.output)) {
                break;
            }

            if (i < lines->line_lens[row]) {
                waddch(cheerios.output, lines->lines[row][i]);
            }
        }

        rows_printed++;
        row--;
    }

    curs_set(1);
    wrefresh(cheerios.output);

    pthread_mutex_unlock(cheerios.term_lock);

    if (lines->bot < 0) {
        bytenuts_set_status(STATUS_CHEERIOS, "scrolling");
    } else {
        bytenuts_set_status(STATUS_CHEERIOS, "locked");
    }

    return 0;
}

static int
handle_color(line_buffer_t *lines, int line_idx, int *pos)
{
    short fg, bg = -1;
    int enable = -1;
    int p = *pos;
    uint8_t *line = lines->lines[line_idx];
    int line_len = lines->line_lens[line_idx];

    if (!cheerios.config->colors)
        return 0;

    while (p < line_len) {
        /* fg or bg color */
        if (!memcmp(&line[p], "\e[38;5;", 7) || !memcmp(&line[p], "\e[48;5;", 7)) {
            char col_str[4] = { 0 };
            short *col = line[p + 2] == '3' ? &fg : &bg;

            p += 7;
            if (p >= line_len)
                break;

            for (int i = 0; i < 3; i++) {
                if (line[p] == 'm') {
                    break;
                }
                else if (line[p] < '0' || line[p] > '9') {
                    col_str[0] = '\0';
                    break;
                }

                col_str[i] = line[p];
                p++;
            }

            if (col_str[0] == '\0')
                break;

            *col = strtol(col_str, NULL, 10);
            p++;
        }
        /* enable our color */
        else if (!memcmp(&line[p], "\e[1m", 4)) {
            p += 4;
            enable = 1;
            break;
        }
        /* disable our color */
        else if (!memcmp(&line[p], "\e[0m", 4)) {
            p += 4;
            enable = 0;
            break;
        }
        /* this is not a color code */
        else {
            break;
        }
    }

    if (enable == 0) {
        for (int i = 0; i < NCOLOR_PAIRS; i++) {
            if (lines->enabled_pairs[i]) {
                wattroff(cheerios.output, COLOR_PAIR(i + 1));
                lines->enabled_pairs[i] = 0;
            }
        }
    }
    else if (enable == 1 && (fg >= 0 || bg >= 0)) {
        int pair_pos = -1;

        if (fg < 0)
            fg = COLOR_WHITE;
        if (bg < 0)
            bg = COLOR_BLACK;

        for (int i = 0; i < NCOLOR_PAIRS; i++) {
            if (lines->color_pairs[i].fg == fg && lines->color_pairs[i].bg == bg) {
                pair_pos = i;
                break;
            }
        }

        if (pair_pos < 0) {
            pair_pos = lines->color_pos;
            init_pair(pair_pos + 1, fg, bg);
            lines->color_pairs[pair_pos].fg = fg;
            lines->color_pairs[pair_pos].bg = bg;
            lines->color_pos = (lines->color_pos + 1) % NCOLOR_PAIRS;
        }

        lines->enabled_pairs[pair_pos] = 1;
        wattron(cheerios.output, COLOR_PAIR(pair_pos + 1));
    }
    else {
        return 0;
    }

    *pos = p;
    return 0;
}

static int
newline(line_buffer_t *lines)
{
    lines->n_lines++;
    lines->lines = realloc(lines->lines, sizeof(uint8_t *) * lines->n_lines);
    lines->lines[lines->n_lines - 1] = NULL;
    lines->line_lens = realloc(lines->line_lens, sizeof(int) * lines->n_lines);
    lines->line_lens[lines->n_lines - 1] = 0;
    lines->pos = 0;
    return 0;
}
