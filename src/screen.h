#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stdbool.h>
#include <SDL2/SDL.h>


typedef struct screen {
    int w, h;
    bool gui;

    /* If screen->gui, then window is non-NULL, otherwise, it's NULL. */
    SDL_Window *window;

    SDL_Palette *palette;

    /* The surface is non-NULL only between calls to screen_{begin,finish}_render */
    SDL_Surface *surface;
} screen_t;


void screen_cleanup(screen_t *screen);
int screen_init_gui(screen_t *screen, int w, int h, const char *title,
    Uint32 window_flags);
int screen_init_no_gui(screen_t *screen, int w, int h);
int screen_begin_render(screen_t *screen);
void screen_finish_render(screen_t *screen);
Uint32 screen_get_palette_color(screen_t *screen, int i);

#endif
