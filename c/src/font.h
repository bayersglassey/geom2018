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

int font_load(font_t *font, const char *filename,
    SDL_Renderer *renderer);
int font_parse(font_t *font, fus_lexer_t *lexer,
    SDL_Renderer *renderer);
void font_blitmsg(font_t *font, SDL_Renderer *renderer, const char *msg, ...);

#endif