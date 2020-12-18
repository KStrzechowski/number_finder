/*
Oświadczam, że niniejsza praca stanowiąca podstawę do uznania
osiągnięcia efektów uczenia się z przedmiotu SOP została wykonana przeze
mnie samodzielnie.
Konrad Strzechowki 305891
*/

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
#define DEFAULT_MAX 50
#define DEFAULT_INTERVAL 600
#define MAX_NAME 50
#define MAX_OFFSET_SIZE 100
#define ERR(source) (perror(source),                                 \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
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

typedef struct args_reading_file
{
    int current_position;
    int number_position;
    int number;
} args_reading_file_t;

int min, max;
/*void read_arguments(int argc, char **argv, int *min, int *max, int *interval, bool *IsSubdirectory, int *count, char directory[][MAX_NAME]);
void create_children(int count, char directory[][MAX_NAME]);
void child_work(int i, char path[][MAX_NAME]);*/
void read_arguments(int argc, char **argv /*, int *min, int *max*/, int *interval, bool *IsSubdirectory, int *count, char (*directory)[]);
void create_children(int count, char directory[], index_element_t *elements);
void child_work(int i, char path[], index_element_t *elements);
void write_indexes(index_element_t *elements, int size);
void read_file(char file[], index_element_t *elements);

int main(int argc, char **argv)
{
    int /*min, max, */ interval, count;
    char directory[MAX_NAME];
    bool IsSubdirectory;

    read_arguments(argc, argv /*, &min, &max*/, &interval, &IsSubdirectory, &count, &directory);

    index_element_t *elements = (index_element_t *)malloc(sizeof(index_element_t) * (max - min + 1));
    for (int i = 0; i < (max - min + 1); i++)
    {
        elements[i].number = min + i;
        elements[i].size = 0;
    }

    create_children(count, directory, elements);

    int n = count;
    while (n > 0)
    {
        sleep(3);
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
        printf("PARENT: %d processes remain\n", n);
    }
    //write_indexes(elements, max - min + 1);
    return EXIT_SUCCESS;
}

void read_arguments(int argc, char **argv /*, int *min, int *max*/, int *interval, bool *IsSubdirectory, int *count, char (*directory)[])
{
    min = DEFAULT_MIN;
    max = DEFAULT_MAX;
    *interval = DEFAULT_INTERVAL;
    *IsSubdirectory = false;
    *count = 0;
    int c;
    while ((c = getopt(argc, argv, "rm:M:")) != -1)
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
        case '?':
        default:
            usage(argv[0]);
        }
    }
    //directory = malloc(sizeof(char *) * 20);
    if (optind < argc)
    {
        //strcpy(directory[*count], argv[optind]);
        strcpy(*directory, argv[optind]);
        *count = *count + 1;
        optind++;
    }
}

void create_children(int count, char directory[], index_element_t *elements)
{
    int i;
    for (i = 0; i < count; i++)
    {
        pid_t pid;
        if ((pid = fork()) < 0)
            ERR("Fork:");
        if (0 == pid)
        {
            child_work(i, directory, elements);
            exit(EXIT_SUCCESS);
        }
    }
}

void child_work(int i, char path[], index_element_t *elements)
{
    DIR *directory;
    struct dirent *dp;
    FILE *pid_file;
    char pid_file_name[MAX_NAME];
    char file_path[300];
    struct stat filestat;

    //////////////////////////////
    if (NULL == (directory = opendir(path)))
        ERR("opendir");

    while ((dp = readdir(directory)) != NULL)
        if (strcmp(dp->d_name, ".numf_pid") == 0)
            ERR("dir_busy");

    sprintf(pid_file_name, "%s/.numf_pid", path);
    if ((pid_file = fopen(pid_file_name, "w")) == NULL)
        ERR("fopen");
    fprintf(pid_file, "%d", getpid());
    closedir(directory);
    ////////////////////////

    if (NULL == (directory = opendir(path)))
        ERR("opendir");

    do
    {
        errno = 0;
        if ((dp = readdir(directory)) != NULL)
        {
            sprintf(file_path, "%s/%s", path, dp->d_name);
            if (lstat(file_path, &filestat))
                ERR("lstat");
            if (strcmp(dp->d_name, ".numf_pid") != 0 && !S_ISDIR(filestat.st_mode))
            {
                read_file(file_path, elements);
            }
        }
    } while (dp != NULL);
    closedir(directory);
    remove(pid_file_name);
}

void read_file(char *name, index_element_t *elements) // przesylac elements - zeby ja zapelniac
{
    FILE *file;
    char ch;
    args_reading_file_t args = {
        .current_position = 0,
        .number = -1,
        .number_position = 0,
    };
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
    write_indexes(elements, max - min + 1);
}

void write_indexes(index_element_t *elements, int size)
{
    for (int i = 0; i < size; i++)
    {
        if (elements[i].size != 0)
        {
            printf("%d: ", elements[i].number);
            for (int j = 0; j < elements[i].size; j++)
                printf("%d, ", elements[i].offset[j]);
            printf("\n");
        }
    }
}
