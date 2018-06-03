
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "util.h"

char *load_file(const char *filename){
    FILE *f = fopen(filename, "r");
    long f_size;
    char *f_buffer;
    size_t n_read_bytes;
    if(f == NULL){
        fprintf(stderr, "Could not open file: %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    f_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    f_buffer = calloc(f_size + 1, 1);
    if(f_buffer == NULL){
        fprintf(stderr, "Could not allocate buffer for file: %s (%li bytes)\n", filename, f_size);
        fclose(f);
        return NULL;
    }
    n_read_bytes = fread(f_buffer, 1, f_size, f);
    fclose(f);
    return f_buffer;
}

size_t strnlen(const char *s, size_t maxlen){
    size_t len = 0;
    while(len < maxlen && s[len] != '\0')len++;
    return len;
}

char *strdup(const char *s1){
    char *s2 = malloc(strlen(s1) + 1);
    if(s2 == NULL)return NULL;
    strcpy(s2, s1);
    return s2;
}

char *strndup(const char *s1, size_t len){
    size_t s_len = strnlen(s1, len);
    char *s2 = malloc(s_len + 1);
    if(s2 == NULL)return NULL;
    strncpy(s2, s1, len);
    s2[s_len] = '\0';
    return s2;
}

SDL_Surface *surface_create(int w, int h, int bpp,
    bool use_rle, bool use_colorkey
){
    SDL_Surface *surface = SDL_CreateRGBSurface(
        0, w, h, bpp, 0, 0, 0, 0);
    RET_NULL_IF_SDL_NULL(surface);
    RET_NULL_IF_SDL_NZ(use_rle
        && SDL_SetSurfaceRLE(surface, 1));
    RET_NULL_IF_SDL_NZ(use_colorkey
        && SDL_SetColorKey(surface, SDL_TRUE, 0));
    return surface;
}

Uint32 *surface_get_pixel_ptr(SDL_Surface *surface, int x, int y){
    return (Uint32 *)(
        (Uint8 *)surface->pixels + y*surface->pitch + x*(32/8)
    );
}
