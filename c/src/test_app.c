
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"
#include "prismelrenderer.h"
#include "vec4.h"
#include "font.h"
#include "console.h"
#include "util.h"
#include "anim.h"
#include "hexmap.h"
#include "hexgame.h"
#include "hexspace.h"




void test_app_cleanup(test_app_t *app){
    SDL_FreePalette(app->pal);
    prismelrenderer_cleanup(&app->prend);
    stateset_cleanup(&app->stateset);
    hexmap_cleanup(&app->hexmap);
    hexgame_cleanup(&app->hexgame);
}

static void test_app_init_input(test_app_t *app){
    app->keydown_shift = false;
    app->keydown_ctrl = false;
    app->keydown_u = 0;
    app->keydown_d = 0;
    app->keydown_l = 0;
    app->keydown_r = 0;
}

int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename
){
    int err;

    app->scw = scw;
    app->sch = sch;
    app->delay_goal = delay_goal;

    app->window = window;
    app->renderer = renderer;
    app->prend_filename = prend_filename;
    app->stateset_filename = stateset_filename;
    app->hexmap_filename = hexmap_filename;

    app->pal = SDL_AllocPalette(256);
    RET_IF_SDL_NULL(app->pal);
    RET_IF_SDL_NZ(SDL_SetPaletteColors(
        app->pal,
        (SDL_Color []){
            {.r=  0, .g=  0, .b=  0, .a=  0},
            {.r=  0, .g=  0, .b=  0, .a=255},
            {.r=255, .g= 60, .b= 60, .a=255},
            {.r= 60, .g=255, .b= 60, .a=255},
            {.r= 60, .g= 60, .b=255, .a=255},
            {.r=255, .g=255, .b= 60, .a=255},
            {.r= 60, .g=255, .b=255, .a=255},
            {.r=255, .g= 60, .b=255, .a=255},
            {.r=255, .g=255, .b=255, .a=255},
        },
        0, 9));

    err = font_load(&app->font, "data/font.fus");
    if(err)return err;

    err = console_init(&app->console, 80, 40, 20000);
    if(err)return err;

    err = prismelrenderer_init(&app->prend, &vec4);
    if(err)return err;
    err = prismelrenderer_load(&app->prend, app->prend_filename);
    if(err)return err;
    if(app->prend.rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", app->prend_filename);
        return 2;}

    err = stateset_init(&app->stateset);
    if(err)return err;
    err = stateset_load(&app->stateset, app->stateset_filename,
        &app->prend, &hexspace);
    if(err)return err;

    err = hexmap_load(&app->hexmap, &app->prend, app->hexmap_filename);
    if(err)return err;

    err = hexgame_init(&app->hexgame, &app->stateset, &app->hexmap);
    if(err)return err;

    app->cur_rgraph_i = 0;
    app->x0 = 0;
    app->y0 = 0;
    app->rot = 0;
    app->flip = false;
    app->zoom = 1;
    app->frame_i = 0;
    app->loop = true;
    app->hexgame_running = false;

    test_app_init_input(app);
    return 0;
}


