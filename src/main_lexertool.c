

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "write.h"
#include "file_utils.h"


bool use_vars = false;


static void print_help(){
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h | --help   This message\n");
    fprintf(stderr, "  -v | --vars   Use vars, macros, etc\n");
}

static void _print_tabs(FILE *file, int depth){
    for(int i = 0; i < depth; i++)fputs("    ", file);
}

static void _print_newline(FILE *file, int depth){
    fputc('\n', file);
    _print_tabs(file, depth);
}

static int mainloop(fus_lexer_t *lexer, FILE *file){
    int err;
    int depth = 0;
    while(!fus_lexer_done(lexer)){
        switch(lexer->token_type){
            case FUS_LEXER_TOKEN_INT:
            case FUS_LEXER_TOKEN_SYM:
            case FUS_LEXER_TOKEN_OP:
                _print_tabs(file, depth);
                fprintf(file, "%.*s\n", lexer->token_len, lexer->token);
                break;
            case FUS_LEXER_TOKEN_STR:
                _print_tabs(file, depth);
                fus_nwrite_str(file, lexer->token + 1, lexer->token_len - 2);
                fputc('\n', file);
                break;
            case FUS_LEXER_TOKEN_BLOCKSTR:
                _print_tabs(file, depth);
                fus_nwrite_str(file, lexer->token + 2, lexer->token_len - 2);
                fputc('\n', file);
                break;
            case FUS_LEXER_TOKEN_OPEN:
                _print_tabs(file, depth);
                fprintf(file, ":\n");
                depth++;
                break;
            case FUS_LEXER_TOKEN_CLOSE:
                depth--;
                break;
            default:
                fprintf(stderr, "Unrecognized token type: %i\n",
                    lexer->token_type);
                return 2;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }
    return 0;
}


int main(int n_args, char **args){
    int err;

    for(int i = 1; i < n_args; i++){
        const char *arg = args[i];
        if(!strcmp(arg, "-h") || !strcmp(arg, "--help")){
            print_help();
            return 0;
        }else if(!strcmp(arg, "-v") || !strcmp(arg, "--vars")){
            use_vars = true;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            print_help();
            return 2;
        }
    }

    char *buffer = read_stream(stdin, "<stdin>");
    if(!buffer)return 1;

    fus_lexer_t lexer;
    if(use_vars){
        err = fus_lexer_init_with_vars(&lexer, buffer, "<stdin>", NULL);
        if(err)return err;
    }else{
        err = fus_lexer_init(&lexer, buffer, "<stdin>");
        if(err)return err;
    }

    err = mainloop(&lexer, stdout);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    return 0;
}
