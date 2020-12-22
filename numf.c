/*
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania
osiągnięcia efektów uczenia się z przedmiotu SOP została wykonana przeze
mnie samodzielnie.
Konrad Strzechowki 305891
*/

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <ftw.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define DEFAULT_MIN 10
#define DEFAULT_MAX 1000
#define DEFAULT_INTERVAL 600
#define MAX_NAME 256
#define MAX_OFFSET_SIZE 100
#define MAX_DIR 10
#define ELAPSED(start, end) ((end).tv_sec - (start).tv_sec) + (((end).tv_nsec - (start).tv_nsec) * 1.0e-9)
#define ERR(source) (perror(source),                    \
                     fprintf(stderr, "%s\n", __FILE__), \
                     exit(EXIT_FAILURE))

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s", name);
    exit(EXIT_FAILURE);
}

typedef struct index_element
{
    int number;
    int offset[MAX_OFFSET_SIZE];
    int size;
} index_element_t;

typedef struct indexing_thread
{
    pthread_t tid;
    char temp_file_name[MAX_NAME];
    char index_file_name[MAX_NAME];
    char path[MAX_NAME];
    bool IsSubdirectory;
} indexing_thread_t;

typedef struct args_reading_file
{
    int current_position;
    int number_position;
    int number;
} args_reading_file_t;

typedef struct querry_thread
{
    pthread_t tid;
    char directory[MAX_DIR * MAX_NAME];
    char input[MAX_INPUT];
} querry_thread_t;

volatile sig_atomic_t end_finder = 0;
volatile sig_atomic_t start_index = 0;
volatile sig_atomic_t status = 0;
pthread_mutex_t lock_index_file;
pthread_mutex_t lock_pid_file;
struct timespec start, current;
int min, max;
void read_arguments(int argc, char **argv, int *interval, bool *IsSubdirectory, int *count, char (*directory)[]);
void create_children(char directory[], bool IsSubdirectory, int interval);
void child_work(char path[], bool IsSubdirectory, int interval);
void *indexing(void *void_ptr);
int walk_in_directory_tree(const char *name, const struct stat *s, int type, struct FTW *f);
void walk_in_current_directory(void *void_ptr);
void read_file(const char file[], FILE *index_file);
void write_indexes(index_element_t *elements, int size, FILE *index_file, const char *name);
void sethandler(void (*f)(int), int sigNo);
void sig_quit_handler(int sig);
void sig_index_handler(int sig);
void sig_status_handler(int sig);
void *querry(void *void_ptr);
int check_command(char input[MAX_INPUT]);

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

void read_arguments(int argc, char **argv, int *interval, bool *IsSubdirectory, int *count, char (*directory)[])
{
    min = DEFAULT_MIN;
    max = DEFAULT_MAX;
    *interval = DEFAULT_INTERVAL;
    *IsSubdirectory = false;
    *count = 0;
    int c, i = 0;
    while ((c = getopt(argc, argv, "rm:M:i:")) != -1)
    {
        switch (c)
        {
        case 'r':
            *IsSubdirectory = true;
            break;
        case 'm':
            min = atoi(optarg);
            break;
        case 'M':
            max = atoi(optarg);
            break;
        case 'i':
            *interval = atoi(optarg);
            break;
        case '?':
        default:
            usage(argv[0]);
        }
    }
    while (optind < argc)
    {
        for (int j = 0; j < strlen(argv[optind]); j++)
        {
            (*directory)[i] = argv[optind][j];
            i++;
        }
        (*directory)[i] = ' ';
        i++;
        *count = *count + 1;
        optind++;
    }
}

void create_children(char directory[], bool IsSubdirectory, int interval)
{
    char dirs[MAX_DIR * MAX_NAME];
    char *path, *saveptr;
    const char *end_dir = " ";
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&lock_pid_file, &attr);

    for (int j = 0; j < strlen(directory); j++)
        dirs[j] = directory[j];
    dirs[strlen(directory) - 1] = '\0';
    path = strtok_r(dirs, end_dir, &saveptr);

    while (path != NULL)
    {
        pid_t pid;
        if ((pid = fork()) < 0)
            ERR("Fork:");
        sethandler(sig_quit_handler, SIGINT);
        sethandler(sig_index_handler, SIGUSR1);
        sethandler(sig_status_handler, SIGUSR2);

        if (0 == pid)
        {
            child_work(path, IsSubdirectory, interval);
            exit(EXIT_SUCCESS);
        }
        path = strtok_r(NULL, end_dir, &saveptr);
    }
}

