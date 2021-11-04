#ifndef _TEST_APP_H_
#define _TEST_APP_H_


#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "vec4.h"
#include "font.h"
#include "geomfont.h"
#include "console.h"
#include "util.h"
#include "anim.h"
#include "hexmap.h"
#include "hexgame.h"
#include "test_app_list.h"
#include "minieditor.h"


#define TEST_APP_CONSOLE_START_TEXT "> "
#define TEST_APP_CONSOLE_W 60
#define TEST_APP_CONSOLE_H 35



enum {
    TEST_APP_MODE_GAME,
    TEST_APP_MODE_EDITOR,
    TEST_APP_MODES
};


typedef struct test_app {
    int scw, sch;
    int delay_goal;
    Uint32 took;
    bool developer_mode;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surface;

    const char *prend_filename;
    const char *stateset_filename;
    const char *hexmap_filename;
    const char *submap_filename;

    palette_t palette;
    SDL_Palette *sdl_palette;
    prismelrenderer_t prend;
    prismelrenderer_t minimap_prend;
    font_t font;
    geomfont_t *geomfont;
    console_t console;
    minieditor_t editor;

    stateset_t stateset;
    hexgame_t hexgame;
    camera_t *camera;
    prismelmapper_t *camera_mapper;

    bool hexgame_running;
    bool show_editor_controls;
    bool show_console; /* console is visible */
    bool process_console; /* console is grabbing input */
    int lines_printed; /* How many lines we've printed so far this frame */
    int mode;

    bool loop;

    char _recording_filename[200];

    test_app_list_t *list;
} test_app_t;




void test_app_cleanup(test_app_t *app);
void test_app_init_input(test_app_t *app);
int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename,
    const char *submap_filename, bool developer_mode,
    bool minimap_alt, bool cache_bitmaps,
    int n_players, int n_players_playing, bool load_game);
int test_app_mainloop(test_app_t *app);
int test_app_mainloop_step(test_app_t *app);
const char *test_app_get_last_recording_filename(test_app_t *app);
const char *test_app_get_next_recording_filename(test_app_t *app);
void test_app_blitter_render_init(test_app_t *app,
    geomfont_blitter_t *blitter,
    int x0, int y0);
int test_app_printf(test_app_t *app, int col, int row, const char *msg, ...);


/* test_app_commands.c */
int test_app_process_console_input(test_app_t *app);
void test_app_write_console_commands(test_app_t *app, const char *prefix);


/* test_app_console.c */
int test_app_process_event_console(test_app_t *app, SDL_Event *event);
int test_app_blit_console(test_app_t *app, int x, int y);
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