#include "timer_math.h"

int
timer_cmp(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec < b->tv_sec) {
        return -1;
    }
    else if (a->tv_sec > b->tv_sec) {
        return 1;
    }
    else if (a->tv_nsec < b->tv_nsec) {
        return -1;
    }
    else if (a->tv_nsec > b->tv_nsec) {
        return 1;
    }
    else {
        return 0;
    }
}

void
timer_add(struct timespec *a, const struct timespec *b)
{
    a->tv_sec += b->tv_sec;
    a->tv_nsec += b->tv_nsec;

    if (a->tv_nsec > 1000000000) {
        a->tv_sec += 1;
        a->tv_nsec -= 1000000000;
    }
}

void
timer_add_ms(struct timespec *ts, uint32_t ms)
{
    struct timespec tmp = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000
    };
    timer_add(ts, &tmp);
}

void
timer_sub(struct timespec *a, const struct timespec *b)
{
    if (timer_cmp(a, b) <= 0) {
        a->tv_sec = 0;
        a->tv_nsec = 0;
        return;
    }

    a->tv_sec -= b->tv_sec;
    a->tv_nsec -= b->tv_nsec;

    if (a->tv_nsec < 0) {
        a->tv_nsec += 1000000000;
        a->tv_sec -= 1;
    }
}

void
timer_sub_ms(struct timespec *ts, uint32_t ms)
{
    struct timespec tmp = {
        .tv_sec = ms / 1000,
        .tv_nsec = (ms % 1000) * 1000000
    };
    timer_sub(ts, &tmp);
}
