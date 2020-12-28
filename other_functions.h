#ifndef _OTHER_FUNCTIONS_H_
#define _OTHER_FUNCTIONS_H_

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

pthread_mutex_t lock_index_file;
pthread_mutex_t lock_pid_file;
struct timespec start, current;
int min, max;
extern volatile sig_atomic_t end_finder;
extern volatile sig_atomic_t start_index;
extern volatile sig_atomic_t status;

void usage(char *name);
void sethandler(void (*f)(int), int sigNo);
void sig_quit_handler(int sig);
void sig_index_handler(int sig);
void sig_status_handler(int sig);

#endif