#ifndef _MINIEDITOR_H_
#define _MINIEDITOR_H_

#include <stdbool.h>
#include <SDL2/SDL.h>


#include "font.h"
#include "geomfont.h"
#include "prismelrenderer.h"


#define MINIEDITOR_MAX_ZOOM 4


typedef struct minieditor {
    SDL_Surface *surface;
    SDL_Palette *sdl_palette;

    const char *prend_filename;

    font_t *font;
    geomfont_t *geomfont;
    prismelrenderer_t *prend;

    bool show_editor_controls;

    int cur_rgraph_i;
    int frame_i;
    int scw;
    int sch;
    int x0;
    int y0;
    int zoom;
    int rot;
    int flip;

    int keydown_u;
    int keydown_d;
    int keydown_l;
    int keydown_r;
    bool keydown_shift;
    bool keydown_ctrl;
} minieditor_t;



void minieditor_cleanup(minieditor_t *editor);
void minieditor_init(minieditor_t *editor,
    SDL_Surface *surface, SDL_Palette *sdl_palette,
    const char *prend_filename,
    font_t *font, geomfont_t *geomfont,
    prismelrenderer_t *prend,
    int scw, int sch);
int minieditor_render(minieditor_t *editor, int *line_y_ptr);
int minieditor_process_event(minieditor_t *editor, SDL_Event *event);
int minieditor_printf(minieditor_t *editor, int col, int row,
    const char *msg, ...);


#endif