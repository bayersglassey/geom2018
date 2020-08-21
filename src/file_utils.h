#ifndef _FILE_UTILS_H_
#define _FILE_UTILS_H_

#include <stdio.h>

int getln(char buf[], int buf_len, FILE *file);
char *load_file(const char *filename);

#endif