
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
#include "geom_lexer_utils.h"



/******************
 * PALETTE MAPPER *
 ******************/

static int parse_palmappers(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    while(1){
        const char *name;
        if(GOT(")"))break;
        GET_STR_CACHED(name, &prend->name_store)

        palettemapper_t *palmapper = prismelrenderer_get_palmapper(prend, name);
        if(!prend->loaded && palmapper != NULL){
            fprintf(stderr, "Palmapper %s already defined\n", name);
            return 2;
        }

        err = fus_lexer_get_palettemapper(lexer, prend, name, &palmapper);
        if(err)return err;
    }
    NEXT
    return 0;
}

int fus_lexer_get_palettemapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, const char *name, palettemapper_t **palmapper_ptr
){
    INIT
    palettemapper_t *palmapper = NULL;
    OPEN

    if(GOT_STR){
        const char *name;
        GET_STR_CACHED(name, &prend->name_store)
        palmapper = prismelrenderer_get_palmapper(prend, name);
        if(palmapper == NULL){
            fprintf(stderr, "Couldn't find palette mapper: %s\n", name);
            return 2;}
        CLOSE
        goto ok;
    }

    if(GOT("map")){
        NEXT

        palettemapper_t *palmapper1 = NULL;
        err = fus_lexer_get_palettemapper(lexer, prend, NULL,
            &palmapper1);
        if(err)return err;

        palettemapper_t *palmapper2 = NULL;
        err = fus_lexer_get_palettemapper(lexer, prend, NULL,
            &palmapper2);
        if(err)return err;

        err = palettemapper_apply_to_palettemapper(palmapper1, prend,
            palmapper2, name, &palmapper);
        if(err)return err;

        CLOSE
        goto ok;
    }

    palmapper = *palmapper_ptr;
    if(palmapper){
        /* TODO: don't cleanup & re-initialize, because this frees our arrays
        palmapper->applications and palmapper->pmapplications, losing our
        references to existing rgraphs (and probably leaving them dangling).
        Instead, do an explicit re-init which keeps the old rgraphs around,
        but also updates them in-place. */
        palettemapper_cleanup(palmapper);
    }else{
        ARRAY_PUSH_NEW(palettemapper_t*, prend->palmappers, _palmapper)
        palmapper = _palmapper;
    }
    err = palettemapper_init(palmapper, name, -1);
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

        OPEN
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
        CLOSE
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
    OPEN

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
            OPEN
            GET_INT(add_x)
            GET_INT(add_y)
            CLOSE
        }

        CLOSE

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
                CLOSE
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
    prismelrenderer_t *prend, const char *name, prismel_t **prismel_ptr
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

    prismel_t *prismel = *prismel_ptr;
    if(prismel){
        prismel_cleanup(prismel);
        err = prismel_init(prismel, name, prend->space);
        if(err)return err;
    }else{
        err = prismelrenderer_push_prismel(prend, name, &prismel);
        if(err)return err;
    }

    OPEN
    while(1){
        if(GOT(")")){
            break;
        }else if(GOT("images")){
            NEXT
            OPEN
            for(int i = 0; i < prismel->n_images; i++){
                err = parse_prismel_image(prismel, lexer, i);
                if(err)return err;
            }
            CLOSE
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
        const char *name;
        if(GOT(")"))break;
        GET_STR_CACHED(name, &prend->name_store)

        prismel_t *prismel = prismelrenderer_get_prismel(prend, name);
        if(!prend->loaded && prismel != NULL){
            fprintf(stderr, "Prismel %s already defined\n", name);
            return 2;
        }

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
    OPEN
    while(1){
        if(GOT(")"))break;

        const char *name;
        const char *palmapper_name = NULL;
        palettemapper_t *palmapper = NULL;
        trf_t trf = {0};
        int frame_start = 0;
        int frame_len = -1;
        int frame_i = 0;
        bool frame_i_additive = true;
        bool frame_i_reversed = false;

        OPEN
        {
            GET_STR_CACHED(name, &prend->name_store)
            GET_TRF(prend->space, trf)

            /* palmapper */
            if(GOT_STR){
                GET_STR_CACHED(palmapper_name, &prend->name_store)
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
                    CLOSE
                }
            }
        }
        CLOSE

        if(palmapper_name != NULL){
            palmapper = prismelrenderer_get_palmapper(
                prend, palmapper_name);
            if(palmapper == NULL){
                fprintf(stderr, "Couldn't find palette mapper: %s\n",
                    palmapper_name);
                return 2;}
        }

        rendergraph_t *found = prismelrenderer_get_rendergraph(
            prend, name);
        if(found == NULL){
            fprintf(stderr, "Couldn't find shape: %s\n", name);
            return 2;}

        rendergraph_child_t *child;
        err = rendergraph_push_child(rgraph,
            RENDERGRAPH_CHILD_TYPE_RGRAPH,
            &child);
        if(err)return err;
        child->u.rgraph.rendergraph = found;
        child->u.rgraph.palmapper = palmapper;
        child->trf = trf;
        child->frame_start = frame_start;
        child->frame_len = frame_len;
        child->u.rgraph.frame_i = frame_i;
        child->u.rgraph.frame_i_additive = frame_i_additive;
        child->u.rgraph.frame_i_reversed = frame_i_reversed;
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
            : "tri" (0 0 0 0)  0 f 0
            : "tri" (1 0 0 0) 11 f 1
            : "sq"  (1 0 0 0)  1 f 2 (3 1)
    */
    INIT
    OPEN
    while(1){
        if(GOT(")"))break;

        const char *name;
        vec_t v;
        int rot;
        bool flip;
        int color;
        int frame_start = 0;
        int frame_len = -1;

        OPEN
        {
            GET_STR_CACHED(name, &prend->name_store)
            GET_VEC(prend->space, v)
            GET_INT(rot)
            GET_BOOL(flip)
            GET_INT_FANCY(color)
            if(GOT("(")){
                NEXT
                GET_INT(frame_start)
                GET_INT(frame_len)
                CLOSE
            }
        }
        CLOSE

        prismel_t *found = prismelrenderer_get_prismel(prend, name);
        if(found == NULL){
            fprintf(stderr, "Couldn't find prismel: %s\n", name);
            return 2;
        }

        rendergraph_child_t *child;
        err = rendergraph_push_child(rgraph,
            RENDERGRAPH_CHILD_TYPE_PRISMEL,
            &child);
        if(err)return err;
        child->u.prismel.prismel = found;
        child->trf.rot = rot;
        child->trf.flip = flip;
        child->u.prismel.color = color;
        child->frame_start = frame_start;
        child->frame_len = frame_len;
        vec_cpy(prend->space->dims, child->trf.add, v);
    }
    NEXT
    return 0;
}

static int parse_shape_labels(prismelrenderer_t *prend, fus_lexer_t *lexer,
    rendergraph_t *rgraph, prismelrenderer_localdata_t *localdata
){
    /*
        Example data:

        :
            : "label1" ( 0 0 0 0)  0 f
            : "label2" (-1 0 2 0) 11 t (3 1)
    */
    INIT
    OPEN
    while(1){
        if(GOT(")"))break;

        const char *name;
        vec_t v;
        int rot;
        bool flip;
        int frame_start = 0;
        int frame_len = -1;

        OPEN
        {
            GET_STR_CACHED(name, &prend->name_store)
            GET_VEC(prend->space, v)
            GET_INT(rot)
            GET_BOOL(flip)
            if(GOT("(")){
                NEXT
                GET_INT(frame_start)
                GET_INT(frame_len)
                CLOSE
            }
        }
        CLOSE

        prismelrenderer_localdata_label_t *label_localdata =
            prismelrenderer_localdata_get_label(localdata, name, true);
        if(!label_localdata){
            fus_lexer_err_info(lexer);
            fprintf(stderr, "Label not defined: \"%s\"\n", name);
            return 2;
        }

        rendergraph_child_t *child;
        err = rendergraph_push_child(rgraph,
            RENDERGRAPH_CHILD_TYPE_LABEL,
            &child);
        if(err)return err;
        child->u.label.name = name;
        child->u.label.default_rgraph_name = label_localdata->default_rgraph_name;
        child->u.label.default_frame_i = label_localdata->default_frame_i;
        child->trf.rot = rot;
        child->trf.flip = flip;
        child->frame_start = frame_start;
        child->frame_len = frame_len;
        vec_cpy(prend->space->dims, child->trf.add, v);
    }
    NEXT
    return 0;
}

static int parse_shape_hexpicture(prismelrenderer_t *prend, fus_lexer_t *lexer,
    rendergraph_t *rgraph
){
    INIT
    OPEN

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
        rendergraph_child_t *child;
        err = rendergraph_push_child(rgraph,
            RENDERGRAPH_CHILD_TYPE_PRISMEL,
            &child);
        if(err)return err;
        child->u.prismel.prismel = prismels[face->type];
        child->trf.rot = face->rot;
        child->trf.flip = false;
        child->u.prismel.color = face->color;
        child->frame_start = 0;
        child->frame_len = -1;
        vec4_set(child->trf.add,
            face->a, face->b, face->c, face->d);
    }

    free(faces);
    ARRAY_FREE_PTR(char *, lines, (void))
    return 0;
}


