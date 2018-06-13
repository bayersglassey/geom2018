
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "util.h"
#include "array.h"
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
                        return fus_lexer_unexpected(lexer, "(...)");
                    }
                }
            }
            err = fus_lexer_expect(lexer, ")");
            if(err)return err;
        }else{
            return fus_lexer_unexpected(lexer, "images");
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

        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;

        prismel_t *prismel;
        err = prismelrenderer_push_prismel(prend, name, &prismel);
        if(err)return err;

        err = parse_prismel(prismel, lexer);
        if(err)return err;
    }
    return 0;
}

static int parse_shape_shapes(prismelrenderer_t *prend, fus_lexer_t *lexer,
    rendergraph_t *rgraph
){
    /*
        Example data:

            : sixth (0 0 0 0)  0 f
            : sixth (0 0 0 0)  2 f
            : sixth (0 0 0 0)  4 f  0 (0 1)
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
            int frame_start = 0;
            int frame_len = -1;
            int frame_i = 0;
            bool frame_i_additive = true;

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

            err = fus_lexer_next(lexer);
            if(err)return err;
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

            rendergraph_t *found = prismelrenderer_get_rendergraph(
                prend, name);
            if(found == NULL){
                fprintf(stderr, "Couldn't find shape: %s\n", name);
                return 2;
            }
            free(name);

            rendergraph_trf_t *rendergraph_trf;
            err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
            if(err)return err;
            rendergraph_trf->rendergraph = found;
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

static int parse_shape(prismelrenderer_t *prend, fus_lexer_t *lexer,
    const char *name, rendergraph_t **rgraph_ptr
){
    int err;
    rendergraph_t *rgraph = NULL;

    const char *animation_type = rendergraph_animation_type_cycle;
    int n_frames = 1;

    err = fus_lexer_next(lexer);
    if(err)return err;
    if(fus_lexer_got(lexer, "map")){
        err = fus_lexer_expect(lexer, "(");
        if(err)return err;

        char *mapper_name;
        err = fus_lexer_expect_str(lexer, &mapper_name);
        if(err)return err;
        prismelmapper_t *mapper = prismelrenderer_get_mapper(prend,
            mapper_name);
        if(mapper == NULL){
            fprintf(stderr, "Couldn't find mapper: %s\n", mapper_name);
            free(mapper_name); return 2;}
        free(mapper_name);

        char *mapped_rgraph_name;
        err = fus_lexer_expect_str(lexer, &mapped_rgraph_name);
        if(err)return err;
        rendergraph_t *mapped_rgraph = prismelrenderer_get_rendergraph(prend,
            mapped_rgraph_name);
        if(mapped_rgraph == NULL){
            fprintf(stderr, "Couldn't find shape: %s\n", mapped_rgraph_name);
            free(mapped_rgraph_name); return 2;}
        free(mapped_rgraph_name);

        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
        err = fus_lexer_next(lexer);
        if(err)return err;

        err = prismelmapper_apply_to_rendergraph(mapper, prend, mapped_rgraph,
            strdup(name), prend->space, &rgraph);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "animation")){
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

    if(rgraph == NULL){
        rgraph = calloc(1, sizeof(rendergraph_t));
        if(rgraph == NULL)return 1;
        err = rendergraph_init(rgraph, strdup(name), prend->space,
            animation_type, n_frames);
        if(err)return err;

        ARRAY_PUSH(rendergraph_t, *prend, rendergraphs, rgraph)
    }

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

        err = fus_lexer_expect(lexer, "(");
        if(err)return err;

        rendergraph_t *rgraph;
        err = parse_shape(prend, lexer, name, &rgraph);
        if(err)return err;
    }
    return 0;
}


static int parse_mapper(prismelrenderer_t *prend, fus_lexer_t *lexer,
    const char *name, prismelmapper_t **mapper_ptr
){
    int err;
    prismelmapper_t *mapper = NULL;

    err = fus_lexer_expect(lexer, "(");
    if(err)return err;

    err = fus_lexer_next(lexer);
    if(err)return err;
    if(fus_lexer_got(lexer, "map")){
        err = fus_lexer_expect(lexer, "(");
        if(err)return err;

        char *mapper1_name;
        err = fus_lexer_expect_str(lexer, &mapper1_name);
        if(err)return err;
        prismelmapper_t *mapper1 = prismelrenderer_get_mapper(prend,
            mapper1_name);
        if(mapper1 == NULL){
            fprintf(stderr, "Couldn't find mapper: %s\n", mapper1_name);
            free(mapper1_name); return 2;}
        free(mapper1_name);

        char *mapper2_name;
        err = fus_lexer_expect_str(lexer, &mapper2_name);
        if(err)return err;
        prismelmapper_t *mapper2 = prismelrenderer_get_mapper(prend,
            mapper2_name);
        if(mapper2 == NULL){
            fprintf(stderr, "Couldn't find mapper: %s\n", mapper2_name);
            free(mapper2_name); return 2;}
        free(mapper2_name);

        err = fus_lexer_expect(lexer, ")");
        if(err)return err;
        err = fus_lexer_next(lexer);
        if(err)return err;

        err = prismelmapper_apply_to_mapper(mapper2, prend, mapper1,
            strdup(name), prend->space, &mapper);
        if(err)return err;
    }else{

        mapper = calloc(1, sizeof(*mapper));
        if(mapper == NULL)return 1;
        err = prismelmapper_init(mapper, strdup(name), prend->space);
        if(err)return err;

        err = fus_lexer_get(lexer, "unit");
        if(err)return err;
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
    }

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
        err = parse_mapper(prend, lexer, name, &mapper);
        if(err)return err;

        ARRAY_PUSH(prismelmapper_t, *prend, mappers, mapper)
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
            return fus_lexer_unexpected(lexer, "shapes or prismels or mappers or import");
        }
    }
    return 0;
}

