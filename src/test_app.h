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
#include "test_app_menu.h"
#include "minieditor.h"


extern const char *TEST_MAP_HEXMAP_FILENAME_TITLE;
extern const char *TEST_MAP_HEXMAP_FILENAME_NEW_GAME;
extern const char *TEST_MAP_HEXMAP_FILENAME_DEFAULT;


#define TEST_APP_CONSOLE_START_TEXT "> "
#define TEST_APP_CONSOLE_W 60
#define TEST_APP_CONSOLE_H 35



enum test_app_state {
    TEST_APP_STATE_RUNNING,
    TEST_APP_STATE_TITLE_SCREEN,
    TEST_APP_STATE_START_GAME,
    TEST_APP_STATE_START_GAME_SKIP_TUTORIAL,
    TEST_APP_STATE_QUIT,
    TEST_APP_STATES
};

enum test_app_mode {
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

    const char *prend_filename;
    const char *stateset_filename;
    const char *actor_filename;

    palette_t palette;
    SDL_Palette *sdl_palette;
    prismelrenderer_t prend;
    prismelrenderer_t minimap_prend;
    font_t font;
    geomfont_t *geomfont;
    console_t console;
    minieditor_t editor;
    test_app_menu_t menu;
    test_app_list_t *list;

    stateset_t stateset;
    hexgame_t hexgame;
    camera_t *camera;
    prismelmapper_t *camera_mapper;

    bool developer_mode;
    bool minimap_alt;
    int n_players;
    bool animate_palettes;
    bool hexgame_running;
    bool show_editor_controls;
    bool show_console; /* console is visible */
    bool process_console; /* console is grabbing input */
    int lines_printed; /* How many lines we've printed so far this frame */
    int state; /* enum test_app_state */
    int mode; /* enum test_app_mode */
    bool show_menu;
    int save_slot;

    bool have_audio;
    SDL_AudioDeviceID audio_id;

    char _recording_filename[200]; /* HACKY :'( */
    const char *load_recording_filename;
    const char *save_recording_filename;
} test_app_t;




void test_app_cleanup(test_app_t *app);
int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *actor_filename,
    const char *hexmap_filename, const char *submap_filename,
    const char *location_name,
    bool developer_mode,
    bool minimap_alt, bool cache_bitmaps, bool animate_palettes,
    int n_players, int save_slot,
    const char *load_recording_filename,
    const char *save_recording_filename,
    bool have_audio, bool load_save_slot);
int test_app_reload_prismelrenderers(test_app_t *app);
int test_app_reload_map(test_app_t *app);
int test_app_mainloop(test_app_t *app);
int test_app_mainloop_step(test_app_t *app);
const char *test_app_get_load_recording_filename(test_app_t *app);
const char *test_app_get_save_recording_filename(test_app_t *app);
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
int test_app_render_game(test_app_t *app);
int test_app_process_event_game(test_app_t *app, SDL_Event *event);
int test_app_process_event_minimap(test_app_t *app, SDL_Event *event);


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
