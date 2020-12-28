#include "other_functions.h"

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s", name);
    exit(EXIT_FAILURE);
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sig_quit_handler(int sig)
{
    end_finder = 1;
}

void sig_index_handler(int sig)
{
    start_index = 1;
}

void sig_status_handler(int sig)
{
    status = 1;
}
