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

#define DEFAULT_MIN 0
#define DEFAULT_MAX 50
#define DEFAULT_INTERVAL 600
#define MAX_NAME 100
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

volatile sig_atomic_t end_finder = 0;
volatile sig_atomic_t start_index = 0;
volatile sig_atomic_t status = 0;
int min, max;
/*void read_arguments(int argc, char **argv, int *min, int *max, int *interval, bool *IsSubdirectory, int *count, char directory[][MAX_NAME]);
void create_children(int count, char directory[][MAX_NAME]);
void child_work(int i, char path[][MAX_NAME]);*/
void read_arguments(int argc, char **argv /*, int *min, int *max*/, int *interval, bool *IsSubdirectory, int *count, char (*directory)[]);
void create_children(int count, char directory[]);
void child_work(int i, char path[]);
void write_indexes(index_element_t *elements, int size, FILE *index_file, char *name);
void read_file(char file[], FILE *index_file);
void sethandler(void (*f)(int), int sigNo);
void sig_quit_handler(int sig);
void sig_index_handler(int sig);
void sig_status_handler(int sig);
void querry(char directory[], char input[MAX_INPUT]);
int check_command(char input[MAX_INPUT]);

int main(int argc, char **argv)
{
    int /*min, max, */ interval, count;
    char directory[MAX_NAME];
    char input[MAX_INPUT];
    bool IsSubdirectory;
    int which_command;

    read_arguments(argc, argv /*, &min, &max*/, &interval, &IsSubdirectory, &count, &directory);

    create_children(count, directory);

    sethandler(sig_quit_handler, SIGINT);
    //sethandler(sig_status_handler, SIGUSR2);

    while (end_finder == 0)
    {
        fgets(input, MAX_NAME, stdin);
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
            querry(directory, input);
        }
        else if (which_command == 4)
        {
            if (kill(0, SIGINT) < 0)
                ERR("kill exit");
        }
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
        printf("PARENT: %d processes remain\n", n);
    }

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

void create_children(int count, char directory[])
{
    int i;
    for (i = 0; i < count; i++)
    {
        pid_t pid;
        if ((pid = fork()) < 0)
            ERR("Fork:");
        sethandler(sig_quit_handler, SIGINT);
        sethandler(sig_index_handler, SIGUSR1);
        sethandler(sig_status_handler, SIGUSR2);

        if (0 == pid)
        {
            child_work(i, directory);
            exit(EXIT_SUCCESS);
        }
    }
}

