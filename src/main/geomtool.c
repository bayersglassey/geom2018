
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../geom.h"
#include "../hexspace.h"
#include "../vec4.h"
#include "../lexer.h"
#include "../file_utils.h"
#include "../geom_lexer_utils.h"

#define LINE_LEN 200


static int process_line(const char *line, size_t line_len, vecspace_t *space){
    int err;

    fus_lexer_t _lexer, *lexer=&_lexer;
    err = fus_lexer_init(lexer, line, "<stdin>");
    if(err)return err;

    /* The following loop should only ever have 1 iteration, because
    fus_lexer_eval_vec is supposed to consume everything within its
    fus "block" (...) */
    while(1){
        if(fus_lexer_done(lexer))break;
        vec_t vec;
        int err = fus_lexer_eval_vec(lexer, space, vec);
        if(err)return err;
        vec_fprintf(stdout, space->dims, vec);
        fputc('\n', stdout);
    }

    fus_lexer_cleanup(lexer);
    return 0;
}


static void print_help(FILE *file){
    fprintf(file,
        "  --hexspace   Use space: hexspace (default space is vec4)\n"
        "  --help       Display this message and exit\n"
    );
}


int main(int n_args, char **args){
    int err;

    vecspace_t *space = &vec4;

    for(int i = 1; i < n_args; i++){
        const char *arg = args[i];
        if(!strcmp(arg, "--help")){
            print_help(stdout);
            return 0;
        }else if(!strcmp(arg, "--hexspace")){
            space = &hexspace;
        }else{
            fprintf(stderr, "Unrecognized argument: %s\n", arg);
            print_help(stderr);
            return 2;
        }
    }

    char line[LINE_LEN];
    while(1){
        err = getln(line, LINE_LEN, stdin);
        if(err)return err;

        err = process_line(line, LINE_LEN, space);
        if(err)return err;
    }

    return 0;
}
