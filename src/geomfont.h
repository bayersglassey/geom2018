#ifndef _GEOMFONT_H_
#define _GEOMFONT_H_

#include <stdarg.h>

#include <SDL2/SDL.h>

#include "font.h"
#include "prismelrenderer.h"


/**********
* GEOMFONT *
**********/

typedef struct geomfont {
    font_t *font;

    prismelrenderer_t *prend;
    rendergraph_t *char_rgraphs[FONT_N_CHARS];
    vec_t vx;
    vec_t vy;

    bool autoupper;
} geomfont_t;

void geomfont_cleanup(geomfont_t *geomfont);
int geomfont_init(geomfont_t *geomfont, font_t *font,
    prismelrenderer_t *prend, const char *prismel_name,
    vec_t vx, vec_t vy);
int geomfont_printf(geomfont_t *geomfont,
    SDL_Renderer *renderer, SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int zoom, trf_t *trf, prismelmapper_t *mapper,
    const char *msg, ...);


/******************
* GEOMFONT_BLITTER *
******************/

typedef struct geomfont_blitter {
    geomfont_t *geomfont;

    SDL_Renderer *renderer;
    SDL_Surface *surface;
    SDL_Palette *pal;
    int x0;
    int y0;
    int zoom;
    trf_t trf;
    prismelmapper_t *mapper;

    int col;
    int row;
} geomfont_blitter_t;

void geomfont_blitter_init(geomfont_blitter_t *blitter, geomfont_t *geomfont,
    SDL_Renderer *renderer, SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int zoom, trf_t *trf, prismelmapper_t *mapper);
int geomfont_blitter_putc(geomfont_blitter_t *blitter, char c);
int geomfont_blitter_putc_callback(void *data, char c);

#endif