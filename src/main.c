#include <sched.h>
#include <signal.h>
#include <stdlib.h>

#include "bytenuts.h"

static void sigint_handler(int s);

int
main(int argc, char **argv)
{
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    if (bytenuts_run(argc, argv)) {
        bytenuts_stop();
        exit(1);
    }

    exit(0);
}

static void
sigint_handler(int s)
{
    bytenuts_kill();
    exit(1);
}
