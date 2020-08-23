#ifndef _DIRECTORY_SHELL_H_
#define _DIRECTORY_SHELL_H_


#include <stdio.h>
#include <stdlib.h>


#include "directory.h"


#define LINE_LEN 2048


typedef struct directory_shell {
    directory_entry_t *root;

    char line[LINE_LEN];
    size_t line_len;

    char path[LINE_LEN];
    int path_parts_len;
} directory_shell_t;


int directory_shell_get_line(directory_shell_t *shell);
int directory_shell_process_line(directory_shell_t *shell);


#endif