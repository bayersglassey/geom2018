
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "vec4.h"
#include "font.h"
#include "console.h"
#include "util.h"


#define SCW 1024
#define SCH 768

/* How many milliseconds we want each frame to last */
#define DELAY_GOAL 30



typedef struct test_app {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Palette *pal;
    prismelrenderer_t prend;
    font_t font;
    console_t console;
    const char *filename;
    int cur_rgraph_i;

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


int load_rendergraphs(test_app_t *app, bool reload){
    int err;

    if(reload){
        prismelrenderer_cleanup(&app->prend);
    }

    err = prismelrenderer_load(&app->prend, app->filename, &vec4);
    if(err)return err;

    if(app->prend.rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", app->filename);
        return 2;}

    return 0;
}

int process_console_input(test_app_t *app){
    int err;

    rendergraph_t *rgraph =
        app->prend.rendergraphs[app->cur_rgraph_i];

    fus_lexer_t lexer;
    err = fus_lexer_init(&lexer, app->console.input);
    if(err)goto lexer_err;

    err = fus_lexer_next(&lexer);
    if(err)goto lexer_err;
    if(fus_lexer_got(&lexer, "exit")){
        app->loop = false;
    }else if(fus_lexer_got(&lexer, "cls")){
        console_clear(&app->console);
    }else if(fus_lexer_got(&lexer, "reload")){
        err = fus_lexer_next(&lexer);
        if(err)goto lexer_err;
        if(!fus_lexer_done(&lexer)){
            char *filename;
            err = fus_lexer_get_str(&lexer, &filename);
            if(err)goto lexer_err;

            FILE *f = fopen(filename, "r");
            if(f == NULL){
                fprintf(stderr, "Could not open file: %s\n", filename);
                console_write_msg(&app->console, "Could not open file\n");
                return 0;
            }else{
                err = fclose(f);
                if(err)return err;
                app->filename = filename;
            }
        }

        err = load_rendergraphs(app, true);
        if(err)return err;
        app->cur_rgraph_i = 0;
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
        err = fus_lexer_next(&lexer);
        if(err)goto lexer_err;
        if(fus_lexer_got(&lexer, "S"))dump_bitmap_surfaces = true;
        rendergraph_dump(rgraph, stdout, 0, dump_bitmap_surfaces);
    }else if(fus_lexer_got(&lexer, "dumpall")){
        bool dump_bitmap_surfaces = false;
        err = fus_lexer_next(&lexer);
        if(err)goto lexer_err;
        if(fus_lexer_got(&lexer, "S"))dump_bitmap_surfaces = true;
        prismelrenderer_dump(&app->prend, stdout, dump_bitmap_surfaces);
    }else if(fus_lexer_got(&lexer, "renderall")){
        SDL_Renderer *renderer = NULL;
        err = fus_lexer_next(&lexer);
        if(err)goto lexer_err;
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


int mainloop(SDL_Window *window, SDL_Renderer *renderer,
    const char *filename
){
    int err;

    test_app_t app;
    app.window = window;
    app.renderer = renderer;
    app.filename = filename;

    app.pal = SDL_AllocPalette(256);
    RET_IF_SDL_NULL(app.pal);
    RET_IF_SDL_NZ(SDL_SetPaletteColors(
        app.pal,
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

    SDL_Surface *render_surface = surface32_create(SCW, SCH,
        true, true);
    if(render_surface == NULL)return 2;

    err = font_load(&app.font, "data/font.fus");
    if(err)return err;

    err = console_init(&app.console, 80, 40, 20000);
    if(err)return err;

    app.cur_rgraph_i = 0;
    err = load_rendergraphs(&app, false);
    if(err)return err;

    SDL_Event event;

    app.x0 = 0;
    app.y0 = 0;
    app.rot = 0;
    app.zoom = 1;
    app.frame_i = 0;
    app.loop = true;
    app.keydown_shift = false;
    app.keydown_ctrl = false;
    app.keydown_u = 0;
    app.keydown_d = 0;
    app.keydown_l = 0;
    app.keydown_r = 0;

    Uint32 took = 0;
    bool refresh = true;

    SDL_StartTextInput();
    while(app.loop){
        Uint32 tick0 = SDL_GetTicks();

        rendergraph_t *rgraph =
            app.prend.rendergraphs[app.cur_rgraph_i];
        int animated_frame_i = get_animated_frame_i(
            rgraph->animation_type, rgraph->n_frames, app.frame_i);

        if(refresh){
            refresh = false;

            /******************************************************************
            * Blit stuff onto render_surface
            */

            RET_IF_SDL_NZ(SDL_FillRect(render_surface, NULL, 0));

            font_blitmsg(&app.font, render_surface, 0, 0,
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
                "  pan=(%i,%i), rot = %i, zoom = %i,"
                    " frame_i = %i (%i) / %i (%s)",
                took, (int)DELAY_GOAL,
                app.filename, app.cur_rgraph_i, app.prend.rendergraphs_len,
                rgraph->name,
                app.x0, app.y0, app.rot, app.zoom, app.frame_i,
                animated_frame_i,
                rgraph->n_frames, rgraph->animation_type);

            console_blit(&app.console, &app.font, render_surface,
                0, 12 * app.font.char_h);


            /******************************************************************
            * Render stuff onto renderer
            */

            RET_IF_SDL_NZ(SDL_SetRenderDrawColor(renderer,
                0, 0, 0, 255));
            RET_IF_SDL_NZ(SDL_RenderClear(renderer));

            rendergraph_bitmap_t *bitmap;
            err = rendergraph_get_or_render_bitmap(
                rgraph, &bitmap,
                app.rot, false, animated_frame_i, app.pal, renderer);
            if(err)return err;

            SDL_Rect dst_rect = {
                SCW / 2 + app.x0 - bitmap->pbox.x * app.zoom,
                SCH / 2 + app.y0 - bitmap->pbox.y * app.zoom,
                bitmap->pbox.w * app.zoom,
                bitmap->pbox.h * app.zoom
            };
            SDL_Texture *bitmap_texture;
            err = rendergraph_bitmap_get_texture(bitmap, renderer,
                &bitmap_texture);
            if(err)return err;
            RET_IF_SDL_NZ(SDL_RenderCopy(renderer, bitmap_texture,
                NULL, &dst_rect));

            SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
                renderer, render_surface);
            RET_IF_SDL_NULL(render_surface);
            SDL_RenderCopy(renderer, render_texture, NULL, NULL);
            SDL_DestroyTexture(render_texture);


            /******************************************************************
            * Present it
            */

            SDL_RenderPresent(renderer);

        }

        while(SDL_PollEvent(&event)){
            switch(event.type){
                case SDL_KEYDOWN: {
                    if(event.key.keysym.sym == SDLK_ESCAPE){
                        app.loop = false;}

                    if(event.key.keysym.sym == SDLK_RETURN){
                        console_newline(&app.console);

                        err = process_console_input(&app);
                        if(err)return err;

                        console_input_clear(&app.console);
                        refresh = true;
                    }
                    if(event.key.keysym.sym == SDLK_BACKSPACE){
                        console_input_backspace(&app.console);
                        refresh = true;}
                    if(event.key.keysym.sym == SDLK_DELETE){
                        console_input_delete(&app.console);
                        refresh = true;}
                    if(
                        event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)
                        && event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)
                    ){
                        if(event.key.keysym.sym == SDLK_c){
                            SDL_SetClipboardText(app.console.input);}
                        if(event.key.keysym.sym == SDLK_v
                            && SDL_HasClipboardText()
                        ){
                            char *input = SDL_GetClipboardText();
                            char *c = input;
                            while(*c != '\0'){
                                console_input_char(&app.console, *c);
                                c++;
                            }
                            SDL_free(input);
                            refresh = true;
                        }
                    }

                    if(event.key.keysym.sym == SDLK_0){
                        app.x0 = 0; app.y0 = 0; app.rot = 0; app.zoom = 1;
                        refresh = true;}

                    #define IF_KEYDOWN(SYM, KEY) \
                        if(event.key.keysym.sym == SDLK_##SYM \
                            && app.keydown_##KEY == 0){ \
                                app.keydown_##KEY = 2;}
                    IF_KEYDOWN(UP, u)
                    IF_KEYDOWN(DOWN, d)
                    IF_KEYDOWN(LEFT, l)
                    IF_KEYDOWN(RIGHT, r)

                    if(event.key.keysym.sym == SDLK_LSHIFT
                        || event.key.keysym.sym == SDLK_RSHIFT){
                            app.keydown_shift = true;}
                    if(event.key.keysym.sym == SDLK_LCTRL
                        || event.key.keysym.sym == SDLK_RCTRL){
                            app.keydown_ctrl = true;}

                    if(event.key.keysym.sym == SDLK_PAGEUP){
                        app.cur_rgraph_i++;
                        if(app.cur_rgraph_i >=
                            app.prend.rendergraphs_len){
                                app.cur_rgraph_i = 0;}
                        refresh = true;}
                    if(event.key.keysym.sym == SDLK_PAGEDOWN){
                        app.cur_rgraph_i--;
                        if(app.cur_rgraph_i < 0){
                            app.cur_rgraph_i =
                                app.prend.rendergraphs_len - 1;}
                        refresh = true;}
                    if(event.key.keysym.sym == SDLK_HOME){
                        app.frame_i++;
                        refresh = true;}
                    if(event.key.keysym.sym == SDLK_END){
                        if(app.frame_i > 0)app.frame_i--;
                        refresh = true;}
                } break;
                case SDL_KEYUP: {

                    #define IF_KEYUP(SYM, KEY) \
                        if(event.key.keysym.sym == SDLK_##SYM){ \
                            app.keydown_##KEY = 0;}
                    IF_KEYUP(UP, u)
                    IF_KEYUP(DOWN, d)
                    IF_KEYUP(LEFT, l)
                    IF_KEYUP(RIGHT, r)

                    if(event.key.keysym.sym == SDLK_LSHIFT
                        || event.key.keysym.sym == SDLK_RSHIFT){
                            app.keydown_shift = false;}
                    if(event.key.keysym.sym == SDLK_LCTRL
                        || event.key.keysym.sym == SDLK_RCTRL){
                            app.keydown_ctrl = false;}
                } break;
                case SDL_TEXTINPUT: {
                    for(char *c = event.text.text; *c != '\0'; c++){
                        console_input_char(&app.console, *c);
                    }
                    refresh = true;
                } break;
                case SDL_QUIT: app.loop = false; break;
                default: break;
            }
        }

        #define IF_APP_KEY(KEY, BODY) \
            if(app.keydown_##KEY >= (app.keydown_shift? 2: 1)){ \
                refresh = true; app.keydown_##KEY = 1; \
                BODY}
        IF_APP_KEY(l, if(app.keydown_ctrl){app.x0 -= 6;}else{app.rot += 1;})
        IF_APP_KEY(r, if(app.keydown_ctrl){app.x0 += 6;}else{app.rot -= 1;})
        IF_APP_KEY(u, if(app.keydown_ctrl){app.y0 -= 6;}else if(app.zoom < 10){app.zoom += 1;})
        IF_APP_KEY(d, if(app.keydown_ctrl){app.y0 += 6;}else if(app.zoom > 1){app.zoom -= 1;})

        Uint32 tick1 = SDL_GetTicks();
        took = tick1 - tick0;
        if(took < DELAY_GOAL)SDL_Delay(DELAY_GOAL - took);
    }

    return 0;
}

int main(int n_args, char *args[]){
    int e = 0;
    Uint32 window_flags = SDL_WINDOW_SHOWN;
    const char *filename = "data/test.fus";

    for(int arg_i = 1; arg_i < n_args; arg_i++){
        char *arg = args[arg_i];
        if(!strcmp(arg, "-F")){
            window_flags |= SDL_WINDOW_FULLSCREEN;
        }else if(!strcmp(arg, "-FD")){
            window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }else if(!strcmp(arg, "-f")){
            arg_i++;
            if(arg_i >= n_args){
                fprintf(stderr, "Missing filename after -f\n");
                return 2;
            }
            arg = args[arg_i];
            filename = arg;
        }else{
            fprintf(stderr, "Unrecognized option: %s\n", arg);
            return 2;
        }
    }

    if(SDL_Init(SDL_INIT_VIDEO)){
        e = 1;
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    }else{
        SDL_Window *window = SDL_CreateWindow("Hello World!",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCW, SCH, window_flags);

        if(!window){
            e = 1;
            fprintf(stderr, "SDL_CreateWindow error: %s\n",
                SDL_GetError());
        }else{
            SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

            if(!renderer){
                e = 1;
                fprintf(stderr, "SDL_CreateRenderer error: %s\n",
                    SDL_GetError());
            }else{
                e = mainloop(window, renderer, filename);
                SDL_DestroyRenderer(renderer);
            }
            SDL_DestroyWindow(window);
        }
        SDL_Quit();
    }
    fprintf(stderr, "Exiting with code: %i\n", e);
    return e;
}
