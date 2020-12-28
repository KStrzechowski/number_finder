#ifndef _INDEXING_H_
#define _INDEXING_H_
#include "other_functions.h"

void create_children(char directory[], bool IsSubdirectory, int interval);
void child_work(char path[], bool IsSubdirectory, int interval);
void *indexing(void *void_ptr);
int walk_in_directory_tree(const char *name, const struct stat *s, int type, struct FTW *f);
void walk_in_current_directory(void *void_ptr);
void read_file(const char file[], FILE *index_file);
void write_to_temp_file(index_element_t *elements, int size, FILE *index_file, const char *name);
void write_to_index_file(indexing_thread_t *args, bool first_dot_in_directory);

#endif