int fus_lexer_get_rendergraph(fus_lexer_t *lexer,
    prismelrenderer_t *prend, const char *name, rendergraph_t **rgraph_ptr,
    prismelrenderer_localdata_t *localdata
){
    INIT
    rendergraph_t *rgraph = NULL;

    OPEN

    if(GOT_STR){
        const char *name;
        GET_STR_CACHED(name, &prend->name_store)
        rgraph = prismelrenderer_get_rendergraph(prend, name);
        if(rgraph == NULL){
            fprintf(stderr, "Couldn't find shape: %s\n", name);
            return 2;}
        goto ok;
    }

    if(GOT("map")){
        NEXT

        prismelmapper_t *mapper = NULL;
        err = fus_lexer_get_mapper(lexer, prend, NULL,
            &mapper);
        if(err)return err;

        rendergraph_t *mapped_rgraph = NULL;
        err = fus_lexer_get_rendergraph(lexer, prend, NULL,
            &mapped_rgraph, localdata);
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
        OPEN

        const char **animation_type_ptr = rendergraph_animation_types;
        while(*animation_type_ptr != NULL){
            if(GOT(*animation_type_ptr))break;
            animation_type_ptr++;
        }
        NEXT
        if(*animation_type_ptr == NULL)return UNEXPECTED("<animation_type>");
        animation_type = *animation_type_ptr;
        if(GOT_STR){
            const char *name;
            GET_STR_CACHED(name, &prend->name_store)
            rendergraph_t *rgraph = prismelrenderer_get_rgraph(prend, name);
            if(!rgraph)return UNEXPECTED("shape name");
            n_frames = rgraph->n_frames;
        }else{
            GET_INT(n_frames)
        }
        CLOSE
    }

    rgraph = *rgraph_ptr;
    if(rgraph){
        rendergraph_cleanup(rgraph);
        err = rendergraph_init(rgraph, name, prend, NULL,
            animation_type, n_frames);
        if(err)return err;
    }else{
        rgraph = calloc(1, sizeof(rendergraph_t));
        if(rgraph == NULL)return 1;
        if(!name){
            char *_name = generate_indexed_name("shape", prend->rendergraphs_len);
            if(!_name)return 1;
            name = stringstore_get_donate(&prend->name_store, _name);
            if(!name)return 1;
        }
        err = rendergraph_init(rgraph, name, prend, NULL,
            animation_type, n_frames);
        if(err)return err;
        ARRAY_PUSH(rendergraph_t*, prend->rendergraphs, rgraph)
    }

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
        }else if(GOT("labels")){
            NEXT
            err = parse_shape_labels(prend, lexer, rgraph, localdata);
            if(err)return err;
        }else if(GOT("hexpicture")){
            NEXT
            err = parse_shape_hexpicture(prend, lexer, rgraph);
            if(err == 2)fprintf(stderr, "While parsing shape: %s\n", name);
            if(err)return err;
        }else if(GOT("text")){
            NEXT
            OPEN

            const char *geomfont_name;
            GET_STR_CACHED(geomfont_name, &prend->name_store)

            geomfont_t *geomfont = prismelrenderer_get_geomfont(prend, geomfont_name);
            if(geomfont == NULL){
                fprintf(stderr, "%s: Couldn't find geomfont: %s\n",
                    lexer->filename, geomfont_name);
                return 2;
            }

            char *text;
            GET_STR(text)

            int cx = 0, cy = 0;
            if(GOT("(")){
                NEXT
                GET_INT(cx)
                GET_INT(cy)
                CLOSE
            }

            err = geomfont_rgraph_printf(geomfont, rgraph, cx, cy,
                NULL, "%s", text);
            if(err)return err;
            free(text);

            CLOSE
            goto ok;
        }else{
            err = UNEXPECTED("shapes or prismels");
            return err;
        }
    }