int test_app_process_console_input(test_app_t *app){
    int err;

    rendergraph_t *rgraph =
        app->prend.rendergraphs[app->cur_rgraph_i];

    fus_lexer_t lexer;
    err = fus_lexer_init(&lexer, app->console.input, "<console input>");
    if(err)goto lexer_err;

    err = fus_lexer_next(&lexer);
    if(err)goto lexer_err;
    if(fus_lexer_got(&lexer, "exit")){
        app->loop = false;
    }else if(fus_lexer_got(&lexer, "cls")){
        console_clear(&app->console);
    }else if(fus_lexer_got(&lexer, "run")){
        app->hexgame_running = true;
        test_app_init_input(app);
        console_write_msg(&app->console, "Game started\n");
        SDL_StopTextInput();
        return 0;
    }else if(fus_lexer_got(&lexer, "save")){
        char *filename = NULL;

        err = fus_lexer_next(&lexer);
        if(err)goto lexer_err;
        if(!fus_lexer_done(&lexer)){
            err = fus_lexer_get_str(&lexer, &filename);
            if(err)goto lexer_err;
        }

        if(filename == NULL){
            err = prismelrenderer_write(&app->prend, stdout);
            if(err)return err;
        }else{
            err = prismelrenderer_save(&app->prend, filename);
            if(err)return err;
        }
    }else if(fus_lexer_got(&lexer, "dump")){
        bool dump_bitmap_surfaces = false;
        int dump_what = 0; /* rgraph, prend */
        while(1){
            err = fus_lexer_next(&lexer);
            if(err)goto lexer_err;

            if(fus_lexer_done(&lexer))break;
            else if(fus_lexer_got(&lexer, "rgraph"))dump_what = 0;
            else if(fus_lexer_got(&lexer, "prend"))dump_what = 1;
            else if(fus_lexer_got(&lexer, "surfaces")){
                /* WARNING: doing this with "prend" after "renderall" causes
                my laptop to hang... */
                dump_bitmap_surfaces = true;}
            else {
                console_write_msg(&app->console, "Dumper says: idunno\n");
                dump_what = -1; break;}
        }
        if(dump_what == 0){
            rendergraph_dump(rgraph, stdout, 0, dump_bitmap_surfaces);
        }else if(dump_what == 1){
            prismelrenderer_dump(&app->prend, stdout, dump_bitmap_surfaces);
        }
    }else if(fus_lexer_got(&lexer, "map")){
        char *mapper_name;
        char *mapped_rgraph_name;
        char *resulting_rgraph_name = NULL;

        err = fus_lexer_expect_str(&lexer, &mapper_name);
        if(err)return err;
        err = fus_lexer_expect_str(&lexer, &mapped_rgraph_name);
        if(err)return err;
        err = fus_lexer_next(&lexer);
        if(err)return err;
        if(!fus_lexer_done(&lexer)){
            err = fus_lexer_get_str(&lexer, &resulting_rgraph_name);
            if(err)return err;
        }

        prismelmapper_t *mapper = prismelrenderer_get_mapper(
            &app->prend, mapper_name);
        if(mapper == NULL){
            fprintf(stderr, "Couldn't find mapper: %s\n", mapper_name);
            return 2;}
        rendergraph_t *mapped_rgraph = prismelrenderer_get_rendergraph(
            &app->prend, mapped_rgraph_name);
        if(mapped_rgraph == NULL){
            fprintf(stderr, "Couldn't find shape: %s\n",
                mapped_rgraph_name);
            return 2;}

        free(mapper_name);
        free(mapped_rgraph_name);

        rendergraph_t *rgraph;
        err = prismelmapper_apply_to_rendergraph(mapper, &app->prend,
            mapped_rgraph, resulting_rgraph_name, app->prend.space,
            &rgraph);
        if(err)return err;
    }else if(fus_lexer_got(&lexer, "renderall")){
        SDL_Renderer *renderer = NULL;
        err = fus_lexer_next(&lexer);
        if(err)goto lexer_err;

        /* WARNING: as of June 16 2018, the following causes my laptop
        to hang... */
        if(fus_lexer_got(&lexer, "R"))renderer = app->renderer;

        err = prismelrenderer_render_all_bitmaps(
            &app->prend, app->pal, renderer);
        if(err)return err;
    }else{
        fus_lexer_unexpected(&lexer, NULL);
        console_write_msg(&app->console, "Sorry, what?\n");
        return 0;
    }

    console_write_msg(&app->console, "OK\n");
    return 0;
lexer_err:
    console_write_msg(&app->console, "That didn't work\n");
    return 0;
}

int test_app_blit_rgraph(test_app_t *app, rendergraph_t *rgraph,
    vec_t pos, rot_t rot, flip_t flip, int frame_i, prismelmapper_t *mapper
){
    int err;

    int animated_frame_i = get_animated_frame_i(
        rgraph->animation_type, rgraph->n_frames, frame_i);

    if(mapper != NULL){
        err = prismelmapper_apply_to_rendergraph(mapper, &app->prend, rgraph,
            NULL, rgraph->space, &rgraph);
        if(err)return err;

        vec_mul(mapper->space, pos, mapper->unit);
    }

    rendergraph_bitmap_t *bitmap;
    err = rendergraph_get_or_render_bitmap(rgraph, &bitmap,
        rot, flip, animated_frame_i, app->pal, app->renderer);
    if(err)return err;

    /* Can't create a texture with either dimension 0, so exit early */
    if(bitmap->surface->w == 0 || bitmap->surface->h == 0)return 0;

    int x0 = app->scw / 2 + app->x0;
    int y0 = app->sch / 2 + app->y0;

    int x, y;
    rgraph->space->vec_render(pos, &x, &y);

    SDL_Rect dst_rect = {
        x0 + (x - bitmap->pbox.x) * app->zoom,
        y0 + (y - bitmap->pbox.y) * app->zoom,
        bitmap->pbox.w * app->zoom,
        bitmap->pbox.h * app->zoom
    };
    SDL_Texture *bitmap_texture;
    err = rendergraph_bitmap_get_texture(bitmap, app->renderer,
        &bitmap_texture);
    if(err)return err;
    RET_IF_SDL_NZ(SDL_RenderCopy(app->renderer, bitmap_texture,
        NULL, &dst_rect));

    return 0;
}