void child_work(char path[], bool IsSubdirectory, int interval)
{
    FILE *pid_file;
    char pid_file_name[MAX_NAME];
    char index_file_name[MAX_NAME];
    char temp_file_name[MAX_NAME];
    struct timespec t = {
        .tv_sec = 1,
        .tv_nsec = 0,
    };
    //////////////////////////////
    if (path[strlen(path) - 1] == '\0')
        path[strlen(path) - 1] = '\n';

    sprintf(pid_file_name, "%s/.numf_pid", path);
    sprintf(index_file_name, "%s/.numf_index", path);
    sprintf(temp_file_name, "%s/.numf_temp", path);

    pthread_mutex_lock(&lock_pid_file);
    if (access(pid_file_name, F_OK) == 0)
        ERR("dir busy");
    pthread_mutex_unlock(&lock_pid_file);

    if (access(index_file_name, F_OK) == 0)
        start_index = 1;

    if ((pid_file = fopen(pid_file_name, "w")) == NULL)
        ERR("fopen");
    fprintf(pid_file, "%d", getpid());
    if (fclose(pid_file))
        ERR("fclose");
    ////////////////////////
    while (end_finder == 0 && start_index == 0)
    {
        if (status != 0)
        {
            fprintf(stdout, "NOT ACTIVE\n");
            status = 0;
        }
        nanosleep(&t, NULL);
    }

    indexing_thread_t args;
    args.IsSubdirectory = IsSubdirectory;
    strcpy(args.path, path);
    strcpy(args.temp_file_name, temp_file_name);
    strcpy(args.index_file_name, index_file_name);

    while (end_finder == 0)
    {
        if (clock_gettime(CLOCK_REALTIME, &start))
            ERR("Failed to retrieve time!");
        int err = pthread_create(&(args.tid), NULL, indexing, &args);
        if (err != 0)
            ERR("Couldn't create thread");
        start_index = 0;
        err = pthread_join(args.tid, NULL);
        if (err != 0)
            ERR("Can't join with a thread");
        do
        {
            if (status != 0)
            {
                fprintf(stdout, "NOT ACTIVE\n");
                status = 0;
            }
            if (clock_gettime(CLOCK_REALTIME, &current))
                ERR("Failed to retrieve time!");
            nanosleep(&t, NULL);
        } while ((ELAPSED(start, current) < interval) && end_finder == 0);
    }
    remove(pid_file_name);
}

void *indexing(void *void_ptr)
{
    FILE *index_file;
    FILE *temp_file;
    indexing_thread_t *args = void_ptr;
    char ch;
    bool first_dot_in_directory = false;
    char cwd[PATH_MAX];

    if (args->IsSubdirectory)
        first_dot_in_directory = true;

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        ERR("getcwd() error");

    if (args->IsSubdirectory)
    {
        chdir(args->path);
        if (nftw(".", walk_in_directory_tree, MAX_DIR, FTW_PHYS) == -1)
            ERR("walk");
        chdir(cwd);
    }
    else
    {
        walk_in_current_directory(args);
    }

    if (end_finder == 0)
    {
        if ((temp_file = fopen(args->temp_file_name, "r")) == NULL)
            ERR("fopen");

        pthread_mutex_lock(&lock_index_file);
        if ((index_file = fopen(args->index_file_name, "w")) == NULL)
            ERR("fopen");
        while ((ch = fgetc(temp_file)) != EOF)
        {
            if (ch == '\n' && args->IsSubdirectory)
                first_dot_in_directory = true;
            if (first_dot_in_directory && ch == '.')
            {
                fprintf(index_file, "%s", args->path);
                first_dot_in_directory = false;
            }
            else
                fputc(ch, index_file);
        }

        if (fclose(index_file))
            ERR("fclose");
        pthread_mutex_unlock(&lock_index_file);

        if (fclose(temp_file))
            ERR("fclose");
    }

    remove(args->temp_file_name);

    return void_ptr;
}

