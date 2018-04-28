
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "util.h"
#include "prismel.h"
#include "vec4.h"


int parse_prismel(fus_lexer_t *lexer, prismelrenderer_t *prend){
    /*
        Example data:

            images:
                : ( 0 -2  2) ( 0 -1  2)
                : ( 0 -3  1) (-1 -2  3) ( 0 -1  1)
                : (-1 -3  1) (-2 -2  3) (-1 -1  1)
                : (-2 -2  2) (-2 -1  2)
                : (-2 -2  1) (-3 -1  3) (-2  0  1)
                : (-2 -1  1) (-3  0  3) (-2  1  1)
                : (-2  0  2) (-2  1  2)
                : (-1  0  1) (-2  1  3) (-1  2  1)
                : ( 0  0  1) (-1  1  3) ( 0  2  1)
                : ( 0  0  2) ( 0  1  2)
                : ( 1 -1  1) ( 0  0  3) ( 1  1  1)
                : ( 1 -2  1) ( 0 -1  3) ( 1  0  1)
    */
    prismel_t *prismel = prend->prismel_list;
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }

        if(fus_lexer_got(lexer, "images")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            for(int i = 0; i < prismel->n_images; i++){
                prismel_image_t *image = &prismel->images[i];
                err = fus_lexer_expect(lexer, "(");
                if(err)return err;
                while(1){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    if(fus_lexer_got(lexer, "(")){
                        int x, y, w;
                        err = fus_lexer_expect_int(lexer, &x);
                        if(err)return err;
                        err = fus_lexer_expect_int(lexer, &y);
                        if(err)return err;
                        err = fus_lexer_expect_int(lexer, &w);
                        if(err)return err;
                        err = fus_lexer_expect(lexer, ")");
                        if(err)return err;
                        err = prismel_image_push_line(image, x, y, w);
                        if(err)return err;
                    }else if(fus_lexer_got(lexer, ")")){
                        break;
                    }else{
                        return fus_lexer_unexpected(lexer);
                    }
                }
            }
            err = fus_lexer_expect(lexer, ")");
            if(err)return err;
        }else{
            return fus_lexer_unexpected(lexer);
        }
    }
    return 0;
}

int parse_prismels(fus_lexer_t *lexer, prismelrenderer_t *prend){
    int n_images = prend->space->rot_max;
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
        err = prismel_create_images(prend->prismel_list, n_images);
        if(err)return err;

        prend->prismel_list->name = name;

        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        err = parse_prismel(lexer, prend);
        if(err)return err;
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
            err = fus_lexer_parse_silent(lexer);
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

    prismelrenderer_dump(&prend, stdout);

    return 0;
}
