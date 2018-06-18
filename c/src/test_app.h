#ifndef _TEST_APP_H_
#define _TEST_APP_H_


#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "vec4.h"
#include "font.h"
#include "console.h"
#include "util.h"
#include "anim.h"
#include "hexmap.h"
#include "hexgame.h"


typedef struct test_app {
    int scw, sch;
    int delay_goal;

    SDL_Window *window;
    SDL_Renderer *renderer;

    const char *prend_filename;
    const char *stateset_filename;
    const char *hexmap_filename;

    SDL_Palette *pal;
    prismelrenderer_t prend;
    font_t font;
    console_t console;
    int cur_rgraph_i;

    stateset_t stateset;
    hexmap_t hexmap;
    hexgame_t hexgame;
    bool hexgame_running;

    int x0;
    int y0;
    int rot;
    int zoom;
    int frame_i;
    bool loop;
    bool keydown_shift;
    bool keydown_ctrl;
    int keydown_u;
    int keydown_d;
    int keydown_l;
    int keydown_r;
} test_app_t;




void test_app_cleanup(test_app_t *app);
int test_app_load_rendergraphs(test_app_t *app);
int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename);
int test_app_process_console_input(test_app_t *app);
int test_app_blit_rgraph(test_app_t *app, rendergraph_t *rgraph,
    vec_t pos, rot_t rot, flip_t flip, int frame_i);
int test_app_mainloop(test_app_t *app);


#endif