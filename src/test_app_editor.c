
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"


int test_app_render_editor(test_app_t *app){
    int err;

    rendergraph_t *rgraph =
        app->prend.rendergraphs[app->cur_rgraph_i];
    int animated_frame_i = get_animated_frame_i(
        rgraph->animation_type, rgraph->n_frames, app->frame_i);

    /******************************************************************
    * Clear screen
    */

    RET_IF_SDL_NZ(SDL_FillRect(app->surface, NULL, 0));

    /******************************************************************
    * Render rgraph
    */

    int x0 = app->scw / 2 + app->x0;
    int y0 = app->sch / 2 + app->y0;
    err = rendergraph_render(rgraph, app->renderer, app->surface,
        app->sdl_palette, &app->prend, x0, y0, app->zoom,
        (vec_t){0}, app->rot, app->flip, app->frame_i, NULL);
    if(err)return err;

    /******************************************************************
    * Render text
    */

    int line_y = 0;

    if(app->show_editor_controls){
        FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y * app->font.char_h),
            "Frame rendered in: %i ms (goal: %i ms)\n"
            "# Textures in use: %i\n"
            "Controls:\n"
            "  up/down - zoom (hold shift for tap mode)\n"
            "  left/right - rotate (hold shift for tap mode)\n"
            "  control + up/down/left/right - pan (hold shift...)\n"
            "  page up/down - cycle through available rendergraphs\n"
            "  0 - reset rotation\n"
            "Displaying rendergraphs from file: %s\n"
            "Rendergraph %i / %i: %s\n"
            "  pan=(%i,%i), rot = %i, flip = %c, zoom = %i\n"
            "  frame_i = %i (%i) / %i (%s)",
            app->took, app->delay_goal,
            app->prend.n_textures,
            app->prend_filename,
            app->cur_rgraph_i, app->prend.rendergraphs_len, rgraph->name,
            app->x0, app->y0, app->rot, app->flip? 'y': 'n', app->zoom,
            app->frame_i, animated_frame_i, rgraph->n_frames, rgraph->animation_type);

        line_y += 12;
    }

    err = test_app_blit_console(app, app->surface,
        0, line_y * app->font.char_h);
    if(err)return 2;

    /******************************************************************
    * Draw to renderer and present it
    */

    {
        SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
            app->renderer, app->surface);
        RET_IF_SDL_NULL(render_texture);
        SDL_RenderCopy(app->renderer, render_texture, NULL, NULL);
        SDL_DestroyTexture(render_texture);
    }

    SDL_RenderPresent(app->renderer);
    return 0;
}

int test_app_process_event_editor(test_app_t *app, SDL_Event *event){
    int err;
    switch(event->type){
        case SDL_KEYDOWN: {
            if(event->key.keysym.sym == SDLK_RETURN){
                app->show_editor_controls = !app->show_editor_controls;}

            if(event->key.keysym.sym == SDLK_0){
                app->x0 = 0; app->y0 = 0;
                app->rot = 0; app->flip = false; app->zoom = 1;}

            if(event->key.keysym.sym == SDLK_SPACE){
                app->flip = !app->flip;}

            #define IF_KEYDOWN(SYM, KEY) \
                if(event->key.keysym.sym == SDLK_##SYM \
                    && app->keydown_##KEY == 0){ \
                        app->keydown_##KEY = 2;}
            IF_KEYDOWN(UP, u)
            IF_KEYDOWN(DOWN, d)
            IF_KEYDOWN(LEFT, l)
            IF_KEYDOWN(RIGHT, r)
            #undef IF_KEYDOWN

            if(event->key.keysym.sym == SDLK_LSHIFT
                || event->key.keysym.sym == SDLK_RSHIFT){
                    app->keydown_shift = true;}
            if(event->key.keysym.sym == SDLK_LCTRL
                || event->key.keysym.sym == SDLK_RCTRL){
                    app->keydown_ctrl = true;}

            if(event->key.keysym.sym == SDLK_PAGEUP){
                app->cur_rgraph_i++;
                if(app->cur_rgraph_i >=
                    app->prend.rendergraphs_len){
                        app->cur_rgraph_i = 0;}}

            if(event->key.keysym.sym == SDLK_PAGEDOWN){
                app->cur_rgraph_i--;
                if(app->cur_rgraph_i < 0){
                    app->cur_rgraph_i =
                        app->prend.rendergraphs_len - 1;}}

            if(event->key.keysym.sym == SDLK_HOME){
                app->frame_i++;}
            if(event->key.keysym.sym == SDLK_END){
                if(app->frame_i > 0)app->frame_i--;}

        } break;
        case SDL_KEYUP: {

            #define IF_KEYUP(SYM, KEY) \
                if(event->key.keysym.sym == SDLK_##SYM){ \
                    app->keydown_##KEY = 0;}
            IF_KEYUP(UP, u)
            IF_KEYUP(DOWN, d)
            IF_KEYUP(LEFT, l)
            IF_KEYUP(RIGHT, r)
            #undef IF_KEYUP

            if(event->key.keysym.sym == SDLK_LSHIFT
                || event->key.keysym.sym == SDLK_RSHIFT){
                    app->keydown_shift = false;}
            if(event->key.keysym.sym == SDLK_LCTRL
                || event->key.keysym.sym == SDLK_RCTRL){
                    app->keydown_ctrl = false;}
        } break;
        default: break;
    }

    #define IF_APP_KEY(KEY, BODY) \
        if(app->keydown_##KEY >= (app->keydown_shift? 2: 1)){ \
            app->keydown_##KEY = 1; \
            BODY}
    IF_APP_KEY(l, if(app->keydown_ctrl){app->x0 += 6;}else{app->rot += 1;})
    IF_APP_KEY(r, if(app->keydown_ctrl){app->x0 -= 6;}else{app->rot -= 1;})
    IF_APP_KEY(u, if(app->keydown_ctrl){app->y0 += 6;}else if(app->zoom < MAX_ZOOM){app->zoom += 1;})
    IF_APP_KEY(d, if(app->keydown_ctrl){app->y0 -= 6;}else if(app->zoom > 1){app->zoom -= 1;})
    #undef IF_APP_KEY

    return 0;
}
