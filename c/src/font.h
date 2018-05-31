#ifndef _FONT_H_
#define _FONT_H_

#include <SDL2/SDL.h>

#include "lexer.h"

typedef struct font {
    int char_w;
    int char_h;
    SDL_Surface *surface;
    SDL_Texture *texture;
} font_t;

typedef struct font_blitter {
    font_t *font;
    SDL_Renderer *renderer;
    int x0;
    int y0;
    int col;
    int row;
} font_blitter_t;


void font_get_char_coords(font_t *font, char c, int *char_x, int *char_y);

int font_load(font_t *font, const char *filename,
    SDL_Renderer *renderer);
int font_parse(font_t *font, fus_lexer_t *lexer,
    SDL_Renderer *renderer);
void font_blitchar(font_t *font, SDL_Renderer *renderer,
    int x, int y, char c);
void font_blitter_blitchar(font_blitter_t *blitter, char c);
void font_blitmsg(font_t *font, SDL_Renderer *renderer,
    int x0, int y0, const char *msg, ...);

#endif