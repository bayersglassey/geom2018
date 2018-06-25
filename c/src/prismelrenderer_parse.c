
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "util.h"
#include "array.h"
#include "prismelrenderer.h"



/******************
 * PALETTE MAPPER *
 ******************/

static int parse_palmappers(prismelrenderer_t *prend, fus_lexer_t *lexer){
    int err;
    while(1){
        char *name;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")"))break;

        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;

        palettemapper_t *palmapper;
        err = fus_lexer_expect_palettemapper(lexer, prend, name, &palmapper);
        if(err)return err;
    }
    return 0;
}

int fus_lexer_expect_palettemapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, palettemapper_t **palmapper_ptr
){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get_palettemapper(lexer, prend, name, palmapper_ptr);
}

int fus_lexer_get_palettemapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, palettemapper_t **palmapper_ptr
){
    int err;

    ARRAY_PUSH_NEW(palettemapper_t, *prend, palmappers, palmapper)
    err = palettemapper_init(palmapper, strdup(name), -1);
    if(err)return err;

    Uint8 *table = palmapper->table;
    int color_i;

    err = fus_lexer_get(lexer, "(");
    if(err)return err;

    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")"))break;

        if(fus_lexer_got_int(lexer)){
            err = fus_lexer_get_int(lexer, &color_i);
            if(err)return err;
            err = fus_lexer_next(lexer);
            if(err)return err;
        }

        int color;

        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_expect_int(lexer, &color);
        if(err)return err;
        err = fus_lexer_expect(lexer, ")");
        if(err)return err;

        if(color < 0 || color >= 256){
            return fus_lexer_unexpected(lexer, "int within 0..255");}

        table[color_i] = color;
    }

    *palmapper_ptr = palmapper;
    return 0;
}


/***********
 * PRISMEL *
 ***********/

int fus_lexer_expect_prismel(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, prismel_t **prismel_ptr
){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get_prismel(lexer, prend, name, prismel_ptr);
}

static int parse_prismel_image(prismel_t *prismel, fus_lexer_t *lexer,
    int image_i
){
    int err;
    prismel_image_t *image = &prismel->images[image_i];
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;

    err = fus_lexer_next(lexer);
    if(err)return err;

    if(fus_lexer_got_int(lexer)){
        /* For example:
            6 +( 4  0) */

        int i;
        err = fus_lexer_get_int(lexer, &i);
        if(err)return err;
        if(i < 0 || i >= image_i){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Prismel image reference out "
                "of bounds: %i not in 0..%i\n",
                i, image_i - 1);
            return 2;
        }
        prismel_image_t *other_image = &prismel->images[i];

        int add_x = 0, add_y = 0;
        err = fus_lexer_next(lexer);
        if(err)return err;
        if(fus_lexer_got(lexer, "+")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;

            err = fus_lexer_expect_int(lexer, &add_x);
            if(err)return err;
            err = fus_lexer_expect_int(lexer, &add_y);
            if(err)return err;

            err = fus_lexer_expect(lexer, ")");
            if(err)return err;
            err = fus_lexer_next(lexer);
            if(err)return err;
        }

        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        for(int i = 0; i < other_image->lines_len; i++){
            prismel_image_line_t *other_line =
                other_image->lines[i];
            err = prismel_image_push_line(image,
                other_line->x + add_x, other_line->y + add_y,
                other_line->w);
            if(err)return err;
        }
    }else{
        /* For example:
            ( 1 -2  1) ( 0 -1  3) ( 1  0  1) */
        while(1){
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
                err = fus_lexer_next(lexer);
                if(err)return err;
            }else if(fus_lexer_got(lexer, ")")){
                break;
            }else{
                return fus_lexer_unexpected(lexer, "(...)");
            }
        }
    }

    return 0;
}

int fus_lexer_get_prismel(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, prismel_t **prismel_ptr
){
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
                :  6 +( 4  0)
                :  7 +( 4 -2)
                :  8 +( 2 -4)
                :  9 +( 0 -4)
                : 10 +(-2 -4)
                : 11 +(-4 -2)
                :  0 +(-4  0)
                :  1 +(-4  2)
                :  2 +(-2  4)
                :  3 +( 0  4)
                :  4 +( 2  4)
                :  5 +( 4  2)
    */
    int err;

    prismel_t *prismel;
    err = prismelrenderer_push_prismel(prend, name, &prismel);
    if(err)return err;

    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    while(1){

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }else if(fus_lexer_got(lexer, "images")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            for(int i = 0; i < prismel->n_images; i++){
                err = parse_prismel_image(prismel, lexer, i);
                if(err)return err;
            }
            err = fus_lexer_expect(lexer, ")");
            if(err)return err;
        }else{
            return fus_lexer_unexpected(lexer, "images");
        }
    }

    *prismel_ptr = prismel;
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

        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;

        prismel_t *prismel;
        err = fus_lexer_get_prismel(lexer, prend, name, &prismel);
        if(err)return err;
    }
    return 0;
}




