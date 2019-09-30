
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "lexer_macros.h"
#include "util.h"
#include "array.h"
#include "prismelrenderer.h"



/******************
 * PALETTE MAPPER *
 ******************/

static int parse_palmappers(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    while(1){
        char *name;
        if(GOT(")"))break;
        GET_STR(name)
        palettemapper_t *palmapper;
        err = fus_lexer_get_palettemapper(lexer, prend, name, &palmapper);
        if(err)return err;
    }
    NEXT
    return 0;
}

int fus_lexer_get_palettemapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, palettemapper_t **palmapper_ptr
){
    INIT
    palettemapper_t *palmapper = NULL;
    GET("(")

    if(GOT_STR){
        char *name;
        GET_STR(name)
        palmapper = prismelrenderer_get_palmapper(prend, name);
        if(palmapper == NULL){
            fprintf(stderr, "Couldn't find palette mapper: %s\n", name);
            free(name); return 2;}
        free(name);
        GET(")")
        goto ok;
    }

    if(GOT("map")){
        NEXT

        palettemapper_t *palmapper1;
        err = fus_lexer_get_palettemapper(lexer, prend, NULL,
            &palmapper1);
        if(err)return err;

        palettemapper_t *palmapper2;
        err = fus_lexer_get_palettemapper(lexer, prend, NULL,
            &palmapper2);
        if(err)return err;

        err = palettemapper_apply_to_palettemapper(palmapper1, prend,
            palmapper2, name, &palmapper);
        if(err)return err;

        GET(")")
        goto ok;
    }

    ARRAY_PUSH_NEW(palettemapper_t*, prend->palmappers, _palmapper)
    palmapper = _palmapper;
    err = palettemapper_init(palmapper, strdup(name), -1);
    if(err)return err;

    Uint8 *table = palmapper->table;
    int color_i = 0;

    while(1){
        if(GOT(")"))break;

        int n_colors = 1;

        if(!GOT("(")){
            err = fus_lexer_get_int_range(lexer, 256, &color_i, &n_colors);
            if(err)return err;
        }

        if(color_i >= 256){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Color index went out of bounds\n");
            return 2;
        }

        GET("(")
        {
            if(!GOT(")")){
                int color;
                GET_INT_FANCY(color)
                if(color < 0 || color >= 256){
                    return UNEXPECTED(
                        "int within 0..255");}
                for(int i = 0; i < n_colors; i++){
                    table[color_i] = color;
                    color_i++;
                }
            }else{
                for(int i = 0; i < n_colors; i++){
                    table[color_i] = color_i;
                    color_i++;
                }
            }
        }
        GET(")")
    }
    NEXT

ok:
    *palmapper_ptr = palmapper;
    return 0;
}


/***********
 * PRISMEL *
 ***********/

static int parse_prismel_image(prismel_t *prismel, fus_lexer_t *lexer,
    int image_i
){
    INIT
    prismel_image_t *image = &prismel->images[image_i];
    GET("(")

    if(GOT_INT){
        /* For example:
            6 +( 4  0) */

        int i;
        GET_INT(i)
        if(i < 0 || i >= image_i){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Prismel image reference out "
                "of bounds: %i not in 0..%i\n",
                i, image_i - 1);
            return 2;
        }
        prismel_image_t *other_image = &prismel->images[i];

        int add_x = 0, add_y = 0;
        if(GOT("+")){
            NEXT
            GET("(")
            GET_INT(add_x)
            GET_INT(add_y)
            GET(")")
        }

        GET(")")

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
            if(GOT("(")){
                int x, y, w;
                NEXT
                GET_INT(x)
                GET_INT(y)
                GET_INT(w)
                GET(")")
                err = prismel_image_push_line(image, x, y, w);
                if(err)return err;
            }else if(GOT(")")){
                break;
            }else{
                return UNEXPECTED("(...)");
            }
        }
        NEXT
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
    INIT

    prismel_t *prismel;
    err = prismelrenderer_push_prismel(prend, name, &prismel);
    if(err)return err;

    GET("(")
    while(1){
        if(GOT(")")){
            break;
        }else if(GOT("images")){
            NEXT
            GET("(")
            for(int i = 0; i < prismel->n_images; i++){
                err = parse_prismel_image(prismel, lexer, i);
                if(err)return err;
            }
            GET(")")
        }else{
            return UNEXPECTED("images");
        }
    }
    NEXT

    *prismel_ptr = prismel;
    return 0;
}

