
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "lexer.h"
#include "lexer_macros.h"
#include "util.h"
#include "array.h"
#include "font.h"
#include "geomfont.h"
#include "prismelrenderer.h"
#include "hexpicture.h"
#include "vec4.h"



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
            err = fus_lexer_get_int_range(lexer, 0, 256,
                &color_i, &n_colors);
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
            : "sixth" (0 0 0 0)  0 f

            # rot = 2, flip = true
            : "sixth" (0 0 0 0)  2 t

            # Animation properties: frame_i (frame_start frame_length)
            # ...where:
            #   frame_i: what frame of sub-rgraph is used
            #   frame_start: sub-rgraph is shown starting on this frame
            #   frame_length: sub-rgraph is shown for this many frames
            : "sixth" (0 0 0 0)  4 f  0 (0 1)

            # The "+" makes frame_i "additive", that is, rgraph's frame_i
            # is added to sub-rgraph's frame_i
            : "sixth" (0 0 0 0)  4 f  0+ (0 1)

            # Adding a palmapper, "red" in this case
            : "sixth" (0 0 0 0)  4 f "red"  0+ (0 1)
    */
    INIT
    GET("(")
    while(1){
        if(GOT(")"))break;

        char *name;
        char *palmapper_name = NULL;
        palettemapper_t *palmapper = NULL;
        trf_t trf = {0};
        int frame_start = 0;
        int frame_len = -1;
        int frame_i = 0;
        bool frame_i_additive = true;
        bool frame_i_reversed = false;

        GET("(")
        {
            GET_STR(name)
            GET_TRF(prend->space, trf)

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
                if(GOT("r")){
                    NEXT
                    frame_i_reversed = true;
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
        rendergraph_trf->trf = trf;
        rendergraph_trf->frame_start = frame_start;
        rendergraph_trf->frame_len = frame_len;
        rendergraph_trf->frame_i = frame_i;
        rendergraph_trf->frame_i_additive = frame_i_additive;
        rendergraph_trf->frame_i_reversed = frame_i_reversed;
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


static int parse_shape_hexpicture(prismelrenderer_t *prend, fus_lexer_t *lexer,
    rendergraph_t *rgraph
){
    INIT
    GET("(")

    static const char *names[3] = {"sq", "tri", "dia"};
    prismel_t *prismels[3];
    for(int i = 0; i < 3; i++){
        prismels[i] = prismelrenderer_get_prismel(prend, names[i]);
        if(prismels[i] == NULL){
            fprintf(stderr, "Couldn't find prismel: %s\n", names[i]);
            return 2;
        }
    }

    ARRAY_DECL(char *, lines)
    ARRAY_INIT(lines)

    while(1){
        if(GOT(")"))break;

        char *line;
        GET_STR(line)
        ARRAY_PUSH(char *, lines, line)
    }
    NEXT

    bool verbose = false;
    hexpicture_return_face_t *faces;
    size_t faces_len;
    err = hexpicture_parse(
        &faces, &faces_len,
        (const char **)lines, lines_len,
        verbose);
    if(err)return err;

    for(int i = 0; i < faces_len; i++){
        hexpicture_return_face_t *face = &faces[i];
        prismel_trf_t *prismel_trf;
        err = rendergraph_push_prismel_trf(rgraph, &prismel_trf);
        if(err)return err;
        prismel_trf->prismel = prismels[face->type];
        prismel_trf->trf.rot = face->rot;
        prismel_trf->trf.flip = false;
        prismel_trf->color = face->color;
        prismel_trf->frame_start = 0;
        prismel_trf->frame_len = -1;
        vec4_set(prismel_trf->trf.add,
            face->a, face->b, face->c, face->d);
    }

    free(faces);
    ARRAY_FREE_PTR(char *, lines, (void))
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
        if(*animation_type_ptr == NULL)return UNEXPECTED("<animation_type>");
        animation_type = *animation_type_ptr;
        if(GOT_STR){
            char *name;
            GET_STR(name)
            rendergraph_t *rgraph = prismelrenderer_get_rgraph(prend, name);
            if(!rgraph)return UNEXPECTED("shape name");
            n_frames = rgraph->n_frames;
        }else{
            GET_INT(n_frames)
        }
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
        }else if(GOT("hexpicture")){
            NEXT
            err = parse_shape_hexpicture(prend, lexer, rgraph);
            if(err == 2)fprintf(stderr, "While parsing shape: %s\n", name);
            if(err)return err;
        }else if(GOT("text")){
            NEXT
            GET("(")

            char *geomfont_name;
            GET_STR(geomfont_name)

            geomfont_t *geomfont = prismelrenderer_get_geomfont(prend, geomfont_name);
            if(geomfont == NULL){
                fprintf(stderr, "%s: Couldn't find geomfont: %s\n",
                    lexer->filename, geomfont_name);
                return 2;
            }
            free(geomfont_name);

            char *text;
            GET_STR(text)

            int cx = 0, cy = 0;
            if(GOT("(")){
                NEXT
                GET_INT(cx)
                GET_INT(cy)
                GET(")")
            }

            err = geomfont_rgraph_printf(geomfont, rgraph, cx, cy,
                NULL, "%s", text);
            if(err)return err;
            free(text);

            GET(")")
            goto ok;
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
        if(GOT(")"))break;
        char *name;
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

static int parse_geomfonts(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    while(1){
        if(GOT(")"))break;

        char *name;
        GET_STR(name)
        if(prismelrenderer_get_geomfont(prend, name) != NULL){
            fprintf(stderr, "Geomfont %s already defined\n", name);
            return 2;
        }

        GET("(")

        /* MAYBE TODO:
        Allow defining a geomfont in terms of another by way of
        prismelmappers/palettemappers.
        E.g.
            "curvy_font1": map ("font1") ("curvy")
            "red.font1": palmap ("font1") ("red")
        In order to make this work, we'll want to add memoization for it...
        That is, struct prismelrenderer currently has:
            ARRAY_DECL(prismelmapper_application_t*, applications)
            ARRAY_DECL(prismelmapper_mapplication_t*, mapplications)
        ...and we'll want to add:
            ARRAY_DECL(prismelmapper_gfapplication_t*, gfapplications)
        ...where:
            typedef struct prismelmapper_gfapplication {
                geomfont_t *mapped_geomfont;
                geomfont_t *resulting_geomfont;
            } prismelmapper_gfapplication_t;
        ...yeah?
        If we're going to do that, can we do a refactor first so we don't
        just copy-paste applications and mapplications?
        Maybe something like...
            MEMOIZED_MAPPING_DECL(gfapplication, geomfont)
        ...oh man yeah, because also palettemapper_t has:
            ARRAY_DECL(struct palettemapper_pmapplication*, pmapplications)
            ARRAY_DECL(struct palettemapper_application*, applications)

        ...HOWEVER, HERE IS WHY WE MIGHT NOT WANT TO BOTHER:
        * We could always just apply our mappers to the text rgraphs we
          generate using a given geomfont.
          Simple solution, clearly a workaround, but not even a hack.
        */

        font_t *font;
        GET("font")
        GET("(")
        {
            char *font_filename;
            GET_STR(font_filename)
            err = prismelrenderer_get_or_create_font(
                prend, font_filename, &font);
            if(err)return err;
            free(font_filename);
        }
        GET(")")

        GET("prismel")
        GET("(")
        {
            char *prismel_name;
            GET_STR(prismel_name)

            vec_t vx, vy;
            GET_VEC(prend->space, vx)
            GET_VEC(prend->space, vy)

            ARRAY_PUSH_NEW(geomfont_t*, prend->geomfonts, geomfont)
            err = geomfont_init(geomfont, name, font,
                prend, prismel_name, vx, vy);
            if(err)return err;

            free(prismel_name);
        }
        GET(")")

        GET(")")
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
        }else if(GOT("geomfonts")){
            NEXT
            GET("(")
            err = parse_geomfonts(prend, lexer);
            if(err)return err;
        }else if(GOT("import")){
            NEXT

            /* We use _fus_lexer_get_str to avoid calling fus_lexer_next until after
            the call to prismelrenderer_load is done, to make sure we don't modify
            lexer->vars first */
            char *filename;
            err = _fus_lexer_get_str(lexer, &filename);
            if(err)return err;

            err = prismelrenderer_load(prend, filename, lexer->vars);
            if(err)return err;

            /* We now call fus_lexer_next manually, see call to _fus_lexer_get_str
            above */
            err = fus_lexer_next(lexer);
            if(err)return err;

            free(filename);
        }else{
            return UNEXPECTED(
                "palmappers or prismels or shapes or mappers or geomfonts "
                "or import");
        }
    }
    return 0;
}

