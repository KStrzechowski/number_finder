#ifndef _INPUT_H_
#define _INPUT_H_
#include "other_functions.h"

void read_arguments(int argc, char **argv, int *interval, bool *IsSubdirectory, int *count, char (*directory)[]);
void *querry(void *void_ptr);
void check_every_index_file(char *path, const char *end_file_word, int input_number, char *path_saveptr);
int check_command(char input[MAX_INPUT]);

#endif