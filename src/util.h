
#ifndef _FUS_UTIL_H_
#define _FUS_UTIL_H_

#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define ERR_INFO() fprintf(stderr, "%s:%s:%i: ", \
    __FILE__, __func__, __LINE__)
#define RET_IF_SDL_NZ(x) { \
    if((x) != 0){ \
        ERR_INFO(); \
        fprintf(stderr, "SDL error: %s\n", SDL_GetError()); \
        return 2;}}
#define RET_IF_SDL_NULL(x) { \
    if((x) == NULL){ \
        ERR_INFO(); \
        fprintf(stderr, "SDL error: %s\n", SDL_GetError()); \
        return 2;}}
#define RET_NULL_IF_SDL_NZ(x) { \
    if((x) != 0){ \
        ERR_INFO(); \
        fprintf(stderr, "SDL error: %s\n", SDL_GetError()); \
        return NULL;}}
#define RET_NULL_IF_SDL_NULL(x) { \
    if((x) == NULL){ \
        ERR_INFO(); \
        fprintf(stderr, "SDL error: %s\n", SDL_GetError()); \
        return NULL;}}

#define MAX_SPACES 256

int int_min(int x, int y);
int int_max(int x, int y);
int linear_interpolation(int x0, int x1, int t, int t_max);
int getln(char buf[], int buf_len);
char *load_file(const char *filename);
bool streq(const char *s1, const char *s2);
size_t strnlen(const char *s, size_t maxlen);
char *strdup(const char *s1);
char *strndup(const char *s1, size_t len);
void get_spaces(char *spaces, int max_spaces, int n_spaces);
void palette_printf(SDL_Palette *pal);
SDL_Surface *surface8_create(int w, int h,
    bool use_rle, bool use_colorkey, SDL_Palette *pal);
SDL_Surface *surface32_create(int w, int h,
    bool use_rle, bool use_colorkey);
Uint8 *surface8_get_pixel_ptr(SDL_Surface *surface, int x, int y);
Uint32 *surface32_get_pixel_ptr(SDL_Surface *surface, int x, int y);

#endif
