#ifndef _GEOMFONT_H_
#define _GEOMFONT_H_

#include <stdarg.h>

#include <SDL2/SDL.h>

#include "font.h"
#include "geom.h"

struct prismelmapper;
struct prismelrenderer;


/**********
* GEOMFONT *
**********/

typedef struct geomfont {
    const char *name;

    struct rendergraph *char_rgraphs[FONT_N_CHARS];
    vec_t vx;
    vec_t vy;

    /* Weakrefs */
    font_t *font;
    struct prismelrenderer *prend;
} geomfont_t;

void geomfont_cleanup(geomfont_t *geomfont);
void geomfont_init(geomfont_t *geomfont, const char *name, font_t *font,
    struct prismelrenderer *prend);
int geomfont_init_chars_from_sq_prismel(geomfont_t *geomfont,
    const char *prismel_name,
    vec_t vx, vec_t vy);
int geomfont_render_printf(geomfont_t *geomfont,
    SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int col, int row, int zoom,
    trf_t *trf, struct prismelmapper *mapper,
    const char *msg, ...);
int geomfont_rgraph_printf(geomfont_t *geomfont,
    struct rendergraph *rgraph, int cx, int cy, trf_t *trf,
    const char *msg, ...);


/******************
* GEOMFONT_BLITTER *
******************/

enum geomfont_blitter_type {
    GEOMFONT_BLITTER_TYPE_RENDER,
    GEOMFONT_BLITTER_TYPE_RGRAPH
};

typedef struct geomfont_blitter {
    geomfont_t *geomfont;

    int type; /* enum geomfont_blitter_type */
    union {
        struct {
            SDL_Surface *surface;
            SDL_Palette *pal;
            int x0;
            int y0;
            int zoom;
            struct prismelmapper *mapper;
        } render;
        struct {
            struct rendergraph *rgraph;
            int cx;
            int cy;
        } rgraph;
    } u;

    trf_t trf;

    int col;
    int row;
} geomfont_blitter_t;

void geomfont_blitter_render_init(
    geomfont_blitter_t *blitter, geomfont_t *geomfont,
    SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int col, int row, int zoom,
    trf_t *trf, struct prismelmapper *mapper);
void geomfont_blitter_rgraph_init(geomfont_blitter_t *blitter,
    geomfont_t *geomfont, struct rendergraph *rgraph,
    int cx, int cy, trf_t *trf);
int geomfont_blitter_putc(geomfont_blitter_t *blitter, char c);
int geomfont_blitter_putc_callback(void *data, char c);

#endif