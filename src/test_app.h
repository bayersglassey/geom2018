#ifndef _TEST_APP_H_
#define _TEST_APP_H_


#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "vec4.h"
#include "font.h"
#include "sdlfont.h"
#include "geomfont.h"
#include "console.h"
#include "util.h"
#include "anim.h"
#include "hexmap.h"
#include "hexgame.h"



enum {
    TEST_APP_MODE_GAME,
    TEST_APP_MODE_EDITOR,
    TEST_APP_MODES
};


struct test_app_list;
typedef int test_app_list_callback_t(struct test_app_list *list);

typedef struct test_app_list {
    /* Structure allowing app's console to display a list of objects.
    Includes callbacks for list navigation, list item rendering, etc. */

    void *data;
        /* If data != NULL, app is in "list mode" (so, up/down keys scroll through
        list etc) instead of the default "console mode" (where you type commands etc). */

    int index_x;
    int index_y;
        /* For now, the callbacks *must* implement wraparound themselves:
        there is no way to query list length, so indexes are unbound */

    /* Callbacks */
    test_app_list_callback_t *render;
    test_app_list_callback_t *cleanup;
    test_app_list_callback_t *select_item;
        /* E.g. user hits "enter" on a particular item */
    test_app_list_callback_t *back;
        /* E.g. user hits the "back" button */

} test_app_list_t;

void test_app_list_clear(test_app_list_t *list);
void test_app_list_cleanup(test_app_list_t *list);



typedef struct test_app {
    int scw, sch;
    int delay_goal;
    Uint32 took;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surface;
    SDL_Surface *render_surface;

    const char *prend_filename;
    const char *stateset_filename;
    const char *hexmap_filename;
    const char *submap_filename;

    palette_t palette;
    SDL_Palette *sdl_palette;
    prismelrenderer_t prend;
    font_t font;
    sdlfont_t sdlfont;
    geomfont_t *geomfont;
    console_t console;
    int cur_rgraph_i;

    stateset_t stateset;
    hexgame_t hexgame;
    camera_t *camera;
    prismelmapper_t *camera_mapper;

    bool hexgame_running;
    bool show_controls;
    int mode;

    int x0;
    int y0;
    int rot;
    int flip;
    int zoom;
    int frame_i;
    bool loop;
    bool keydown_shift;
    bool keydown_ctrl;
    int keydown_u;
    int keydown_d;
    int keydown_l;
    int keydown_r;

    test_app_list_t list;
} test_app_t;




void test_app_cleanup(test_app_t *app);
int test_app_load_rendergraphs(test_app_t *app);
int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename,
    const char *submap_filename, bool use_textures,
    bool cache_bitmaps, int n_players);
int test_app_set_players(test_app_t *app, int n_players);
int test_app_process_console_input(test_app_t *app);
void test_app_write_console_commands(test_app_t *app, const char *prefix);
int test_app_mainloop(test_app_t *app);
int test_app_mainloop_step(test_app_t *app);
int test_app_open_list(test_app_t *app, void *data, int index_x, int index_y,
    test_app_list_callback_t *render, test_app_list_callback_t *cleanup);
int test_app_close_list(test_app_t *app);


#endif