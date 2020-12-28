#include "input.h"
#include "indexing.h"

volatile sig_atomic_t end_finder = 0;
volatile sig_atomic_t start_index = 0;
volatile sig_atomic_t status = 0;

int main(int argc, char **argv)
{
    int interval, count;
    char directory[MAX_DIR * MAX_NAME];
    char input[MAX_INPUT];
    bool IsSubdirectory;
    int which_command;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&lock_index_file, &attr);

    read_arguments(argc, argv, &interval, &IsSubdirectory, &count, &directory);
    create_children(directory, IsSubdirectory, interval);

    sethandler(sig_quit_handler, SIGINT);

    querry_thread_t args;
    strcpy(args.directory, directory);
    fgets(input, MAX_NAME, stdin);
    while (end_finder == 0)
    {
        strcpy(args.input, input);
        which_command = check_command(input);
        if (which_command == 1)
        {
            if (kill(0, SIGUSR2) < 0)
                ERR("kill status");
        }
        else if (which_command == 2)
        {
            if (kill(0, SIGUSR1) < 0)
                ERR("kill index");
        }
        else if (which_command == 3)
        {
            int err = pthread_create(&(args.tid), NULL, querry, &args);
            if (err != 0)
                ERR("Couldn't create thread");
            err = pthread_join(args.tid, NULL);
            if (err != 0)
                ERR("Can't join with a thread");
        }
        else if (which_command == 4)
        {
            if (kill(0, SIGINT) < 0)
                ERR("kill exit");
        }
        if (which_command != 4)
            fgets(input, MAX_NAME, stdin);
    }
    int n = count;
    while (n > 0)
    {
        sleep(1);
        pid_t pid;
        for (;;)
        {
            pid = waitpid(0, NULL, WNOHANG);
            if (pid > 0)
                n--;
            if (0 == pid)
                break;
            if (0 >= pid)
            {
                if (ECHILD == errno)
                    break;
                ERR("waitpid:");
            }
        }
        printf("\nPARENT: %d processes remain\n", n);
    }

    return EXIT_SUCCESS;
}