int test_app_mainloop(test_app_t *app){
    int err;

    SDL_Surface *render_surface = surface32_create(app->scw, app->sch,
        true, true);
    if(render_surface == NULL)return 2;

    Uint32 took = 0;

    SDL_StartTextInput();
    while(app->loop){
        Uint32 tick0 = SDL_GetTicks();

        rendergraph_t *rgraph =
            app->prend.rendergraphs[app->cur_rgraph_i];
        int animated_frame_i = get_animated_frame_i(
            rgraph->animation_type, rgraph->n_frames, app->frame_i);

        if(app->hexgame_running){
            err = hexgame_step(&app->hexgame);
            if(err)return err;
            err = hexgame_render(&app->hexgame, app);
            if(err)return err;
        }else{

            /******************************************************************
            * Blit stuff onto render_surface
            */

            RET_IF_SDL_NZ(SDL_FillRect(render_surface, NULL, 0));

            font_blitmsg(&app->font, render_surface, 0, 0,
                "Game running? %c\n"
                "Frame rendered in: %i ms\n"
                "  (Aiming for sub-%i ms)\n"
                "Controls:\n"
                "  up/down - zoom (hold shift for tap mode)\n"
                "  left/right - rotate (hold shift for tap mode)\n"
                "  control + up/down/left/right - pan (hold shift...)\n"
                "  page up/down - cycle through available rendergraphs\n"
                "  0 - reset rotation\n"
                "Currently displaying rendergraphs from file: %s\n"
                "Currently displaying rendergraph %i / %i: %s\n"
                "  pan=(%i,%i), rot = %i, flip = %c, zoom = %i,"
                    " frame_i = %i (%i) / %i (%s)",
                app->hexgame_running? 'y': 'n', took, app->delay_goal,
                app->prend_filename, app->cur_rgraph_i,
                app->prend.rendergraphs_len, rgraph->name,
                app->x0, app->y0, app->rot, app->flip? 'y': 'n', app->zoom,
                app->frame_i, animated_frame_i,
                rgraph->n_frames, rgraph->animation_type);

            console_blit(&app->console, &app->font, render_surface,
                0, 12 * app->font.char_h);


            /******************************************************************
            * Render stuff onto renderer
            */

            RET_IF_SDL_NZ(SDL_SetRenderDrawColor(app->renderer,
                0, 0, 0, 255));
            RET_IF_SDL_NZ(SDL_RenderClear(app->renderer));

            err = test_app_blit_rgraph(app, rgraph, (vec_t){0}, app->rot,
                app->flip, app->frame_i, NULL);
            if(err)return err;

            SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
                app->renderer, render_surface);
            RET_IF_SDL_NULL(render_surface);
            SDL_RenderCopy(app->renderer, render_texture, NULL, NULL);
            SDL_DestroyTexture(render_texture);


            /******************************************************************
            * Present it
            */

            SDL_RenderPresent(app->renderer);

        }

        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_QUIT){
                app->loop = false; break;}

            if(event.type == SDL_KEYDOWN
                && event.key.keysym.sym == SDLK_ESCAPE){
                    if(app->hexgame_running){
                        app->hexgame_running = false;
                        console_write_msg(&app->console, "Game stopped\n");
                        SDL_StartTextInput();
                    }else{
                        console_write_msg(&app->console,
                            "Game already stopped - try \"exit\"\n");}
                    continue;}

            if(app->hexgame_running){
                err = hexgame_process_event(&app->hexgame, &event);
                if(err)return err;
                continue;}

            switch(event.type){
                case SDL_KEYDOWN: {
                    if(event.key.keysym.sym == SDLK_RETURN){
                        console_newline(&app->console);

                        err = test_app_process_console_input(app);
                        if(err)return err;

                        console_input_clear(&app->console);
                    }
                    if(event.key.keysym.sym == SDLK_BACKSPACE){
                        console_input_backspace(&app->console);}
                    if(event.key.keysym.sym == SDLK_DELETE){
                        console_input_delete(&app->console);}
                    if(
                        event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)
                        && event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)
                    ){
                        if(event.key.keysym.sym == SDLK_c){
                            SDL_SetClipboardText(app->console.input);}
                        if(event.key.keysym.sym == SDLK_v
                            && SDL_HasClipboardText()
                        ){
                            char *input = SDL_GetClipboardText();
                            char *c = input;
                            while(*c != '\0'){
                                console_input_char(&app->console, *c);
                                c++;
                            }
                            SDL_free(input);
                        }
                    }

                    if(event.key.keysym.sym == SDLK_0){
                        app->x0 = 0; app->y0 = 0;
                        app->rot = 0; app->flip = false; app->zoom = 1;}

                    if(event.key.keysym.sym == SDLK_SPACE){
                        app->flip = !app->flip;}

                    #define IF_KEYDOWN(SYM, KEY) \
                        if(event.key.keysym.sym == SDLK_##SYM \
                            && app->keydown_##KEY == 0){ \
                                app->keydown_##KEY = 2;}
                    IF_KEYDOWN(UP, u)
                    IF_KEYDOWN(DOWN, d)
                    IF_KEYDOWN(LEFT, l)
                    IF_KEYDOWN(RIGHT, r)
                    #undef IF_KEYDOWN

                    if(event.key.keysym.sym == SDLK_LSHIFT
                        || event.key.keysym.sym == SDLK_RSHIFT){
                            app->keydown_shift = true;}
                    if(event.key.keysym.sym == SDLK_LCTRL
                        || event.key.keysym.sym == SDLK_RCTRL){
                            app->keydown_ctrl = true;}

                    if(event.key.keysym.sym == SDLK_PAGEUP){
                        app->cur_rgraph_i++;
                        if(app->cur_rgraph_i >=
                            app->prend.rendergraphs_len){
                                app->cur_rgraph_i = 0;}}
                    if(event.key.keysym.sym == SDLK_PAGEDOWN){
                        app->cur_rgraph_i--;
                        if(app->cur_rgraph_i < 0){
                            app->cur_rgraph_i =
                                app->prend.rendergraphs_len - 1;}}
                    if(event.key.keysym.sym == SDLK_HOME){
                        app->frame_i++;}
                    if(event.key.keysym.sym == SDLK_END){
                        if(app->frame_i > 0)app->frame_i--;}
                } break;
                case SDL_KEYUP: {

                    #define IF_KEYUP(SYM, KEY) \
                        if(event.key.keysym.sym == SDLK_##SYM){ \
                            app->keydown_##KEY = 0;}
                    IF_KEYUP(UP, u)
                    IF_KEYUP(DOWN, d)
                    IF_KEYUP(LEFT, l)
                    IF_KEYUP(RIGHT, r)
                    #undef IF_KEYUP

                    if(event.key.keysym.sym == SDLK_LSHIFT
                        || event.key.keysym.sym == SDLK_RSHIFT){
                            app->keydown_shift = false;}
                    if(event.key.keysym.sym == SDLK_LCTRL
                        || event.key.keysym.sym == SDLK_RCTRL){
                            app->keydown_ctrl = false;}
                } break;
                case SDL_TEXTINPUT: {
                    for(char *c = event.text.text; *c != '\0'; c++){
                        console_input_char(&app->console, *c);
                    }
                } break;
                default: break;
            }
        }

        #define IF_APP_KEY(KEY, BODY) \
            if(app->keydown_##KEY >= (app->keydown_shift? 2: 1)){ \
                app->keydown_##KEY = 1; \
                BODY}
        IF_APP_KEY(l, if(app->keydown_ctrl){app->x0 += 6;}else{app->rot += 1;})
        IF_APP_KEY(r, if(app->keydown_ctrl){app->x0 -= 6;}else{app->rot -= 1;})
        IF_APP_KEY(u, if(app->keydown_ctrl){app->y0 += 6;}else if(app->zoom < 10){app->zoom += 1;})
        IF_APP_KEY(d, if(app->keydown_ctrl){app->y0 -= 6;}else if(app->zoom > 1){app->zoom -= 1;})
        #undef IF_APP_KEY

        Uint32 tick1 = SDL_GetTicks();
        took = tick1 - tick0;
        if(took < app->delay_goal)SDL_Delay(app->delay_goal - took);
    }
    SDL_StopTextInput();

    return 0;
}

