#include "indexing.h"

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
    indexing_thread_t *args = void_ptr;
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
        write_to_index_file(args, first_dot_in_directory);
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
    write_to_temp_file(elements, amount_of_numbers, index_file, name);
    if (fclose(file))
        ERR("fclose");
}

void write_to_temp_file(index_element_t *elements, int amount_of_numbers, FILE *index_file, const char *name)
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

void write_to_index_file(indexing_thread_t *args, bool first_dot_in_directory)
{
    FILE *temp_file;
    FILE *index_file;
    char ch;
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