
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>


#include "util.h"
#include "font.h"
#include "geom.h"
#include "prismelrenderer.h"
#include "geomfont.h"
#include "generic_printf.h"


static char *generate_char_name(const char *base_name, const char *suffix, int i){
    /* Generate a name, e.g. "sq_char65" */
    int base_name_len = strlen(base_name);
    int suffix_len = strlen(suffix);
    int i_len = strlen_of_int(i);
    int name_len = base_name_len + suffix_len + i_len;
    char *name = malloc(sizeof(*name) * (name_len + 1));
    if(name == NULL)return NULL;
    strcpy(name, base_name);
    strcpy(name + base_name_len, suffix);
    strncpy_of_int(name + base_name_len + suffix_len, i, i_len);
    name[name_len] = '\0';
    return name;
}


/**********
* GEOMFONT *
**********/

void geomfont_cleanup(geomfont_t *geomfont){
    /* Nuthin */
}

void geomfont_init(geomfont_t *geomfont, const char *name, font_t *font,
    prismelrenderer_t *prend
){
    geomfont->name = name;
    geomfont->font = font;
    geomfont->prend = prend;
}

int geomfont_init_chars_from_sq_prismel(geomfont_t *geomfont,
    const char *prismel_name,
    vec_t vx, vec_t vy
){
    int err = 0;
    font_t *font = geomfont->font;
    prismelrenderer_t *prend = geomfont->prend;
    vecspace_t *space = prend->space;

    vec_cpy(space->dims, geomfont->vx, vx);
    vec_cpy(space->dims, geomfont->vy, vy);

    int char_w = font->char_w;
    int char_h = font->char_h;

    prismel_t *prismel = prismelrenderer_get_prismel(prend, prismel_name);
    if(!prismel){
        fprintf(stderr,
            "Can't find prismel \"%s\" for use with geomfont\n",
            prismel_name);
        return 2;
    }


    /***********************
     * CREATE CHAR RGRAPHS *
     ***********************/

    for(int i = 0; i < FONT_N_CHARS; i++){
        unsigned char *char_data = font->char_data[i];
        if(!char_data){
            geomfont->char_rgraphs[i] = NULL;
            continue;
        }

        const char *rgraph_name;
        {
            char *_rgraph_name = generate_char_name(prismel->name,
                "_char", i);
            if(!_rgraph_name)return 1;
            rgraph_name = stringstore_get_donate(&prend->name_store,
                _rgraph_name);
            if(!rgraph_name)return 1;
        }

        int n_frames = 1;

        ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, rgraph)
        err = rendergraph_init(rgraph, rgraph_name, prend, NULL,
            rendergraph_animation_type_cycle,
            n_frames);
        if(err)return err;

        for(int y = 0; y < char_h; y++){
            vec_t v;
            vec_cpy(space->dims, v, vy);

            /* (-y - 1) because vy points up whereas y starts from top of
            bitmap and goes down.
            The "- 1" is because the prismel's origin is at bottom-left, so
            we need to shift it downwards by its height to get its top-left
            corner to be the origin when rendering as a bitmap.
            If you see what I mean. */
            vec_nmul(space->dims, v, -y - 1);

            for(int x = 0; x < char_w; x++){
                int color = char_data[y * char_h + x];

                rendergraph_child_t *child;
                err = rendergraph_push_child(rgraph,
                    RENDERGRAPH_CHILD_TYPE_PRISMEL,
                    &child);
                if(err)return err;
                child->u.prismel.prismel = prismel;
                child->u.prismel.color = color;
                vec_cpy(space->dims, child->trf.add, v);

                vec_add(space->dims, v, vx);
            }
        }

        geomfont->char_rgraphs[i] = rgraph;
    }

    return 0;
}

int geomfont_render_printf(geomfont_t *geomfont,
    SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int col, int row, int zoom,
    trf_t *trf, prismelmapper_t *mapper,
    const char *msg, ...
){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);

    geomfont_blitter_t blitter;
    geomfont_blitter_render_init(&blitter, geomfont,
        surface, pal,
        x0, y0, col, row, zoom, trf, mapper);
    err = generic_vprintf(&geomfont_blitter_putc_callback, &blitter,
        msg, vlist);

    va_end(vlist);
    return err;
}

int geomfont_rgraph_printf(geomfont_t *geomfont,
    rendergraph_t *rgraph, int cx, int cy, trf_t *trf,
    const char *msg, ...
){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);

    geomfont_blitter_t rgraph_blitter;
    geomfont_blitter_rgraph_init(&rgraph_blitter, geomfont,
        rgraph, cx, cy, trf);
    err = generic_vprintf(&geomfont_blitter_putc_callback,
        &rgraph_blitter, msg, vlist);

    va_end(vlist);
    return err;
}

static int geomfont_get_c_rgraph(geomfont_t *geomfont, char c,
    rendergraph_t **rgraph_ptr
){
    /* Sets *rgraph_ptr based on c
    Returns nonzero on error
    May set *rgraph_ptr to NULL on success if c has no bitmap */
    if(geomfont->font->autoupper)c = toupper(c);
    int char_i = (unsigned char)c;
    if(char_i < 0 || char_i >= FONT_N_CHARS){
        fprintf(stderr, "%s: Char outside 0..%i: %i (%c)\n",
            __func__, FONT_N_CHARS - 1, char_i, c);
        return 2;
    }
    *rgraph_ptr = geomfont->char_rgraphs[char_i];
    return 0;
}


/********************
* GEOMFONT_BLITTER *
********************/

