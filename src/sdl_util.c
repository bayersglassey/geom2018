
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "util.h"
#include "sdl_util.h"

void interpolate_color(SDL_Color *c, Uint8 r, Uint8 g, Uint8 b,
    int t, int t_max
){
    c->r = linear_interpolation(c->r, r, t, t_max);
    c->g = linear_interpolation(c->g, g, t, t_max);
    c->b = linear_interpolation(c->b, b, t, t_max);
}

void palette_printf(SDL_Palette *pal){
    for(int i = 0; i < pal->ncolors; i++){
        SDL_Color *c = &pal->colors[i];
        printf("%i: (%i, %i, %i, %i)\n", i, c->r, c->g, c->b, c->a);
    }
}

SDL_Surface *surface8_create(int w, int h,
    bool use_rle, bool use_colorkey, SDL_Palette *pal
){
    SDL_Surface *surface = SDL_CreateRGBSurface(
        0, w, h, 8, 0, 0, 0, 0);
    RET_NULL_IF_SDL_NULL(surface);

    if(use_rle){
        RET_NULL_IF_SDL_NZ(SDL_SetSurfaceRLE(surface, 1));}

    if(use_colorkey){
        RET_NULL_IF_SDL_NZ(SDL_SetColorKey(surface, SDL_TRUE, 0));}

    RET_NULL_IF_SDL_NZ(SDL_SetSurfacePalette(surface, pal));
    return surface;
}

SDL_Surface *surface32_create(int w, int h,
    bool use_rle, bool use_colorkey
){
    SDL_Surface *surface = SDL_CreateRGBSurface(
        0, w, h, 32, 0, 0, 0, 0);
    RET_NULL_IF_SDL_NULL(surface);
    if(use_rle){
        RET_NULL_IF_SDL_NZ(SDL_SetSurfaceRLE(surface, 1));}
    if(use_colorkey){
        RET_NULL_IF_SDL_NZ(SDL_SetColorKey(surface, SDL_TRUE, 0));}
    return surface;
}

Uint8 *surface8_get_pixel_ptr(SDL_Surface *surface, int x, int y){
    if(x < 0 || y < 0 || x >= surface->w || y >= surface->h){
        fprintf(stderr,
            "%s: out of bounds: x=%i, y=%i, surface->w=%i, surface->h=%i\n",
            __func__, x, y, surface->w, surface->h);
        return (Uint8 *)surface->pixels;
    }
    return (Uint8 *)surface->pixels + y*surface->pitch + x;
}

Uint32 *surface32_get_pixel_ptr(SDL_Surface *surface, int x, int y){
    if(x < 0 || y < 0 || x >= surface->w || y >= surface->h){
        fprintf(stderr,
            "%s: out of bounds: x=%i, y=%i, surface->w=%i, surface->h=%i\n",
            __func__, x, y, surface->w, surface->h);
        return (Uint32 *)surface->pixels;
    }
    return (Uint32 *)(
        (Uint8 *)surface->pixels + y*surface->pitch + x*(32/8)
    );
}