ok:
    CLOSE

    *rgraph_ptr = rgraph;
    return 0;
}

static int parse_shapes(prismelrenderer_t *prend, fus_lexer_t *lexer,
    prismelrenderer_localdata_t *localdata
){
    INIT
    while(1){
        const char *name;
        if(GOT(")"))break;
        GET_STR_CACHED(name, &prend->name_store)

        rendergraph_t *rgraph = prismelrenderer_get_rendergraph(prend, name);
        if(!prend->loaded && rgraph != NULL){
            fprintf(stderr, "Shape %s already defined\n", name);
            return 2;
        }

        err = fus_lexer_get_rendergraph(lexer, prend, name, &rgraph, localdata);
        if(err)return err;
    }
    NEXT
    return 0;
}



/**********
 * MAPPER *
 **********/

int fus_lexer_get_mapper(fus_lexer_t *lexer,
    prismelrenderer_t *prend, const char *name, prismelmapper_t **mapper_ptr
){
    INIT
    prismelmapper_t *mapper = NULL;

    OPEN

    if(GOT_STR){
        const char *name;
        GET_STR_CACHED(name, &prend->name_store)
        mapper = prismelrenderer_get_mapper(prend, name);
        if(mapper == NULL){
            fprintf(stderr, "Couldn't find mapper: %s\n", name);
            return 2;}
        goto ok;
    }

    bool solid = false;
    if(GOT("solid")){
        NEXT
        solid = true;
    }

    if(GOT("map")){
        NEXT
        prismelmapper_t *mapper1 = NULL;
        err = fus_lexer_get_mapper(lexer, prend, NULL, &mapper1);
        if(err)return err;

        prismelmapper_t *mapper2 = NULL;
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

    mapper = *mapper_ptr;
    if(mapper){
        /* TODO: don't cleanup & re-initialize, because this frees our arrays
        mapper->applications and mapper->mapplications, losing our
        references to existing rgraphs (and probably leaving them dangling).
        Instead, do an explicit re-init which keeps the old rgraphs around,
        but also updates them in-place. */
        prismelmapper_cleanup(mapper);
        err = prismelmapper_init(mapper, name, prend->space, solid);
        if(err)return err;
    }else{
        mapper = calloc(1, sizeof(*mapper));
        if(mapper == NULL)return 1;
        if(!name){
            char *_name = generate_indexed_name("mapper", prend->mappers_len);
            if(!_name)return 1;
            name = stringstore_get_donate(&prend->name_store, _name);
            if(!name)return 1;
        }
        err = prismelmapper_init(mapper, name, prend->space, solid);
        if(err)return err;
        ARRAY_PUSH(prismelmapper_t*, prend->mappers, mapper)
    }

    GET_VEC(prend->space, mapper->unit)

    if(GOT("entries")){
        NEXT
        OPEN
        while(1){
            if(GOT(")"))break;
            OPEN
            const char *prismel_name;
            GET_STR_CACHED(prismel_name, &prend->name_store)
            prismel_t *prismel = prismelrenderer_get_prismel(
                prend, prismel_name);
            if(prismel == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find prismel: %s\n", prismel_name);
                err = 2; return err;}

            GET("->")

            const char *rgraph_name;
            GET_STR_CACHED(rgraph_name, &prend->name_store)
            rendergraph_t *rgraph = prismelrenderer_get_rendergraph(
                prend, rgraph_name);
            if(rgraph == NULL){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Couldn't find shape: %s\n", rgraph_name);
                err = 2; return err;}

            err = prismelmapper_push_entry(mapper, prismel, rgraph);
            if(err)return err;

            CLOSE
        }
        NEXT
    }

ok:
    CLOSE

    *mapper_ptr = mapper;
    return 0;
}

static int parse_mappers(prismelrenderer_t *prend, fus_lexer_t *lexer){
    INIT
    while(1){
        if(GOT(")"))break;
        const char *name;
        GET_STR_CACHED(name, &prend->name_store)

        prismelmapper_t *mapper = prismelrenderer_get_mapper(prend, name);
        if(!prend->loaded && mapper != NULL){
            fprintf(stderr, "Mapper %s already defined\n", name);
            return 2;
        }

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

        const char *name;
        GET_STR_CACHED(name, &prend->name_store)

        geomfont_t *geomfont = prismelrenderer_get_geomfont(prend, name);
        if(!prend->loaded && geomfont != NULL){
            fprintf(stderr, "Geomfont %s already defined\n", name);
            return 2;
        }

        OPEN

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
        OPEN
        {
            const char *font_filename;
            GET_STR_CACHED(font_filename, &prend->filename_store)
            err = prismelrenderer_get_or_create_font(
                prend, font_filename, &font);
            if(err)return err;
        }
        CLOSE

        GET("prismel")
        OPEN
        {
            const char *prismel_name;
            GET_STR_CACHED(prismel_name, &prend->name_store)

            vec_t vx, vy;
            GET_VEC(prend->space, vx)
            GET_VEC(prend->space, vy)

            if(geomfont != NULL){
                geomfont_cleanup(geomfont);
            }else{
                ARRAY_PUSH_NEW(geomfont_t*, prend->geomfonts, _geomfont)
                geomfont = _geomfont;
            }
            geomfont_init(geomfont, name, font, prend);
            err = geomfont_init_chars_from_sq_prismel(geomfont,
                prismel_name, vx, vy);
            if(err)return err;
        }
        CLOSE

        CLOSE
    }
    NEXT
    return 0;
}




/*******************
 * PRISMELRENDERER *
 *******************/

int prismelrenderer_parse(prismelrenderer_t *prend, fus_lexer_t *lexer,
    prismelrenderer_localdata_t *parent_localdata
){

    /* This function parses a single file (errrr, lexer) worth of data.
    If it encounters an "import" it may call itself recursively (via
    prismelrenderer_load).
    However, some data is not shared between these recursive calls, i.e.
    between files: that data is the "localdata". */
    ARRAY_PUSH_NEW(prismelrenderer_localdata_t*, prend->localdatas, localdata)
    prismelrenderer_localdata_init(localdata, parent_localdata);

    INIT
    while(1){
        if(DONE){
            break;
        }else if(GOT("label")){
            NEXT
            const char *name;
            GET_STR_CACHED(name, &prend->name_store)
            if(prismelrenderer_localdata_get_label(localdata, name, false)){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "Redefinition of label \"%s\"\n", name);
                return 2;
            }
            ARRAY_PUSH_NEW(prismelrenderer_localdata_label_t*, localdata->labels, label)
            label->name = name;
            OPEN
            GET("default")
            if(GOT("null")){
                NEXT
                label->default_rgraph_name = NULL;
            }else{
                GET_STR_CACHED(label->default_rgraph_name, &prend->name_store)
                if(!GOT(")")){
                    GET_INT(label->default_frame_i)
                }
            }
            CLOSE
        }else if(GOT("palmappers")){
            NEXT
            OPEN
            err = parse_palmappers(prend, lexer);
            if(err)return err;
        }else if(GOT("prismels")){
            NEXT
            OPEN
            err = parse_prismels(prend, lexer);
            if(err)return err;
        }else if(GOT("shapes")){
            NEXT
            OPEN
            err = parse_shapes(prend, lexer, localdata);
            if(err)return err;
        }else if(GOT("mappers")){
            NEXT
            OPEN
            err = parse_mappers(prend, lexer);
            if(err)return err;
        }else if(GOT("geomfonts")){
            NEXT
            OPEN
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

            err = prismelrenderer_load(prend, filename, lexer->vars, localdata);
            if(err){
                fus_lexer_err_info(lexer);
                fprintf(stderr, "...while importing here.\n");
                return err;
            }

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