/*********
 * SHAPE *
 *********/

static int parse_shape_shapes(prismelrenderer_t *prend, fus_lexer_t *lexer,
    rendergraph_t *rgraph
){
    /*
        Example data:

            : sixth (0 0 0 0)  0 f
            : sixth (0 0 0 0)  2 f
            : sixth (0 0 0 0)  4 f  0 (0 1)
            : sixth (0 0 0 0)  6 f "red"  0 (0 1)
    */
    int err;
    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")")){
            break;
        }else if(fus_lexer_got(lexer, "(")){
            char *name;
            char *palmapper_name = NULL;
            palettemapper_t *palmapper = NULL;
            vec_t v;
            int rot;
            bool flip;
            int frame_start = 0;
            int frame_len = -1;
            int frame_i = 0;
            bool frame_i_additive = true;

            /* name */
            err = fus_lexer_expect_str(lexer, &name);
            if(err)return err;

            /* trf.add */
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            for(int i = 0; i < prend->space->dims; i++){
                err = fus_lexer_expect_int(lexer, &v[i]);
                if(err)return err;
            }
            err = fus_lexer_expect(lexer, ")");
            if(err)return err;

            /* trf.rot */
            err = fus_lexer_expect_int(lexer, &rot);
            if(err)return err;

            /* trf.flip */
            err = fus_lexer_next(lexer);
            if(err)return err;
            if(fus_lexer_got(lexer, "t"))flip = true;
            else if(fus_lexer_got(lexer, "f"))flip = false;
            else return fus_lexer_unexpected(lexer, "t or f");

            err = fus_lexer_next(lexer);
            if(err)return err;

            /* palmapper */
            if(fus_lexer_got_str(lexer)){
                err = fus_lexer_expect_str(lexer, &palmapper_name);
                if(err)return err;
                err = fus_lexer_next(lexer);
                if(err)return err;
            }

            /* animation */
            if(!fus_lexer_got(lexer, ")")){
                frame_i_additive = false;
                err = fus_lexer_get_int(lexer, &frame_i);
                if(err)return err;
                err = fus_lexer_next(lexer);
                if(err)return err;
                if(fus_lexer_got(lexer, "+")){
                    frame_i_additive = true;
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                }
                if(!fus_lexer_got(lexer, ")")){
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;
                    err = fus_lexer_expect_int(lexer, &frame_start);
                    if(err)return err;
                    err = fus_lexer_expect_int(lexer, &frame_len);
                    if(err)return err;
                    err = fus_lexer_expect(lexer, ")");
                    if(err)return err;
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                }
            }

            err = fus_lexer_get(lexer, ")");
            if(err)return err;

            if(palmapper_name != NULL){
                palmapper = prismelrenderer_get_palmapper(
                    prend, palmapper_name);
                if(palmapper == NULL){
                    fprintf(stderr, "Couldn't find palette mapper: %s\n",
                        palmapper_name);
                    free(palmapper_name); return 2;}
            }

            rendergraph_t *found = prismelrenderer_get_rendergraph(
                prend, name);
            if(found == NULL){
                fprintf(stderr, "Couldn't find shape: %s\n", name);
                free(name); return 2;}
            free(name);

            rendergraph_trf_t *rendergraph_trf;
            err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
            if(err)return err;
            rendergraph_trf->rendergraph = found;
            rendergraph_trf->palmapper = palmapper;
            rendergraph_trf->trf.rot = rot;
            rendergraph_trf->trf.flip = flip;
            rendergraph_trf->frame_start = frame_start;
            rendergraph_trf->frame_len = frame_len;
            rendergraph_trf->frame_i = frame_i;
            rendergraph_trf->frame_i_additive = frame_i_additive;
            vec_cpy(prend->space->dims, rendergraph_trf->trf.add, v);
        }else{
            err = fus_lexer_unexpected(lexer, "(...)");
            return err;
        }
    }
    return 0;
}

