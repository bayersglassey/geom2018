
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "util.h"
#include "prismelrenderer.h"


static int parse_prismel(prismel_t *prismel, fus_lexer_t *lexer){
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
    int err;
    while(1){

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }else if(fus_lexer_got(lexer, "images")){
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

static int parse_prismels(prismelrenderer_t *prend, fus_lexer_t *lexer){
    int err;
    int n_images = prend->space->rot_max;
    while(1){
        char *name;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")"))break;

        err = fus_lexer_get_name(lexer, &name);
        if(err)return err;
        err = prismelrenderer_push_prismel(prend);
        if(err)return err;

        prend->prismel_list->name = name;

        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        err = parse_prismel(prend->prismel_list, lexer);
        if(err)return err;
    }
    return 0;
}

static int parse_shape_shapes(prismelrenderer_t *prend, fus_lexer_t *lexer, rendergraph_t *rgraph){
    /*
        Example data:

            : sixth (0 0 0 0)  0 f
            : sixth (0 0 0 0)  2 f
            : sixth (0 0 0 0)  4 f
    */
    int err;
    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }else if(fus_lexer_got(lexer, "(")){
            char *name;
            vec_t v;
            int rot;
            bool flip;

            err = fus_lexer_expect_name(lexer, &name);
            if(err)return err;

            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            for(int i = 0; i < prend->space->dims; i++){
                err = fus_lexer_expect_int(lexer, &v[i]);
                if(err)return err;
            }
            err = fus_lexer_expect(lexer, ")");
            if(err)return err;

            err = fus_lexer_expect_int(lexer, &rot);
            if(err)return err;

            err = fus_lexer_next(lexer);
            if(err)return err;
            if(fus_lexer_got(lexer, "t"))flip = true;
            else if(fus_lexer_got(lexer, "f"))flip = false;
            else return fus_lexer_unexpected(lexer);

            err = fus_lexer_expect(lexer, ")");
            if(err)return err;

            err = rendergraph_push_rendergraph_trf(rgraph);
            if(err)return err;

            rendergraph_t *found = prismelrenderer_get_rendergraph(
                prend, name);
            if(found == NULL){
                fprintf(stderr, "Couldn't find shape: %s\n", name);
                return 2;
            }

            free(name);
            rgraph->rendergraph_trf_list->rendergraph = found;
            rgraph->rendergraph_trf_list->trf.rot = rot;
            rgraph->rendergraph_trf_list->trf.flip = flip;
            vec_cpy(prend->space->dims, rgraph->rendergraph_trf_list->trf.add, v);
        }else{
            err = fus_lexer_unexpected(lexer);
            return err;
        }
    }
    return 0;
}

static int parse_shape_prismels(prismelrenderer_t *prend, fus_lexer_t *lexer, rendergraph_t *rgraph){
    /*
        Example data:

            : tri (0 0 0 0)  0 f 0
            : tri (1 0 0 0) 11 f 1
            : sq  (1 0 0 0)  1 f 2
    */
    int err;
    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }else if(fus_lexer_got(lexer, "(")){
            char *name;
            int dims = 0;
            vec_t v;
            int rot;
            bool flip;
            int color;

            err = fus_lexer_expect_name(lexer, &name);
            if(err)return err;

            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            for(int i = 0; i < prend->space->dims; i++){
                err = fus_lexer_expect_int(lexer, &v[i]);
                if(err)return err;
            }
            err = fus_lexer_expect(lexer, ")");
            if(err)return err;

            err = fus_lexer_expect_int(lexer, &rot);
            if(err)return err;

            err = fus_lexer_next(lexer);
            if(err)return err;
            if(fus_lexer_got(lexer, "t"))flip = true;
            else if(fus_lexer_got(lexer, "f"))flip = false;
            else return fus_lexer_unexpected(lexer);

            err = fus_lexer_expect_int(lexer, &color);
            if(err)return err;

            err = fus_lexer_expect(lexer, ")");
            if(err)return err;

            err = rendergraph_push_prismel_trf(rgraph);
            if(err)return err;

            prismel_t *found = prismelrenderer_get_prismel(prend, name);
            if(found == NULL){
                fprintf(stderr, "Couldn't find prismel: %s\n", name);
                return 2;
            }

            free(name);
            rgraph->prismel_trf_list->prismel = found;
            rgraph->prismel_trf_list->trf.rot = rot;
            rgraph->prismel_trf_list->trf.flip = flip;
            rgraph->prismel_trf_list->color = color;
            vec_cpy(prend->space->dims, rgraph->prismel_trf_list->trf.add, v);
        }else{
            err = fus_lexer_unexpected(lexer);
            return err;
        }
    }
    return 0;
}

static int parse_shape(prismelrenderer_t *prend, fus_lexer_t *lexer,
    const char *name
){
    int err;
    rendergraph_t *rgraph = calloc(1, sizeof(rendergraph_t));
    if(rgraph == NULL)return 1;
    err = rendergraph_init(rgraph, prend->space);
    if(err)goto err;

    rgraph->name = strdup(name);

    while(1){
        err = fus_lexer_next(lexer);
        if(err)goto err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }else if(fus_lexer_got(lexer, "shapes")){
            err = fus_lexer_expect(lexer, "(");
            if(err)goto err;
            err = parse_shape_shapes(prend, lexer, rgraph);
            if(err)goto err;
        }else if(fus_lexer_got(lexer, "prismels")){
            err = fus_lexer_expect(lexer, "(");
            if(err)goto err;
            err = parse_shape_prismels(prend, lexer, rgraph);
            if(err)goto err;
        }else{
            err = fus_lexer_unexpected(lexer);
            goto err;
        }
    }
    prend->rendergraph_map->rgraph = rgraph;
    return 0;
err:
    free(rgraph);
    return err;
}

static int parse_shapes(prismelrenderer_t *prend, fus_lexer_t *lexer){
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

        if(prismelrenderer_get_rendergraph(prend, name) != NULL){
            fprintf(stderr, "Shape %s already defined\n", name);
            return 2;
        }

        err = rendergraph_map_push(&prend->rendergraph_map);
        if(err)return err;
        prend->rendergraph_map->name = name;

        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        err = parse_shape(prend, lexer, name);
        if(err)return err;
    }
    return 0;
}

int prismelrenderer_parse(prismelrenderer_t *prend, fus_lexer_t *lexer){
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_done(lexer)){
            break;
        }else if(fus_lexer_got(lexer, "prismels")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_prismels(prend, lexer);
            if(err)return err;
        }else if(fus_lexer_got(lexer, "shapes")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_shapes(prend, lexer);
            if(err)return err;
        }else{
            return fus_lexer_unexpected(lexer);
        }
    }
    return 0;
}

