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
#include "test_app_list.h"


#define MAX_ZOOM 4

#define CONSOLE_START_TEXT "> "

#define USE_GEOMFONT
#ifdef USE_GEOMFONT
    #define FONT_BLITTER_T geomfont_blitter_t
    #define FONT_BLITTER_INIT geomfont_blitter_render_init
    #define FONT_BLITTER_PUTC_CALLBACK geomfont_blitter_putc_callback
    #define FONT_PRINTF geomfont_render_printf
    #define CONSOLE_CHAR_H_MULTIPLIER 2
        /* Because we're using "sq" prismel, which is 2 pixels high */
    #define FONT_ARGS(SURFACE, X0, Y0) app->geomfont, app->renderer, (SURFACE), \
        app->sdl_palette, (X0), (Y0) * CONSOLE_CHAR_H_MULTIPLIER, 1, NULL, NULL
    #define CONSOLE_W 60
    #define CONSOLE_H 35
#else
    #define FONT_BLITTER_T sdlfont_blitter_t
    #define FONT_BLITTER_INIT sdlfont_blitter_init
    #define FONT_BLITTER_PUTC_CALLBACK sdlfont_blitter_putc_callback
    #define FONT_PRINTF sdlfont_printf
    #define FONT_ARGS(SURFACE, X0, Y0) &app->sdlfont, (SURFACE), (X0), (Y0)
    #define CONSOLE_W 80
    #define CONSOLE_H 40
#endif



enum {
    TEST_APP_MODE_GAME,
    TEST_APP_MODE_EDITOR,
    TEST_APP_MODES
};


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
    prismelrenderer_t minimap_prend;
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
    bool show_editor_controls;
    bool show_console; /* console is visible */
    bool process_console; /* console is grabbing input */
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

    test_app_list_t *list;
} test_app_t;




void test_app_cleanup(test_app_t *app);
void test_app_init_input(test_app_t *app);
int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename,
    const char *submap_filename, bool minimap_alt, bool use_textures,
    bool cache_bitmaps, int n_players);
int test_app_mainloop(test_app_t *app);
int test_app_mainloop_step(test_app_t *app);


/* test_app_commands.c */
int test_app_process_console_input(test_app_t *app);
void test_app_write_console_commands(test_app_t *app, const char *prefix);


/* test_app_console.c */
int test_app_process_event_console(test_app_t *app, SDL_Event *event);
int test_app_blit_console(test_app_t *app, SDL_Surface *surface, int x, int y);
void test_app_start_console(test_app_t *app);
void test_app_stop_console(test_app_t *app);
void test_app_show_console(test_app_t *app);
void test_app_hide_console(test_app_t *app);


/* test_app_game.c */
int test_app_exit_callback(hexgame_t *game, player_t *player);
int test_app_set_players_callback(hexgame_t *game, player_t *player,
    int n_players);
int test_app_continue_callback(hexgame_t *game, player_t *player);
int test_app_new_game_callback(hexgame_t *game, player_t *player,
    const char *map_filename);
int test_app_set_players(test_app_t *app, int n_players);
int test_app_process_event_game(test_app_t *app, SDL_Event *event);
int test_app_render_game(test_app_t *app);


/* test_app_editor.c */
int test_app_render_editor(test_app_t *app);
int test_app_process_event_editor(test_app_t *app, SDL_Event *event);


/* test_app_list.c */
int test_app_step_list(test_app_t *app);
int test_app_render_list(test_app_t *app);
int test_app_process_event_list(test_app_t *app, SDL_Event *event);
int test_app_open_list(test_app_t *app, const char *title,
    int index_x, int index_y,
    void *data,
    test_app_list_callback_t *step,
    test_app_list_callback_t *render,
    test_app_list_callback_t *select_item,
    test_app_list_callback_t *cleanup);
int test_app_close_list(test_app_t *app);


#endif