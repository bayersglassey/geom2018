
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>


#include "directory_shell.h"



int directory_shell_get_line(directory_shell_t *shell){
    int err;

    fputs("> ", stderr);

    if(!fgets(shell->line, LINE_LEN, stdin)){
        if(ferror(stdin)){
            perror("fgets");
            return 1;
        }
        return 0;
    }
    shell->line_len = strcspn(shell->line, "\n");
    shell->line[shell->line_len] = '\0';
    return 0;
}


int directory_shell_process_line(directory_shell_t *shell){
    if(
        !strncmp(shell->line, "ls", 2) &&
        (shell->line[2] == ' ' || shell->line[2] == '\0')
    ){
        if(shell->line[2] == ' '){
            directory_parse_path(shell->line + 3, shell->path, &shell->path_parts_len);
        }else{
            shell->path_parts_len = 0;
        }
        directory_entry_t _entry, *entry = directory_entry_find_path(
            shell->root, shell->path, shell->path_parts_len, &_entry);
        if(!entry){
            fprintf(stderr, "Entry not found\n");
        }else if(!entry->class->list){
            fprintf(stderr, "Not a directory\n");
        }else{
            directory_list_t list;
            for(int page = 0; page <= INT_MAX; page++){
                entry->class->list(entry, &list, page);
                for(int i = 0; i < list.entries_len; i++){
                    directory_entry_t *entry = &list.entries[i];
                    fputs(entry->name, stdout);
                    if(entry->class->list)putc('/', stdout);
                    fputc('\n', stdout);
                }
                if(list.entries_len < DIRECTORY_LIST_ENTRIES)break;
            }
        }
    }else{
        fprintf(stderr, "Unrecognized command\n");
    }

    return 0;
}

