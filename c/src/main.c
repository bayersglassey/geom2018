
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "util.h"
#include "prismel.h"


int parse_prismels(struct fus_lexer_t *lexer){
    int depth = 1;
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, "(")){
            depth++;
        }else if(fus_lexer_got(lexer, ")")){
            depth--;
            if(depth == 0){
                break;
            }
        }
    }
    return 0;
}

int parse_shapes(struct fus_lexer_t *lexer){
    int depth = 1;
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, "(")){
            depth++;
        }else if(fus_lexer_got(lexer, ")")){
            depth--;
            if(depth == 0){
                break;
            }
        }
    }
    return 0;
}

int parse_geom(struct fus_lexer_t *lexer){
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_done(lexer)){
            break;
        }else if(fus_lexer_got(lexer, "prismels")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_prismels(lexer);
            if(err)return err;
        }else if(fus_lexer_got(lexer, "shapes")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_shapes(lexer);
            if(err)return err;
        }else{
            return fus_lexer_unexpected(lexer);
        }
    }
    return 0;
}


int main(int n_args, char *args[]){
    char *text = "1 2 (3 4) 5";

    if(n_args >= 2){
        text = load_file(args[1]);
        if(text == NULL)return 1;
    }

    struct fus_lexer_t lexer;
    int err;

    err = fus_lexer_init(&lexer, text);
    if(err)return err;

    err = parse_geom(&lexer);
    if(err)return err;

    return 0;
}
