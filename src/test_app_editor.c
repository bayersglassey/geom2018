
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"


int test_app_render_editor(test_app_t *app){
    int err;

    RET_IF_SDL_NZ(SDL_FillRect(app->surface, NULL, 0));

    int line_y = 0;

    test_app_printf(app, 0, line_y * app->font.char_h,
        "Frame rendered in: %i ms (goal: %i ms)\n",
        app->took, app->delay_goal);
    line_y += 1;

    err = minieditor_render(&app->editor, &line_y);
    if(err)return err;

    err = test_app_blit_console(app, 0, line_y * app->font.char_h);
    if(err)return 2;

    return 0;
}

int test_app_process_event_editor(test_app_t *app, SDL_Event *event){
    return minieditor_process_event(&app->editor, event);
}