int walk_in_directory_tree(const char *file_name, const struct stat *s, int type, struct FTW *f)
{
    FILE *temp_file;
    struct stat filestat;
    const char temp_file_name[] = ".numf_temp";
    if ((temp_file = fopen(temp_file_name, "a")) == NULL)
        ERR("fopen");

    if (lstat(file_name, &filestat))
        ERR("lstat");
    if (strcmp(file_name, "./.numf_pid") != 0 && strcmp(file_name, "./.numf_index") != 0 && strcmp(file_name, "./.numf_temp") != 0 && !S_ISDIR(filestat.st_mode))
    {
        read_file(file_name, temp_file);
    }
    if (fclose(temp_file))
        ERR("fclose");

    if (status == 1)
    {
        if (clock_gettime(CLOCK_REALTIME, &current))
            ERR("Failed to retrieve time!");
        fprintf(stderr, "ACTIVE %f\n", ELAPSED(start, current));
        status = 0;
    }
    if (end_finder != 0)
    {
        return 1;
    }

    return 0;
}

void walk_in_current_directory(void *void_ptr)
{
    DIR *directory;
    struct dirent *dp;
    FILE *temp_file;
    struct stat filestat;
    indexing_thread_t *args = void_ptr;
    char file_path[MAX_NAME + MAX_NAME];

    if (NULL == (directory = opendir(args->path)))
        ERR("opendir");
    if ((temp_file = fopen(args->temp_file_name, "w")) == NULL)
        ERR("fopen");
    do
    {
        errno = 0;
        if ((dp = readdir(directory)) != NULL)
        {
            sprintf(file_path, "%s/%s", args->path, dp->d_name);
            if (lstat(file_path, &filestat))
                ERR("lstat");

            if (strcmp(dp->d_name, ".numf_pid") != 0 && strcmp(dp->d_name, ".numf_index") != 0 && strcmp(dp->d_name, ".numf_temp") != 0 && !S_ISDIR(filestat.st_mode))
            {
                read_file(file_path, temp_file);
            }
        }
        if (status == 1)
        {
            if (clock_gettime(CLOCK_REALTIME, &current))
                ERR("Failed to retrieve time!");
            fprintf(stderr, "ACTIVE %f\n", ELAPSED(start, current));
            status = 0;
        }
    } while (dp != NULL && end_finder == 0);
    if (fclose(temp_file))
        ERR("fclose");
}

void read_file(const char *name, FILE *index_file)
{
    FILE *file;
    char ch;
    int amount_of_numbers = max - min + 1;
    args_reading_file_t args = {
        .current_position = 0,
        .number = -1,
        .number_position = 0,
    };
    index_element_t *elements = (index_element_t *)malloc(sizeof(index_element_t) * (amount_of_numbers));
    for (int i = 0; i < (amount_of_numbers); i++)
    {
        elements[i].number = min + i;
        elements[i].size = 0;
    }

    if ((file = fopen(name, "r")) == NULL)
        ERR("fopen");
    while ((ch = fgetc(file)) != EOF)
    {
        if (min <= 0 && ch == '0' && args.number == -1)
        {
            elements[-min].offset[elements[-min].size] = args.current_position;
            elements[-min].size++;
        }
        else if (isdigit(ch))
        {
            if (args.number != -1)
                args.number = args.number * 10 + ch - 48;
            else
            {
                args.number = ch - 48;
                args.number_position = args.current_position;
            }
        }
        else
        {
            if (args.number >= min && args.number <= max)
            {
                elements[args.number - min].offset[elements[args.number - min].size] = args.number_position;
                elements[args.number - min].size++;
            }
            args.number = -1;
        }
        args.current_position++;
    }
    write_indexes(elements, amount_of_numbers, index_file, name);
    if (fclose(file))
        ERR("fclose");
}

