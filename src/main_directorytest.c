
#include <stdio.h>
#include <stdlib.h>


#include "file_utils.h"
#include "str_utils.h"
#include "array.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "directory.h"


typedef struct entry {
    char *name;
    ARRAY_DECL(struct entry *, children)
} entry_t;


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



int main(int n_args, char **args){
    if(n_args != 2){
        fprintf(stderr, "Missing required argument: FILENAME\n");
        return 1;
    }

    const char *filename = args[1];
    entry_t *entry = load_entry(filename);
    if(!entry){
        fprintf(stderr, "Couldn't load entry from: %s\n", filename);
        return 1;
    }

    dump_entry(entry, stdout, 0);

    return 0;
}
