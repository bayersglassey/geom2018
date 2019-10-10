#ifndef _SDLFONT_H_
#define _SDLFONT_H_

#include <stdarg.h>

#include <SDL2/SDL.h>

#include "font.h"

/* SDLFONT_N_COLOR_VALUES: the color values are Uint8 SDL_Surface pixel
values, the first of which is always the transparent color 0 */
#define SDLFONT_N_COLOR_VALUES 4


/**********
* SDLFONT *
**********/

typedef struct sdlfont {
    font_t *font;
    SDL_Surface *surface;
    bool autoupper;
} sdlfont_t;

void sdlfont_cleanup(sdlfont_t *sdlfont);
int sdlfont_init(sdlfont_t *sdlfont, font_t *font, SDL_Palette *pal);
void sdlfont_putc(sdlfont_t *sdlfont, SDL_Surface *render_surface,
    int x, int y, char c);
int sdlfont_putc_callback(void *data, char c);
int sdlfont_printf(sdlfont_t *sdlfont, SDL_Surface *render_surface,
    int x0, int y0, const char *msg, ...);


/******************
* SDLFONT_BLITTER *
******************/

typedef struct sdlfont_blitter {
    sdlfont_t *sdlfont;
    SDL_Surface *render_surface;
    int x0;
    int y0;
    int col;
    int row;
} sdlfont_blitter_t;

void sdlfont_blitter_init(sdlfont_blitter_t *blitter, sdlfont_t *sdlfont,
    SDL_Surface *render_surface, int x0, int y0);
void sdlfont_blitter_putc(sdlfont_blitter_t *blitter, char c);

#endif