void write_indexes(index_element_t *elements, int amount_of_numbers, FILE *index_file, const char *name)
{
    for (int i = 0; i < amount_of_numbers; i++)
    {
        if (elements[i].size != 0)
        {
            fprintf(index_file, "%d: ", elements[i].number);
            fprintf(index_file, "%s: ", name);
            for (int j = 0; j < elements[i].size; j++)
            {
                fprintf(index_file, "%d, ", elements[i].offset[j]);
            }
            fprintf(index_file, "\n");
        }
    }
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

void *querry(void *void_ptr)
{
    FILE *index_file;
    char index_file_name[MAX_NAME];
    char *line = NULL;
    int read;
    size_t len = 0;
    int i, input_number, file_number;
    char *input_word, *input_saveptr, *file_word, *file_saveptr, *path, *path_saveptr, ch;
    char dirs[MAX_DIR * MAX_NAME];
    const char *end_input_word = " ,.:()";
    const char *end_file_word = " ,:";
    char *current_path;
    querry_thread_t *args = void_ptr;

    input_word = strtok_r(args->input, end_input_word, &input_saveptr);
    input_word = strtok_r(NULL, end_input_word, &input_saveptr);
    while (input_word != NULL)
    {
        i = 0;
        input_number = 0;
        while ((ch = input_word[i]) != '\n' && (ch = input_word[i]) != '\0')
        {
            if (!isdigit(ch))
                ERR("Invalid input in querry");
            input_number = input_number * 10 + ch - 48;
            i++;
        }

        for (int j = 0; j < strlen(args->directory); j++)
            dirs[j] = args->directory[j];
        dirs[strlen(args->directory) - 1] = '\0';
        path = strtok_r(dirs, end_file_word, &path_saveptr);
        fprintf(stdout, "%d: \n", input_number);
        while (path != NULL)
        {
            if (path[strlen(path) - 1] == '\0')
                path[strlen(path) - 1] = '/';
            sprintf(index_file_name, "%s/.numf_index", path);
            if (access(index_file_name, F_OK) != 0)
            {
                fprintf(stdout, "No index file \n");
                return void_ptr;
            }
            pthread_mutex_lock(&lock_index_file);
            if ((index_file = fopen(index_file_name, "r")) == NULL)
                ERR("fopen");

            while ((read = getline(&line, &len, index_file)) != -1)
            {
                while (true)
                {
                    i = 0;
                    file_word = strtok_r(line, end_file_word, &file_saveptr);
                    file_number = 0;
                    while ((ch = file_word[i]) != '\n' && (ch = file_word[i]) != '\0')
                    {
                        if (!isdigit(ch))
                            ERR("Invalid symbol in file");
                        file_number = file_number * 10 + ch - 48;
                        i++;
                    }
                    if (file_number == input_number)
                        break;
                    if ((read = getline(&line, &len, index_file)) == -1)
                        break;
                }
                if (read == -1)
                    break;
                file_word = strtok_r(NULL, end_file_word, &file_saveptr);
                if (file_word == NULL)
                    ERR("Invalid data in file");
                current_path = file_word;
                file_word = strtok_r(NULL, end_file_word, &file_saveptr);
                while (file_word != NULL)
                {
                    file_number = 0;
                    i = 0;
                    if (isdigit(file_word[i]))
                    {
                        while ((ch = file_word[i]) != '\n' && (ch = file_word[i]) != '\0')
                        {
                            if (!isdigit(ch))
                                ERR("Invalid data in file");
                            file_number = file_number * 10 + ch - 48;
                            i++;
                        }
                        fprintf(stdout, "%s: %d \n", current_path, file_number);
                    }

                    file_word = strtok_r(NULL, end_file_word, &file_saveptr);
                }
            }
            if (fclose(index_file))
                ERR("fclose");
            pthread_mutex_unlock(&lock_index_file);
            path = strtok_r(NULL, end_file_word, &path_saveptr);
        }
        input_word = strtok_r(NULL, end_input_word, &input_saveptr);
    }

    return void_ptr;
}

int check_command(char input[MAX_INPUT])
{
    char *command, *saveptr;
    const char *end_word = " ";
    char line[MAX_INPUT];

    for (int j = 0; j < MAX_INPUT; j++)
        line[j] = input[j];
    command = strtok_r(line, end_word, &saveptr);

    if (command[strlen(command) - 1] == '\n')
        command[strlen(command) - 1] = '\0';
    if (strcmp("status", command) == 0)
        return 1;
    if (strcmp("index", command) == 0)
        return 2;
    if (strcmp("querry", command) == 0)
        return 3;
    if (strcmp("exit", command) == 0)
        return 4;
    return 0;
}