void child_work(int i, char path[])
{
    DIR *directory;
    struct dirent *dp;
    FILE *pid_file;
    FILE *index_file;
    FILE *temp_file;
    char pid_file_name[MAX_NAME];
    char index_file_name[MAX_NAME];
    char temp_file_name[MAX_NAME];
    char file_path[300];
    struct stat filestat;
    char ch;
    struct timespec t1 = {100000000, 0};
    struct timespec t2 = {1000000000, 0};
    //////////////////////////////
    if (NULL == (directory = opendir(path)))
        ERR("opendir");

    while ((dp = readdir(directory)) != NULL)
        if (strcmp(dp->d_name, ".numf_pid") == 0)
            ERR("dir_busy");
    closedir(directory);

    sprintf(pid_file_name, "%s/.numf_pid", path);
    sprintf(index_file_name, "%s/.numf_index", path);
    sprintf(temp_file_name, "%s/.numf_temp", path);
    if ((pid_file = fopen(pid_file_name, "w")) == NULL)
        ERR("fopen");
    fprintf(pid_file, "%d", getpid());
    if (fclose(pid_file))
        ERR("fclose");
    ////////////////////////

    while (end_finder == 0 && start_index == 0)
    {
        if (status == 1)
        {
            fprintf(stdout, "%s: NOT ACTIVE\n", index_file_name);
            status = 0;
        }
        nanosleep(&t1, NULL);
    }

    while (end_finder == 0)
    {
        if (NULL == (directory = opendir(path)))
            ERR("opendir");
        if ((temp_file = fopen(temp_file_name, "w")) == NULL)
            ERR("fopen");

        do
        {
            errno = 0;
            if ((dp = readdir(directory)) != NULL)
            {
                sprintf(file_path, "%s/%s", path, dp->d_name);
                if (lstat(file_path, &filestat))
                    ERR("lstat");
                if (strcmp(dp->d_name, ".numf_pid") != 0 && strcmp(dp->d_name, ".numf_index") != 0 && strcmp(dp->d_name, ".numf_temp") != 0 && !S_ISDIR(filestat.st_mode))
                {
                    read_file(file_path, temp_file);
                }
            }

        } while (dp != NULL);
        if (status == 1)
        {
            fprintf(stdout, "%s: ACTIVE\n", index_file_name);
            status = 0;
        }

        if (fclose(temp_file))
            ERR("fclose");

        if ((temp_file = fopen(temp_file_name, "r")) == NULL)
            ERR("fopen");
        //////////////// OCHRONA MUTEX PRZED SPRAWDZANIEM QUERRY
        if ((index_file = fopen(index_file_name, "w")) == NULL)
            ERR("fopen");

        while ((ch = fgetc(temp_file)) != EOF)
            fputc(ch, index_file);

        if (fclose(index_file))
            ERR("fclose");
        /////////////////
        if (fclose(temp_file))
            ERR("fclose");
        nanosleep(&t2, NULL);
        ///////////// ZMIENIC CZEKANIE I DODAC DO NIEGO SPRAWDZANIE STATUS
        closedir(directory);
    }
    remove(pid_file_name);
    remove(temp_file_name);
}

void read_file(char *name, FILE *index_file)
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

void write_indexes(index_element_t *elements, int amount_of_numbers, FILE *index_file, char *name)
{
    for (int i = 0; i < amount_of_numbers; i++)
    {
        fprintf(index_file, "%d: ", elements[i].number);
        fprintf(index_file, "%s: ", name);
        if (elements[i].size != 0)
        {
            for (int j = 0; j < elements[i].size; j++)
                fprintf(index_file, "%d, ", elements[i].offset[j]);
        }
        fprintf(index_file, "\n");
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

void querry(char directory[], char input[MAX_INPUT])
{
    FILE *index_file;
    char index_file_name[MAX_NAME];
    char *line = NULL;
    int read;
    size_t len = 0;
    int i, input_number, file_number;
    char *input_word, *input_saveptr, *file_word, *file_saveptr, ch;
    const char *end_word = " ,.:()";
    char *current_path;
    input_word = strtok_r(input, end_word, &input_saveptr);
    input_word = strtok_r(NULL, end_word, &input_saveptr);

    sprintf(index_file_name, "%s/.numf_index", directory);
    if ((index_file = fopen(index_file_name, "r")) == NULL)
        ERR("fopen");

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

        i = 0;
        printf("%d:\n", input_number);

        while ((read = getline(&line, &len, index_file)) != -1)
        {
            for (int j = 0; j < input_number; j++) // DOPISAC DLA WIELU SCIEZEK
                if ((read = getline(&line, &len, index_file)) == -1)
                    break;

            file_word = strtok_r(line, end_word, &file_saveptr);
            file_word = strtok_r(NULL, end_word, &file_saveptr);
            if (file_word == NULL)
                ERR("Invalid data in file");
            current_path = file_word;
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
                    printf("%s: %d\n", current_path, file_number);
                }

                file_word = strtok_r(NULL, end_word, &file_saveptr);
            }

            for (int j = 0; j < max - input_number; j++)
                if ((read = getline(&line, &len, index_file)) == -1)
                    break;
        }
        rewind(index_file);
        input_word = strtok_r(NULL, end_word, &input_saveptr);
    }

    if (fclose(index_file))
        ERR("fclose");
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
