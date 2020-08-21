
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "util.h"

int int_min(int x, int y){
    return x < y? x: y;
}

int int_max(int x, int y){
    return x > y? x: y;
}

int linear_interpolation(int x0, int x1, int t, int t_max){
    int diff = x1 - x0;
    return x0 + diff * t / t_max;
}

void interpolate_color(SDL_Color *c, Uint8 r, Uint8 g, Uint8 b,
    int t, int t_max
){
    c->r = linear_interpolation(c->r, r, t, t_max);
    c->g = linear_interpolation(c->g, g, t, t_max);
    c->b = linear_interpolation(c->b, b, t, t_max);
}

void get_spaces(char *spaces, int max_spaces, int n_spaces){
    if(n_spaces > max_spaces){
        fprintf(stderr, "Can't handle %i spaces - max %i\n",
            n_spaces, max_spaces);
        n_spaces = max_spaces;
    }
    for(int i = 0; i < n_spaces; i++)spaces[i] = ' ';
    spaces[n_spaces] = '\0';
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
        RET_NULL_IF_SDL_NZ(use_rle
            && SDL_SetSurfaceRLE(surface, 1));}

    if(use_colorkey){
        RET_NULL_IF_SDL_NZ(SDL_SetColorKey(surface, SDL_TRUE, 0));}
    //if(use_colorkey){
    //    RET_NULL_IF_SDL_NZ(SDL_SetColorKey(surface, SDL_TRUE,
    //        use_rle? SDL_RLEACCEL: 0));}

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
            "%s: out of bounds: x=%i, y=%i, surface->w=%i, surface->h=%i",
            __func__, x, y, surface->w, surface->h);
        return (Uint8 *)surface->pixels;
    }
    return (Uint8 *)surface->pixels + y*surface->pitch + x;
}

Uint32 *surface32_get_pixel_ptr(SDL_Surface *surface, int x, int y){
    if(x < 0 || y < 0 || x >= surface->w || y >= surface->h){
        fprintf(stderr,
            "%s: out of bounds: x=%i, y=%i, surface->w=%i, surface->h=%i",
            __func__, x, y, surface->w, surface->h);
        return (Uint32 *)surface->pixels;
    }
    return (Uint32 *)(
        (Uint8 *)surface->pixels + y*surface->pitch + x*(32/8)
    );
}
