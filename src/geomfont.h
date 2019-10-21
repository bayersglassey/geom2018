#ifndef _GEOMFONT_H_
#define _GEOMFONT_H_

#include <stdarg.h>

#include <SDL2/SDL.h>

#include "font.h"
#include "geom.h"

struct prismelmapper;


/**********
* GEOMFONT *
**********/

typedef struct geomfont {
    char *name;
    font_t *font;

    struct prismelrenderer *prend;
    struct rendergraph *char_rgraphs[FONT_N_CHARS];
    vec_t vx;
    vec_t vy;

    bool autoupper;
} geomfont_t;

void geomfont_cleanup(geomfont_t *geomfont);
int geomfont_init(geomfont_t *geomfont, char *name, font_t *font,
    struct prismelrenderer *prend, const char *prismel_name,
    vec_t vx, vec_t vy);
int geomfont_printf(geomfont_t *geomfont,
    SDL_Renderer *renderer, SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int zoom, trf_t *trf, struct prismelmapper *mapper,
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
    struct prismelmapper *mapper;

    int col;
    int row;
} geomfont_blitter_t;

void geomfont_blitter_init(geomfont_blitter_t *blitter, geomfont_t *geomfont,
    SDL_Renderer *renderer, SDL_Surface *surface, SDL_Palette *pal,
    int x0, int y0, int zoom, trf_t *trf, struct prismelmapper *mapper);
int geomfont_blitter_putc(geomfont_blitter_t *blitter, char c);
int geomfont_blitter_putc_callback(void *data, char c);

#endif