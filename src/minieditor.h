#ifndef _MINIEDITOR_H_
#define _MINIEDITOR_H_

#include <stdbool.h>
#include <SDL2/SDL.h>


#include "font.h"
#include "geomfont.h"
#include "prismelrenderer.h"


#define MINIEDITOR_MAX_ZOOM 4


typedef struct minieditor {
    int delay_goal;

    SDL_Surface *surface;
    SDL_Palette *sdl_palette;

    prismelmapper_t *mapper;
    palettemapper_t *palmapper;

    ARRAY_DECL(label_mapping_t*, label_mappings)

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
    prismelmapper_t *mapper,
    palettemapper_t *palmapper,
    const char *prend_filename,
    font_t *font, geomfont_t *geomfont,
    prismelrenderer_t *prend,
    int delay_goal, int scw, int sch);
rendergraph_t *minieditor_get_rgraph(minieditor_t *editor);
int minieditor_render(minieditor_t *editor, int *line_y_ptr);
int minieditor_process_event(minieditor_t *editor, SDL_Event *event);
int minieditor_vprintf(minieditor_t *editor, int col, int row,
    const char *msg, va_list vlist);
int minieditor_printf(minieditor_t *editor, int col, int row,
    const char *msg, ...);


#endif
