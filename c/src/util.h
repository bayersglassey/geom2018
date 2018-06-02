
#ifndef _FUS_UTIL_H_
#define _FUS_UTIL_H_

#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

char *load_file(const char *filename);
size_t strnlen(const char *s, size_t maxlen);
char *strdup(const char *s1);
char *strndup(const char *s1, size_t len);
SDL_Surface *surface_create(int w, int h, int bpp,
    bool use_rle, bool use_colorkey);
Uint32 *surface_get_pixel_ptr(SDL_Surface *surface, int x, int y);

#endif
