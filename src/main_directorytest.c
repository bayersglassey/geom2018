
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>


#include "file_utils.h"
#include "str_utils.h"
#include "array.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "directory.h"


#define LINE_LEN 1024


typedef struct entry {
    char *name;
    ARRAY_DECL(struct entry *, children)
} entry_t;


static int _min(int a, int b){
    return a < b? a: b;
}

static void directory_entry_list(directory_entry_t *root, struct directory_list *list, int page);
directory_entry_class_t entry_class = {
    .list = &directory_entry_list,
};


static void directory_entry_list(directory_entry_t *root, struct directory_list *list, int page){
    entry_t *entry = root->self;

    /* TODO: paging */
    if(page > 0){
        list->entries_len = 0;
        return;
    }
    int entries_len = _min(DIRECTORY_LIST_ENTRIES, entry->children_len);
    list->entries_len = entries_len;

    for(int i = 0; i < entries_len; i++){
        entry_t *child = entry->children[i];
        directory_entry_t *list_entry = &list->entries[i];
        list_entry->class = &entry_class;
        list_entry->name = child->name;
        list_entry->self = child;
    }
}



int parse_entry(entry_t *entry, fus_lexer_t *lexer){
    INIT
    while(!DONE && !GOT(")")){
        char *name;
        GET_NAME_OR_STR(name)

        ARRAY_PUSH_NEW(entry_t *, entry->children, child)
        child->name = name;

        if(GOT("(")){
            NEXT
            err = parse_entry(child, lexer);
            if(err)return err;
            GET(")")
        }
    }
    return 0;
}

entry_t *load_entry(const char *filename){
    char *buffer = load_file(filename);
    if(!buffer)return NULL;

    fus_lexer_t lexer;
    int err = fus_lexer_init(&lexer, buffer, filename);
    if(err)return NULL;

    entry_t *entry = calloc(1, sizeof(*entry));
    if(!entry){
        perror("calloc entry");
        return NULL;
    }
    entry->name = strdup(filename);

    err = parse_entry(entry, &lexer);
    if(err)return NULL;
    if(!fus_lexer_done(&lexer)){
        fprintf(stderr, "Lexer not done\n");
        return NULL;
    }

    fus_lexer_cleanup(&lexer);

    return entry;
}

void dump_entry(entry_t *entry, FILE *file, int depth){
    for(int i = 0; i < depth; i++)fputs("    ", file);
    fputs(entry->name, file);
    fputc('\n', file);
    for(int i = 0; i < entry->children_len; i++){
        entry_t *child = entry->children[i];
        dump_entry(child, file, depth + 1);
    }
}


int mainloop(directory_entry_t *root){
    int err;
    while(1){
        fputs("> ", stderr);

        char line[LINE_LEN];
        if(!fgets(line, LINE_LEN, stdin)){
            if(ferror(stdin)){
                perror("fgets");
                return 1;
            }
            continue;
        }
        size_t line_len = strcspn(line, "\n");
        line[line_len] = '\0';

        char path[LINE_LEN];
        int path_len;
        if(!strcmp(line, "exit")){
            return 0;
        }else if(!strcmp(line, "dump")){
            dump_entry(root->self, stderr, 0);
        }else if(
            !strncmp(line, "ls", 2) &&
            (line[2] == ' ' || line[2] == '\0')
        ){
            if(line[2] == ' '){
                directory_parse_path(line + 3, path, &path_len);
            }else{
                path_len = 0;
            }
            directory_entry_t *entry = directory_entry_find_path(
                root, path, path_len);
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
                        fputc('\n', stdout);
                    }
                    if(list.entries_len < DIRECTORY_LIST_ENTRIES)break;
                }
            }
        }else{
            fprintf(stderr, "Unrecognized command\n");
        }
    }
    return 0;
}



int main(int n_args, char **args){
    if(n_args < 2){
        fprintf(stderr, "Missing required argument: FILENAME\n");
        return 1;
    }

    const char *filename = args[1];
    entry_t *entry = load_entry(filename);
    if(!entry){
        fprintf(stderr, "Couldn't load entry from: %s\n", filename);
        return 1;
    }

    directory_entry_t root = {
        .class = &entry_class,
        .name = entry->name,
        .self = entry,
    };

    return mainloop(&root);
}