static int parse_shape_prismels(prismelrenderer_t *prend, fus_lexer_t *lexer,
    rendergraph_t *rgraph
){
    /*
        Example data:

            : tri (0 0 0 0)  0 f 0
            : tri (1 0 0 0) 11 f 1
            : sq  (1 0 0 0)  1 f 2 (3 1)
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
            int frame_start = 0;
            int frame_len = -1;

            err = fus_lexer_expect_str(lexer, &name);
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
            else return fus_lexer_unexpected(lexer, "t or f");

            err = fus_lexer_expect_int(lexer, &color);
            if(err)return err;

            err = fus_lexer_next(lexer);
            if(err)return err;
            if(!fus_lexer_got(lexer, ")")){
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_expect_int(lexer, &frame_start);
                if(err)return err;
                err = fus_lexer_expect_int(lexer, &frame_len);
                if(err)return err;
                err = fus_lexer_expect(lexer, ")");
                if(err)return err;
                err = fus_lexer_next(lexer);
                if(err)return err;
            }

            err = fus_lexer_get(lexer, ")");
            if(err)return err;

            prismel_t *found = prismelrenderer_get_prismel(prend, name);
            if(found == NULL){
                fprintf(stderr, "Couldn't find prismel: %s\n", name);
                return 2;
            }
            free(name);

            prismel_trf_t *prismel_trf;
            err = rendergraph_push_prismel_trf(rgraph, &prismel_trf);
            if(err)return err;
            prismel_trf->prismel = found;
            prismel_trf->trf.rot = rot;
            prismel_trf->trf.flip = flip;
            prismel_trf->color = color;
            prismel_trf->frame_start = frame_start;
            prismel_trf->frame_len = frame_len;
            vec_cpy(prend->space->dims, prismel_trf->trf.add, v);
        }else{
            err = fus_lexer_unexpected(lexer, "(...)");
            return err;
        }
    }
    return 0;
}


int fus_lexer_expect_rendergraph(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, rendergraph_t **rgraph_ptr
){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get_rendergraph(lexer, prend, name, rgraph_ptr);
}

int fus_lexer_get_rendergraph(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, rendergraph_t **rgraph_ptr
){
    int err;
    rendergraph_t *rgraph = NULL;

    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_next(lexer);
    if(err)return err;

    if(fus_lexer_got_str(lexer)){
        char *name;
        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;
        rgraph = prismelrenderer_get_rendergraph(prend, name);
        if(rgraph == NULL){
            fprintf(stderr, "Couldn't find shape: %s\n", name);
            free(name); return 2;}
        free(name);

        err = fus_lexer_next(lexer);
        if(err)return err;
        goto ok;
    }

    if(fus_lexer_got(lexer, "map")){
        prismelmapper_t *mapper;
        err = fus_lexer_expect_mapper(lexer, prend, NULL,
            &mapper);
        if(err)return err;

        rendergraph_t *mapped_rgraph;
        err = fus_lexer_expect_rendergraph(lexer, prend, NULL,
            &mapped_rgraph);
        if(err)return err;

        err = prismelmapper_apply_to_rendergraph(mapper, prend, mapped_rgraph,
            name, prend->space, &rgraph);
        if(err)return err;

        err = fus_lexer_next(lexer);
        if(err)return err;
        goto ok;
    }

    const char *animation_type = rendergraph_animation_type_cycle;
    int n_frames = 1;

    if(fus_lexer_got(lexer, "animation")){
        err = fus_lexer_expect(lexer, "(");
        if(err)return err;

        err = fus_lexer_next(lexer);
        if(err)return err;
        const char **animation_type_ptr = rendergraph_animation_types;
        while(*animation_type_ptr != NULL){
            if(fus_lexer_got(lexer, *animation_type_ptr))break;
            animation_type_ptr++;
        }
        if(*animation_type_ptr == NULL){
            return fus_lexer_unexpected(lexer, "<animation_type>");}
        animation_type = *animation_type_ptr;

        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get_int(lexer, &n_frames);
        if(err)return err;

        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    rgraph = calloc(1, sizeof(rendergraph_t));
    if(rgraph == NULL)return 1;
    if(!name){name = generate_indexed_name("shape",
        prend->rendergraphs_len);}
    err = rendergraph_init(rgraph, name, prend->space,
        animation_type, n_frames);
    if(err)return err;

    ARRAY_PUSH(rendergraph_t, *prend, rendergraphs, rgraph)

    while(1){
        if(fus_lexer_got(lexer, ")")){
            break;
        }else if(fus_lexer_got(lexer, "shapes")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_shape_shapes(prend, lexer, rgraph);
            if(err)return err;
        }else if(fus_lexer_got(lexer, "prismels")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_shape_prismels(prend, lexer, rgraph);
            if(err)return err;
        }else{
            err = fus_lexer_unexpected(lexer, "shapes or prismels");
            return err;
        }

        err = fus_lexer_next(lexer);
        if(err)return err;
    }

ok:
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    *rgraph_ptr = rgraph;
    return 0;
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

        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;

        if(prismelrenderer_get_rendergraph(prend, name) != NULL){
            fprintf(stderr, "Shape %s already defined\n", name);
            return 2;
        }

        rendergraph_t *rgraph;
        err = fus_lexer_expect_rendergraph(lexer, prend, name, &rgraph);
        if(err)return err;
    }
    return 0;
}



/**********
 * MAPPER *
 **********/

int fus_lexer_expect_mapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, prismelmapper_t **mapper_ptr
){
    int err = fus_lexer_next(lexer);
    if(err)return err;
    return fus_lexer_get_mapper(lexer, prend, name, mapper_ptr);
}

