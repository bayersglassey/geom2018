
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>


#include "util.h"
#include "font.h"
#include "geom.h"
#include "prismelrenderer.h"
#include "geomfont.h"


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
    free(geomfont->name);
}

int geomfont_init(geomfont_t *geomfont, char *name, font_t *font,
    prismelrenderer_t *prend, const char *prismel_name,
    vec_t vx, vec_t vy
){
    int err = 0;
    vecspace_t *space = prend->space;

    geomfont->name = name;
    geomfont->font = font;
    geomfont->prend = prend;
    geomfont->autoupper = false;
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

    Uint8 colors[FONT_N_COLOR_VALUES] = {0, 1+8, 1+7, 1+15};

    for(int i = 0; i < FONT_N_CHARS; i++){
        unsigned char *char_data = font->char_data[i];
        if(!char_data){
            geomfont->char_rgraphs[i] = NULL;
            continue;
        }

        char *rgraph_name = generate_char_name(prismel->name, "_char", i);
        if(!rgraph_name)return 1;

        int n_frames = 1;

        ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, rgraph)
        err = rendergraph_init(rgraph, rgraph_name, prend, NULL,
            rendergraph_animation_type_default,
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
                int color_i = char_data[y * char_h + x];

                prismel_trf_t *prismel_trf;
                err = rendergraph_push_prismel_trf(rgraph, &prismel_trf);
                if(err)return err;
                prismel_trf->prismel = prismel;
                prismel_trf->color = colors[color_i];
                vec_cpy(space->dims, prismel_trf->trf.add, v);

                vec_add(space->dims, v, vx);
            }
        }

        geomfont->char_rgraphs[i] = rgraph;
    }

    return 0;
}

int geomfont_printf(geomfont_t *geomfont,
    SDL_Renderer *renderer, SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int zoom, trf_t *trf, prismelmapper_t *mapper,
    const char *msg, ...
){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);

    geomfont_blitter_t blitter;
    geomfont_blitter_init(&blitter, geomfont, renderer, surface, pal,
        x0, y0, zoom, trf, mapper);
    err = font_vprintf(&geomfont_blitter_putc_callback, &blitter,
        msg, vlist);

    va_end(vlist);
    return err;
}


/******************
* GEOMFONT_BLITTER *
******************/

void geomfont_blitter_init(geomfont_blitter_t *blitter, geomfont_t *geomfont,
    SDL_Renderer *renderer, SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int zoom, trf_t *trf, prismelmapper_t *mapper
){
    blitter->geomfont = geomfont;
    blitter->renderer = renderer;
    blitter->surface = surface;
    blitter->pal = pal;
    blitter->x0 = x0;
    blitter->y0 = y0;
    blitter->zoom = zoom;
    blitter->x0 = x0;
    blitter->y0 = y0;
    blitter->zoom = zoom;
    if(trf){
        blitter->trf = *trf;
    }else{
        trf_zero(&blitter->trf);
    }
    blitter->mapper = mapper;
    blitter->col = 0;
    blitter->row = 0;
}

static void geomfont_blitter_newline(geomfont_blitter_t *blitter){
    blitter->col = 0;
    blitter->row++;
}

static void geomfont_blitter_move_right(geomfont_blitter_t *blitter){
    blitter->col++;
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
    if(geomfont->autoupper)c = toupper(c);
    int char_i = c;
    if(char_i < 0 || char_i >= FONT_N_CHARS){
        fprintf(stderr, "%s: Char outside 0..%i: %i (%c)\n",
            __func__, FONT_N_CHARS - 1, char_i, c);
        return 2;
    }
    rendergraph_t *rgraph = geomfont->char_rgraphs[char_i];
    if(!rgraph)return 0;

    /* Where to render rgraph? At pos! */
    vec_t pos = {0};
    vec_t add;

    /* pos += col * vx */
    vec_cpy(space->dims, add, geomfont->vx);
    vec_nmul(space->dims, add, blitter->col * font->char_w);
    vec_add(space->dims, pos, add);

    /* pos += -row * vy */
    /* "-row" because vy points up whereas row starts from top of
    screen and goes down. */
    vec_cpy(space->dims, add, geomfont->vy);
    vec_nmul(space->dims, add, -blitter->row * font->char_h);
    vec_add(space->dims, pos, add);

    /* Hardcoded first frame for now */
    int frame_i = 0;

    /* Render that rgraph */
    err = rendergraph_render(rgraph,
        blitter->renderer,
        blitter->surface,
        blitter->pal,
        geomfont->prend,
        blitter->x0,
        blitter->y0,
        blitter->zoom,
        pos,
        blitter->trf.rot,
        blitter->trf.flip,
        frame_i,
        blitter->mapper
    );
    if(err){
        fprintf(stderr, "%s: Error rendering rgraph\n", __func__);
    }

    geomfont_blitter_move_right(blitter);
    return 0;
}

int geomfont_blitter_putc_callback(void *data, char c){
    /* Callback for use with font_printf, console_blit, etc */
    return geomfont_blitter_putc(data, c);
}
