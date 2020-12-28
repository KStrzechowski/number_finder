#include "input.h"

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

void *querry(void *void_ptr)
{
    int i, input_number;
    char *input_word, *input_saveptr, *path, *path_saveptr, ch;
    char dirs[MAX_DIR * MAX_NAME];
    const char *end_input_word = " ,.:()";
    const char *end_file_word = " ,:";

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
        check_every_index_file(path, end_file_word, input_number, path_saveptr);

        input_word = strtok_r(NULL, end_input_word, &input_saveptr);
    }
    return void_ptr;
}

void check_every_index_file(char *path, const char *end_file_word, int input_number, char *path_saveptr)
{
    FILE *index_file;
    char *line = NULL;
    int read, i, file_number;
    size_t len = 0;
    char index_file_name[MAX_NAME];
    char *file_saveptr, *file_word, *current_path;
    char ch;
    while (path != NULL)
    {
        if (path[strlen(path) - 1] == '\0')
            path[strlen(path) - 1] = '/';
        sprintf(index_file_name, "%s/.numf_index", path);
        if (access(index_file_name, F_OK) != 0)
        {
            fprintf(stdout, "No index file \n");
            return;
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