int fus_lexer_get_mapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, prismelmapper_t **mapper_ptr
){
    int err;
    prismelmapper_t *mapper = NULL;

    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_next(lexer);
    if(err)return err;

    if(fus_lexer_got_str(lexer)){
        char *name;
        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;
        mapper = prismelrenderer_get_mapper(prend, name);
        if(mapper == NULL){
            fprintf(stderr, "Couldn't find mapper: %s\n", name);
            free(name); return 2;}
        free(name);

        err = fus_lexer_next(lexer);
        if(err)return err;
        goto ok;
    }

    if(fus_lexer_got(lexer, "map")){
        prismelmapper_t *mapper1;
        err = fus_lexer_expect_mapper(lexer, prend, NULL, &mapper1);
        if(err)return err;

        prismelmapper_t *mapper2;
        err = fus_lexer_expect_mapper(lexer, prend, NULL, &mapper2);
        if(err)return err;

        err = prismelmapper_apply_to_mapper(mapper2, prend, mapper1,
            name, prend->space, &mapper);
        if(err)return err;

        err = fus_lexer_next(lexer);
        if(err)return err;
        goto ok;
    }

    err = fus_lexer_get(lexer, "unit");
    if(err)return err;

    mapper = calloc(1, sizeof(*mapper));
    if(mapper == NULL)return 1;
    if(!name){name = generate_indexed_name("mapper",
        prend->mappers_len);}
    err = prismelmapper_init(mapper, name, prend->space);
    if(err)return err;

    ARRAY_PUSH(prismelmapper_t, *prend, mappers, mapper)

    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    for(int i = 0; i < prend->space->dims; i++){
        err = fus_lexer_expect_int(lexer, &mapper->unit[i]);
        if(err)return err;
    }
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;
    err = fus_lexer_next(lexer);
    if(err)return err;

    if(fus_lexer_got(lexer, "entries")){
        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        while(1){
            err = fus_lexer_next(lexer);
            if(err)return err;

            if(fus_lexer_got(lexer, ")"))break;

            err = fus_lexer_get(lexer, "(");
            if(err)return err;

            char *prismel_name;
            err = fus_lexer_expect_str(lexer, &prismel_name);
            if(err)return err;
            prismel_t *prismel = prismelrenderer_get_prismel(
                prend, prismel_name);
            if(prismel == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find prismel: %s\n", prismel_name);
                err = 2; return err;}

            err = fus_lexer_expect(lexer, "->");
            if(err)return err;

            char *rgraph_name;
            err = fus_lexer_expect_str(lexer, &rgraph_name);
            if(err)return err;
            rendergraph_t *rgraph = prismelrenderer_get_rendergraph(
                prend, rgraph_name);
            if(rgraph == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find shape: %s\n", rgraph_name);
                err = 2; return err;}

            err = prismelmapper_push_entry(mapper, prismel, rgraph);
            if(err)return err;

            err = fus_lexer_expect(lexer, ")");
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

ok:
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    *mapper_ptr = mapper;
    return 0;
}

static int parse_mappers(prismelrenderer_t *prend, fus_lexer_t *lexer){
    while(1){
        int err;
        char *name;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_got(lexer, ")"))break;

        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;

        if(prismelrenderer_get_mapper(prend, name) != NULL){
            fprintf(stderr, "Mapper %s already defined\n", name);
            return 2;
        }

        prismelmapper_t *mapper;
        err = fus_lexer_expect_mapper(lexer, prend, name, &mapper);
        if(err)return err;
    }
    return 0;
}




/*******************
 * PRISMELRENDERER *
 *******************/

int prismelrenderer_parse(prismelrenderer_t *prend, fus_lexer_t *lexer){
    while(1){
        int err;

        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_done(lexer)){
            break;
        }else if(fus_lexer_got(lexer, "palmappers")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_palmappers(prend, lexer);
            if(err)return err;
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
        }else if(fus_lexer_got(lexer, "mappers")){
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = parse_mappers(prend, lexer);
            if(err)return err;
        }else if(fus_lexer_got(lexer, "import")){
            char *filename;
            err = fus_lexer_expect_str(lexer, &filename);
            if(err)return err;
            err = prismelrenderer_load(prend, filename);
            if(err)return err;
            free(filename);
        }else{
            return fus_lexer_unexpected(lexer,
                "palmappers or prismels or shapes or mappers or import");
        }
    }
    return 0;
}

