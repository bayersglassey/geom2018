
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "prismelrenderer.h"
#include "vec4.h"
#include "font.h"
#include "console.h"
#include "util.h"
#include "anim.h"


#define SCW 1024
#define SCH 768

/* How many milliseconds we want each frame to last */
#define DELAY_GOAL 30



typedef struct test_app {
    SDL_Window *window;
    SDL_Renderer *renderer;

    const char *prend_filename;
    const char *stateset_filename;
    const char *collmapset_filename;

    SDL_Palette *pal;
    prismelrenderer_t prend;
    font_t font;
    console_t console;
    int cur_rgraph_i;

    stateset_t stateset;
    hexcollmapset_t collmapset;

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


#include "anim.h"
#include "hexcollmap.h"
#include "util.h"


void test_app_cleanup(test_app_t *app){
    prismelrenderer_cleanup(&app->prend);
    stateset_cleanup(&app->stateset);
    hexcollmapset_cleanup(&app->collmapset);
}

int add_tile_rgraph(rendergraph_t *rgraph, rendergraph_t *rgraph2,
    vecspace_t *space, vec_t add, rot_t rot
){
    int err;
    rendergraph_trf_t *rendergraph_trf;
    err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
    if(err)return err;
    rendergraph_trf->rendergraph = rgraph2;
    rendergraph_trf->trf.rot = rot;
    vec_cpy(space->dims, rendergraph_trf->trf.add, add);
    return 0;
}

int test_app_load_map(test_app_t *app, const char *map_name){
    int err;

    prismelrenderer_t *prend = &app->prend;

    vec_t mul;
    vec4_set(mul, 3, 2, 0, -1);

    const char *vert_name = "map_vert";
    rendergraph_t *rgraph_vert = prismelrenderer_get_rendergraph(
        prend, vert_name);
    if(rgraph_vert == NULL){
        fprintf(stderr, "Couldn't find rgraph: %s\n", vert_name);
        return 2;}

    const char *edge_name = "map_edge";
    rendergraph_t *rgraph_edge = prismelrenderer_get_rendergraph(
        prend, edge_name);
    if(rgraph_edge == NULL){
        fprintf(stderr, "Couldn't find rgraph: %s\n", edge_name);
        return 2;}

    const char *face_name = "map_face";
    rendergraph_t *rgraph_face = prismelrenderer_get_rendergraph(
        prend, face_name);
    if(rgraph_face == NULL){
        fprintf(stderr, "Couldn't find rgraph: %s\n", face_name);
        return 2;}

    hexcollmap_t *collmap = hexcollmapset_get_collmap(&app->collmapset,
        map_name);
    if(collmap == NULL){
        fprintf(stderr, "Couldn't find map: %s\n", map_name);
        return 2;}

    ARRAY_PUSH_NEW(rendergraph_t, *prend, rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(map_name), &vec4,
        rendergraph_animation_type_default,
        rendergraph_n_frames_default);
    if(err)return err;

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            int px = x - collmap->ox;
            int py = y - collmap->oy;

            vec_t v;
            vec4_set(v, px + py, 0, -py, 0);
            vec_mul(prend->space, v, mul);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            for(int i = 0; i < 1; i++){
                if(tile->vert[i]){
                    err = add_tile_rgraph(rgraph, rgraph_vert,
                        prend->space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 3; i++){
                if(tile->edge[i]){
                    err = add_tile_rgraph(rgraph, rgraph_edge,
                        prend->space, v, i * 2);
                    if(err)return err;}}
            for(int i = 0; i < 2; i++){
                if(tile->face[i]){
                    err = add_tile_rgraph(rgraph, rgraph_face,
                        prend->space, v, i * 2);
                    if(err)return err;}}
        }
    }

    return 0;
}

int test_app_load_rendergraphs(test_app_t *app, bool reload){
    int err;

    if(reload){
        prismelrenderer_cleanup(&app->prend);
    }

    err = prismelrenderer_init(&app->prend, &vec4);
    if(err)return err;
    err = prismelrenderer_load(&app->prend, app->prend_filename);
    if(err)return err;

    if(app->prend.rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", app->prend_filename);
        return 2;}

    return 0;
}

int test_app_init(test_app_t *app, SDL_Window *window,
    SDL_Renderer *renderer, const char *prend_filename
){
    int err;

    app->window = window;
    app->renderer = renderer;
    app->prend_filename = prend_filename;

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

    app->stateset_filename = "data/anim.fus";
    err = stateset_load(&app->stateset,
        app->stateset_filename);
    if(err)return err;

    app->collmapset_filename = "data/map.fus";
    err = hexcollmapset_load(&app->collmapset,
        app->collmapset_filename);
    if(err)return err;

    err = font_load(&app->font, "data/font.fus");
    if(err)return err;

    err = console_init(&app->console, 80, 40, 20000);
    if(err)return err;

    app->cur_rgraph_i = 0;
    err = test_app_load_rendergraphs(app, false);
    if(err)return err;

    app->x0 = 0;
    app->y0 = 0;
    app->rot = 0;
    app->zoom = 1;
    app->frame_i = 0;
    app->loop = true;
    app->keydown_shift = false;
    app->keydown_ctrl = false;
    app->keydown_u = 0;
    app->keydown_d = 0;
    app->keydown_l = 0;
    app->keydown_r = 0;

    return 0;
}


int process_console_input(test_app_t *app){
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
                app->prend_filename = filename;
            }
        }

        err = test_app_load_rendergraphs(app, true);
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
    }else if(fus_lexer_got(&lexer, "loadmap")){
        char *map_name;
        err = fus_lexer_expect_str(&lexer, &map_name);
        if(err)goto lexer_err;
        err = test_app_load_map(app, map_name);
        if(err)return err;
        free(map_name);
    }else if(fus_lexer_got(&lexer, "dump")){
        bool dump_bitmap_surfaces = false;
        int dump_what = 0; /* rgraph, prend, states, maps */
        while(1){
            err = fus_lexer_next(&lexer);
            if(err)goto lexer_err;

            if(fus_lexer_done(&lexer))break;
            else if(fus_lexer_got(&lexer, "rgraph"))dump_what = 0;
            else if(fus_lexer_got(&lexer, "prend"))dump_what = 1;
            else if(fus_lexer_got(&lexer, "states"))dump_what = 2;
            else if(fus_lexer_got(&lexer, "maps"))dump_what = 3;
            else if(fus_lexer_got(&lexer, "surfaces")){
                dump_bitmap_surfaces = true;}
            else {
                console_write_msg(&app->console, "Dumper says: idunno\n");
                dump_what = -1; break;}
        }
        if(dump_what == 0){
            rendergraph_dump(rgraph, stdout, 0, dump_bitmap_surfaces);
        }else if(dump_what == 1){
            prismelrenderer_dump(&app->prend, stdout, dump_bitmap_surfaces);
        }else if(dump_what == 2){
            stateset_dump(&app->stateset, stdout);
        }else if(dump_what == 3){
            hexcollmapset_dump(&app->collmapset, stdout);
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
            fprintf(stderr, "Couldn't find map: %s\n", mapper_name);
            return 2;}
        rendergraph_t *mapped_rgraph = prismelrenderer_get_rendergraph(
            &app->prend, mapped_rgraph_name);
        if(mapper == NULL){
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
    err = test_app_init(&app, window, renderer, filename);

    SDL_Surface *render_surface = surface32_create(SCW, SCH,
        true, true);
    if(render_surface == NULL)return 2;

    SDL_Event event;

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
                app.prend_filename, app.cur_rgraph_i,
                app.prend.rendergraphs_len, rgraph->name,
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
        IF_APP_KEY(l, if(app.keydown_ctrl){app.x0 += 6;}else{app.rot += 1;})
        IF_APP_KEY(r, if(app.keydown_ctrl){app.x0 -= 6;}else{app.rot -= 1;})
        IF_APP_KEY(u, if(app.keydown_ctrl){app.y0 += 6;}else if(app.zoom < 10){app.zoom += 1;})
        IF_APP_KEY(d, if(app.keydown_ctrl){app.y0 -= 6;}else if(app.zoom > 1){app.zoom -= 1;})

        Uint32 tick1 = SDL_GetTicks();
        took = tick1 - tick0;
        if(took < DELAY_GOAL)SDL_Delay(DELAY_GOAL - took);
    }

    fprintf(stderr, "Cleaning up...\n");
    test_app_cleanup(&app);

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