static void geomfont_blitter_init_core(int type,
    geomfont_blitter_t *blitter, geomfont_t *geomfont,
    trf_t *trf
){
    blitter->type = type;
    blitter->geomfont = geomfont;
    if(trf){
        blitter->trf = *trf;
    }else{
        trf_zero(&blitter->trf);
    }
    blitter->col = 0;
    blitter->row = 0;
}

void geomfont_blitter_render_init(
    geomfont_blitter_t *blitter, geomfont_t *geomfont,
    SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int col, int row, int zoom,
    trf_t *trf, prismelmapper_t *mapper
){
    geomfont_blitter_init_core(GEOMFONT_BLITTER_TYPE_RENDER,
        blitter, geomfont, trf);
    blitter->u.render.surface = surface;
    blitter->u.render.pal = pal;

    /* TODO: replace these hardcoded "* 2" with fancier stuff involving
    app->geomfont->v{x,y} and vec4_render.
    We're currently assuming "geomfont1" which uses "sq" as its prismel,
    and (1 0 0 0), (0 0 0 1) as vx, vy, which render to (2 0) and (0 2),
    thus the "* 2".
    NOTE: x0, y0 can stay as they are: it's col, row we need to multiply by vx, vy. */
    blitter->u.render.x0 = x0 + col * 2;
    blitter->u.render.y0 = y0 + row * 2;

    blitter->u.render.zoom = zoom;
    blitter->u.render.mapper = mapper;
}

void geomfont_blitter_rgraph_init(geomfont_blitter_t *blitter,
    geomfont_t *geomfont, rendergraph_t *rgraph, int cx, int cy, trf_t *trf
){
    geomfont_blitter_init_core(GEOMFONT_BLITTER_TYPE_RGRAPH,
        blitter, geomfont, trf);
    blitter->u.rgraph.rgraph = rgraph;
    blitter->u.rgraph.cx = cx;
    blitter->u.rgraph.cy = cy;
}

static void geomfont_blitter_newline(geomfont_blitter_t *blitter){
    blitter->col = 0;
    blitter->row++;
}

static void geomfont_blitter_move_right(geomfont_blitter_t *blitter){
    blitter->col++;
}

static void geomfont_blitter_get_pos(geomfont_blitter_t *blitter, vec_t pos){
    /* Sets pos to where next character should be rendered */

    geomfont_t *geomfont = blitter->geomfont;
    font_t *font = geomfont->font;
    vecspace_t *space = geomfont->prend->space;

    vec_zero(pos);
    vec_t add;

    int x = blitter->col;
    int y = blitter->row;

    if(blitter->type == GEOMFONT_BLITTER_TYPE_RGRAPH){
        x -= blitter->u.rgraph.cx;
        y -= blitter->u.rgraph.cy;
    }

    /* pos += x * char_w * vx */
    vec_cpy(space->dims, add, geomfont->vx);
    vec_nmul(space->dims, add, x * font->char_w);
    vec_add(space->dims, pos, add);

    /* pos += -y * char_h * vy */
    /* NOTE: "-y" because vy points up whereas row starts from top
    of screen and goes down. */
    vec_cpy(space->dims, add, geomfont->vy);
    vec_nmul(space->dims, add, -y * font->char_h);
    vec_add(space->dims, pos, add);
}

int geomfont_blitter_putc(geomfont_blitter_t *blitter, char c){
    int err;

    geomfont_t *geomfont = blitter->geomfont;
    font_t *font = geomfont->font;
    vecspace_t *space = geomfont->prend->space;

    /* Newlines are easy */
    if(c == '\n'){
        geomfont_blitter_newline(blitter);
        return 0;
    }

    /* Turn character into rgraph */
    rendergraph_t *c_rgraph;
    err = geomfont_get_c_rgraph(geomfont, c, &c_rgraph);
    if(err)return err;
    if(!c_rgraph){
        geomfont_blitter_move_right(blitter);
        return 0;
    }

    /* Where to render c_rgraph? At pos! */
    vec_t pos;
    geomfont_blitter_get_pos(blitter, pos);

    /* Hardcoded first frame for now */
    int frame_i = 0;

    if(blitter->type == GEOMFONT_BLITTER_TYPE_RENDER){
        vec_add(space->dims, pos, blitter->trf.add);

        /* Render that rgraph */
        err = rendergraph_render(c_rgraph,
            blitter->u.render.surface,
            blitter->u.render.pal,
            geomfont->prend,
            blitter->u.render.x0,
            blitter->u.render.y0,
            blitter->u.render.zoom,
            pos,
            blitter->trf.rot,
            blitter->trf.flip,
            frame_i,
            blitter->u.render.mapper
        );
        if(err){
            fprintf(stderr, "%s: Error rendering c_rgraph\n", __func__);
            return 2;
        }
    }else if(blitter->type == GEOMFONT_BLITTER_TYPE_RGRAPH){
        rendergraph_child_t *child;
        err = rendergraph_push_child(
            blitter->u.rgraph.rgraph,
            RENDERGRAPH_CHILD_TYPE_RGRAPH,
            &child);
        if(err)return err;
        child->u.rgraph.rendergraph = c_rgraph;
        child->u.rgraph.frame_i = frame_i;
        trf_cpy(space, &child->trf, &blitter->trf);
        vec_add(space->dims, child->trf.add, pos);
    }else{
        fprintf(stderr, "%s: Unrecognized geomfont_blitter type: %i\n",
            __func__, blitter->type);
        return 2;
    }

    geomfont_blitter_move_right(blitter);
    return 0;
}

int geomfont_blitter_putc_callback(void *data, char c){
    /* Callback for use with generic_printf, console_blit, etc */
    return geomfont_blitter_putc(data, c);
}

