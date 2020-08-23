
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>


#include "file_utils.h"
#include "str_utils.h"
#include "array.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "directory.h"
#include "directory_shell.h"



typedef struct fusnode {
    char *name;
    ARRAY_DECL(struct fusnode *, children)
} fusnode_t;


static int _min(int a, int b){
    return a < b? a: b;
}

static void fusnode_list(directory_entry_t *root, struct directory_list *list, int page);
directory_entry_class_t fusnode_class = {
    .list = &fusnode_list,
};


static void fusnode_list(directory_entry_t *root, struct directory_list *list, int page){
    fusnode_t *fusnode = root->self;

    /* TODO: paging */
    if(page > 0){
        list->entries_len = 0;
        return;
    }
    int entries_len = _min(DIRECTORY_LIST_ENTRIES, fusnode->children_len);
    list->entries_len = entries_len;

    for(int i = 0; i < entries_len; i++){
        fusnode_t *child = fusnode->children[i];
        directory_entry_t *list_entry = &list->entries[i];
        list_entry->class = &fusnode_class;
        list_entry->name = child->name;
        list_entry->self = child;
    }
}



int parse_fusnode(fusnode_t *fusnode, fus_lexer_t *lexer){
    INIT
    while(!DONE && !GOT(")")){
        char *name;
        GET_NAME_OR_STR(name)

        ARRAY_PUSH_NEW(fusnode_t *, fusnode->children, child)
        child->name = name;

        if(GOT("(")){
            NEXT
            err = parse_fusnode(child, lexer);
            if(err)return err;
            GET(")")
        }
    }
    return 0;
}

fusnode_t *load_fusnode(const char *filename){
    char *buffer = load_file(filename);
    if(!buffer)return NULL;

    fus_lexer_t lexer;
    int err = fus_lexer_init(&lexer, buffer, filename);
    if(err)return NULL;

    fusnode_t *fusnode = calloc(1, sizeof(*fusnode));
    if(!fusnode){
        perror("calloc fusnode");
        return NULL;
    }
    fusnode->name = strdup(filename);

    err = parse_fusnode(fusnode, &lexer);
    if(err)return NULL;
    if(!fus_lexer_done(&lexer)){
        fprintf(stderr, "Lexer not done\n");
        return NULL;
    }

    fus_lexer_cleanup(&lexer);

    return fusnode;
}

void dump_fusnode(fusnode_t *fusnode, FILE *file, int depth){
    for(int i = 0; i < depth; i++)fputs("    ", file);
    fputs(fusnode->name, file);
    fputc('\n', file);
    for(int i = 0; i < fusnode->children_len; i++){
        fusnode_t *child = fusnode->children[i];
        dump_fusnode(child, file, depth + 1);
    }
}


int mainloop(directory_entry_t *root){
    int err;

    directory_shell_t _shell = {
        .root = root,
    };
    directory_shell_t *shell = &_shell;

    while(1){
        err = directory_shell_get_line(shell);
        if(err)return err;

        if(!strcmp(shell->line, "exit")){
            return 0;
        }else if(!strcmp(shell->line, "dump")){
            dump_fusnode(root->self, stderr, 0);
        }else{
            err = directory_shell_process_line(shell);
            if(err)return err;
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
    fusnode_t *fusnode = load_fusnode(filename);
    if(!fusnode){
        fprintf(stderr, "Couldn't load fusnode from: %s\n", filename);
        return 1;
    }

    directory_entry_t root = {
        .class = &fusnode_class,
        .name = fusnode->name,
        .self = fusnode,
    };

    return mainloop(&root);
}