static int parse_prismels(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    int n_images = prend->space->rot_max;
    while(1){
        char *name;
        if(GOT(")"))break;
        GET_STR(name)
        prismel_t *prismel;
        err = fus_lexer_get_prismel(lexer, prend, name, &prismel);
        if(err)return err;
    }
    NEXT
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

        :
            : sixth (0 0 0 0)  0 f
            : sixth (0 0 0 0)  2 f
            : sixth (0 0 0 0)  4 f  0 (0 1)
            : sixth (0 0 0 0)  4 f  0+ (0 1)
            : sixth (0 0 0 0)  6 f "red"  0 (0 1)
            : sixth (0 0 0 0)  6 f "red"  0+ (0 1)
    */
    INIT
    GET("(")
    while(1){
        if(GOT(")"))break;

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

        GET("(")
        {
            /* name */
            GET_STR(name)

            /* trf.add */
            GET_VEC(prend->space, v)

            /* trf.rot */
            GET_INT(rot)

            /* trf.flip */
            GET_BOOL(flip)

            /* palmapper */
            if(GOT_STR){
                GET_STR(palmapper_name)
            }

            /* animation */
            if(GOT_INT){
                frame_i_additive = false;
                GET_INT(frame_i)
                if(GOT("+")){
                    NEXT
                    frame_i_additive = true;
                }
                if(GOT("(")){
                    NEXT
                    GET_INT(frame_start)
                    GET_INT(frame_len)
                    GET(")")
                }
            }
        }
        GET(")")

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
    }
    NEXT
    return 0;
}

static int parse_shape_prismels(prismelrenderer_t *prend, fus_lexer_t *lexer,
    rendergraph_t *rgraph
){
    /*
        Example data:

        :
            : tri (0 0 0 0)  0 f 0
            : tri (1 0 0 0) 11 f 1
            : sq  (1 0 0 0)  1 f 2 (3 1)
    */
    INIT
    GET("(")
    while(1){
        if(GOT(")"))break;

        char *name;
        int dims = 0;
        vec_t v;
        int rot;
        bool flip;
        int color;
        int frame_start = 0;
        int frame_len = -1;

        GET("(")
        {
            GET_STR(name)
            GET_VEC(prend->space, v)
            GET_INT(rot)
            GET_BOOL(flip)
            GET_INT_FANCY(color)
            if(GOT("(")){
                NEXT
                GET_INT(frame_start)
                GET_INT(frame_len)
                GET(")")
            }
        }
        GET(")")

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
    }
    NEXT
    return 0;
}


int fus_lexer_get_rendergraph(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, rendergraph_t **rgraph_ptr
){
    INIT
    rendergraph_t *rgraph = NULL;

    GET("(")

    if(GOT_STR){
        char *name;
        GET_STR(name)
        rgraph = prismelrenderer_get_rendergraph(prend, name);
        if(rgraph == NULL){
            fprintf(stderr, "Couldn't find shape: %s\n", name);
            free(name); return 2;}
        free(name);
        goto ok;
    }

    if(GOT("map")){
        NEXT

        prismelmapper_t *mapper;
        err = fus_lexer_get_mapper(lexer, prend, NULL,
            &mapper);
        if(err)return err;

        rendergraph_t *mapped_rgraph;
        err = fus_lexer_get_rendergraph(lexer, prend, NULL,
            &mapped_rgraph);
        if(err)return err;

        err = prismelmapper_apply_to_rendergraph(mapper, prend, mapped_rgraph,
            name, prend->space, NULL, &rgraph);
        if(err)return err;

        goto ok;
    }

    const char *animation_type = rendergraph_animation_type_cycle;
    int n_frames = 1;

    if(GOT("animation")){
        NEXT
        GET("(")

        const char **animation_type_ptr = rendergraph_animation_types;
        while(*animation_type_ptr != NULL){
            if(GOT(*animation_type_ptr))break;
            animation_type_ptr++;
        }
        NEXT
        if(*animation_type_ptr == NULL){
            return UNEXPECTED("<animation_type>");}
        animation_type = *animation_type_ptr;
        GET_INT(n_frames)
        GET(")")
    }

    rgraph = calloc(1, sizeof(rendergraph_t));
    if(rgraph == NULL)return 1;
    if(!name){name = generate_indexed_name("shape",
        prend->rendergraphs_len);}
    err = rendergraph_init(rgraph, name, prend, NULL,
        animation_type, n_frames);
    if(err)return err;

    ARRAY_PUSH(rendergraph_t*, prend->rendergraphs, rgraph)

    while(1){
        if(GOT(")")){
            break;
        }else if(GOT("shapes")){
            NEXT
            err = parse_shape_shapes(prend, lexer, rgraph);
            if(err)return err;
        }else if(GOT("prismels")){
            NEXT
            err = parse_shape_prismels(prend, lexer, rgraph);
            if(err)return err;
        }else{
            err = UNEXPECTED("shapes or prismels");
            return err;
        }
    }

ok:
    GET(")")

    *rgraph_ptr = rgraph;
    return 0;
}

