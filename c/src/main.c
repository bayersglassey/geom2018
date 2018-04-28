
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "util.h"
#include "prismel.h"
#include "vec4.h"


int parse_prismel(fus_lexer_t *lexer, prismelrenderer_t *prend){
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

int parse_prismels(fus_lexer_t *lexer, prismelrenderer_t *prend){
    while(1){
        int err;
        char *name;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }

        err = fus_lexer_get_name(lexer, &name);
        if(err)return err;
        err = prismelrenderer_push_prismel(prend);
        if(err)return err;

        prend->prismel_list->name = name;

        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        err = parse_prismel(lexer, prend);
        if(err)return err;
    }
    return 0;
}

int parse_shapes(fus_lexer_t *lexer, prismelrenderer_t *prend){
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

int parse_geom(fus_lexer_t *lexer, prismelrenderer_t *prend){
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_done(lexer)){
            break;
        }else if(fus_lexer_got(lexer, "prismels")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_prismels(lexer, prend);
            if(err)return err;
        }else if(fus_lexer_got(lexer, "shapes")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_shapes(lexer, prend);
            if(err)return err;
        }else{
            return fus_lexer_unexpected(lexer);
        }
    }
    return 0;
}


int main(int n_args, char *args[]){
    fus_lexer_t lexer;
    prismelrenderer_t prend;
    char *text = "1 2 (3 4) 5";
    int err;

    if(n_args >= 2){
        text = load_file(args[1]);
        if(text == NULL)return 1;
    }

    err = fus_lexer_init(&lexer, text);
    if(err)return err;

    err = prismelrenderer_init(&prend, &vec4);
    if(err)return err;

    err = parse_geom(&lexer, &prend);
    if(err)return err;

    return 0;
}