static int parse_shapes(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    while(1){
        char *name;
        if(GOT(")"))break;
        GET_STR(name)
        if(prismelrenderer_get_rendergraph(prend, name) != NULL){
            fprintf(stderr, "Shape %s already defined\n", name);
            return 2;
        }

        rendergraph_t *rgraph;
        err = fus_lexer_get_rendergraph(lexer, prend, name, &rgraph);
        if(err)return err;
    }
    NEXT
    return 0;
}



/**********
 * MAPPER *
 **********/

int fus_lexer_get_mapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, char *name, prismelmapper_t **mapper_ptr
){
    INIT
    prismelmapper_t *mapper = NULL;

    GET("(")

    if(GOT_STR){
        char *name;
        GET_STR(name)
        mapper = prismelrenderer_get_mapper(prend, name);
        if(mapper == NULL){
            fprintf(stderr, "Couldn't find mapper: %s\n", name);
            free(name); return 2;}
        free(name);
        goto ok;
    }

    bool solid = false;
    if(GOT("solid")){
        NEXT
        solid = true;
    }

    if(GOT("map")){
        NEXT
        prismelmapper_t *mapper1;
        err = fus_lexer_get_mapper(lexer, prend, NULL, &mapper1);
        if(err)return err;

        prismelmapper_t *mapper2;
        err = fus_lexer_get_mapper(lexer, prend, NULL, &mapper2);
        if(err)return err;

        err = prismelmapper_apply_to_mapper(mapper1, prend, mapper2,
            name, prend->space, &mapper);
        if(err)return err;

        /* umm kind of hack. But so is our whole mapper->name thing,
        really we want an array of mapper_entries on prend, where the
        entries have names instead of the mappers they point to. */
        if(solid)mapper->solid = true;

        goto ok;
    }

    GET("unit")

    mapper = calloc(1, sizeof(*mapper));
    if(mapper == NULL)return 1;
    if(!name){name = generate_indexed_name("mapper",
        prend->mappers_len);}
    err = prismelmapper_init(mapper, name, prend->space, solid);
    if(err)return err;

    ARRAY_PUSH(prismelmapper_t*, prend->mappers, mapper)
    GET_VEC(prend->space, mapper->unit)

    if(GOT("entries")){
        NEXT
        GET("(")
        while(1){
            if(GOT(")"))break;
            GET("(")
            char *prismel_name;
            GET_STR(prismel_name)
            prismel_t *prismel = prismelrenderer_get_prismel(
                prend, prismel_name);
            if(prismel == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find prismel: %s\n", prismel_name);
                err = 2; return err;}

            GET("->")

            char *rgraph_name;
            GET_STR(rgraph_name)
            rendergraph_t *rgraph = prismelrenderer_get_rendergraph(
                prend, rgraph_name);
            if(rgraph == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find shape: %s\n", rgraph_name);
                err = 2; return err;}

            err = prismelmapper_push_entry(mapper, prismel, rgraph);
            if(err)return err;

            GET(")")
        }
        NEXT
    }

ok:
    GET(")")

    *mapper_ptr = mapper;
    return 0;
}

static int parse_mappers(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    while(1){
        char *name;
        if(GOT(")"))break;
        GET_STR(name)
        if(prismelrenderer_get_mapper(prend, name) != NULL){
            fprintf(stderr, "Mapper %s already defined\n", name);
            return 2;
        }

        prismelmapper_t *mapper;
        err = fus_lexer_get_mapper(lexer, prend, name, &mapper);
        if(err)return err;
    }
    NEXT
    return 0;
}




/*******************
 * PRISMELRENDERER *
 *******************/

int prismelrenderer_parse(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    while(1){
        if(DONE){
            break;
        }else if(GOT("palmappers")){
            NEXT
            GET("(")
            err = parse_palmappers(prend, lexer);
            if(err)return err;
        }else if(GOT("prismels")){
            NEXT
            GET("(")
            err = parse_prismels(prend, lexer);
            if(err)return err;
        }else if(GOT("shapes")){
            NEXT
            GET("(")
            err = parse_shapes(prend, lexer);
            if(err)return err;
        }else if(GOT("mappers")){
            NEXT
            GET("(")
            err = parse_mappers(prend, lexer);
            if(err)return err;
        }else if(GOT("import")){
            NEXT
            char *filename;
            GET_STR(filename)
            err = prismelrenderer_load(prend, filename);
            if(err)return err;
            free(filename);
        }else{
            return UNEXPECTED(
                "palmappers or prismels or shapes or mappers or import");
        }
    }
    return 0;
}

