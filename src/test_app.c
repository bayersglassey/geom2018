
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"
#include "prismelrenderer.h"
#include "rendergraph.h"
#include "array.h"
#include "vec4.h"
#include "font.h"
#include "sdlfont.h"
#include "geomfont.h"
#include "console.h"
#include "util.h"
#include "anim.h"
#include "hexmap.h"
#include "hexgame.h"
#include "hexspace.h"


#define MAX_ZOOM 4

#ifndef SDLFONT
    #define FONT_BLITTER_T geomfont_blitter_t
    #define FONT_BLITTER_INIT geomfont_blitter_render_init
    #define FONT_BLITTER_PUTC_CALLBACK geomfont_blitter_putc_callback
    #define FONT_PRINTF geomfont_render_printf
    #define FONT_ARGS(SURFACE, X0, Y0) app->geomfont, app->renderer, (SURFACE), \
        app->sdl_palette, (X0), (Y0), 1, NULL, NULL
#else
    #define FONT_BLITTER_T sdlfont_blitter_t
    #define FONT_BLITTER_INIT sdlfont_blitter_init
    #define FONT_BLITTER_PUTC_CALLBACK sdlfont_blitter_putc_callback
    #define FONT_PRINTF sdlfont_printf
    #define FONT_ARGS(SURFACE, X0, Y0) &app->sdlfont, (SURFACE), (X0), (Y0)
#endif


/*******************
* STATIC UTILITIES *
*******************/

static void test_app_init_input(test_app_t *app){
    app->keydown_shift = false;
    app->keydown_ctrl = false;
    app->keydown_u = 0;
    app->keydown_d = 0;
    app->keydown_l = 0;
    app->keydown_r = 0;
}

static int new_game_callback(hexgame_t *game, player_t *player,
    const char *map_filename
){
    int err;
    hexmap_t *map;
    err = hexgame_get_or_load_map(game, map_filename, &map);
    if(err)return err;
    return hexgame_reset_players(game, RESET_HARD, map);
}

static int continue_callback(hexgame_t *game, player_t *player){
    int err;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        bool file_found;
        err = player_reload(player, &file_found);
        if(err)return err;
        if(!file_found){
            err = hexgame_reset_player(game, player, RESET_SOFT, NULL);
            if(err)return err;
        }
    }
    return 0;
}

static int set_players_callback(hexgame_t *game, player_t *player,
    int n_players
){
    test_app_t *app = game->app;
    return test_app_set_players(app, n_players);
}

static int exit_callback(hexgame_t *game, player_t *player){
    test_app_t *app = game->app;
    app->loop = false;
    return 0;
}

static char *generate_respawn_filename(const char *base_name, int i, const char *ext){
    /* Generate a name, e.g. "respawn_1.txt" */

    /* Defaults */
    if(base_name == NULL)base_name = "respawn_";
    if(ext == NULL)ext = ".txt";

    int base_name_len = strlen(base_name);
    int i_len = strlen_of_int(i);
    int ext_len = strlen(ext);
    int filename_len = base_name_len + i_len + ext_len;
    char *filename = malloc(sizeof(*filename) * (filename_len + 1));
    if(filename == NULL)return NULL;
    strcpy(filename, base_name);
    strncpy_of_int(filename + base_name_len, i, i_len);
    strcpy(filename + base_name_len + i_len, ext);
    filename[filename_len] = '\0';
    return filename;
}


/***********
* TEST_APP *
***********/

void test_app_cleanup(test_app_t *app){
    palette_cleanup(&app->palette);
    SDL_FreePalette(app->sdl_palette);
    prismelrenderer_cleanup(&app->prend);
    hexgame_cleanup(&app->hexgame);
    font_cleanup(&app->font);
    sdlfont_cleanup(&app->sdlfont);
}

int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename,
    bool use_textures, bool cache_bitmaps, int n_players
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

    SDL_Palette *sdl_palette = SDL_AllocPalette(256);
    app->sdl_palette = sdl_palette;
    RET_IF_SDL_NULL(sdl_palette);

    palette_t *palette = &app->palette;
    err = palette_load(palette, "data/pal1.fus");
    if(err)return err;

    if(use_textures){
        app->surface = NULL;
    }else{
        app->surface = surface8_create(scw, sch, false, false,
            app->sdl_palette);
        if(app->surface == NULL)return 1;
    }

    prismelrenderer_t *prend = &app->prend;
    err = prismelrenderer_init(prend, &vec4);
    if(err)return err;
    err = prismelrenderer_load(prend, app->prend_filename);
    if(err)return err;
    if(prend->rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", app->prend_filename);
        return 2;}
    prend->cache_bitmaps = cache_bitmaps;

    err = font_load(&app->font, strdup("data/font.fus"));
    if(err)return err;

    err = sdlfont_init(&app->sdlfont, &app->font, sdl_palette);
    if(err)return err;
    app->sdlfont.autoupper = true;

    const char *geomfont_name = "geomfont1";
    app->geomfont = prismelrenderer_get_geomfont(prend, geomfont_name);
    if(app->geomfont == NULL){
        fprintf(stderr, "Couldn't find geomfont: %s\n", geomfont_name);
        return 2;
    }

    err = console_init(&app->console, 80, 40, 20000);
    if(err)return err;

    hexgame_t *game = &app->hexgame;
    err = hexgame_init(game, prend, app->hexmap_filename, app,
        &new_game_callback,
        &continue_callback,
        &set_players_callback,
        &exit_callback);
    if(err)return err;

    hexmap_t *map = game->maps[0];
    vecspace_t *space = map->space;

    for(int i = 0; i < n_players; i++){
        char *respawn_filename = generate_respawn_filename(
            NULL, i, NULL);
        if(respawn_filename == NULL)return 1;

        /* Why the strdup, again?.. who ends up owning it?..
        probably player */
        char *respawn_map_filename = strdup(map->name);

        ARRAY_PUSH_NEW(player_t*, game->players, player)

        err = player_init(player, game, i,
            map->spawn, 0, false,
            respawn_map_filename, respawn_filename);
        if(err)return err;
    }

    app->cur_rgraph_i = 0;
    app->x0 = 0;
    app->y0 = 0;
    app->rot = 0;
    app->flip = false;
    app->zoom = 1;
    app->frame_i = 0;
    app->loop = true;
    app->hexgame_running = true;
    app->show_controls = true;

    test_app_init_input(app);

    app->render_surface = surface32_create(app->scw, app->sch,
        false, true);
    if(app->render_surface == NULL)return 2;

    /* Player 0 gets a body right off the bat, everyone else has to
    wait for him to choose multiplayer mode.
    (See set_players_callback) */
    err = test_app_set_players(app, 1);
    if(err)return err;

    {
        /* Find first body for player 0 (going to point camera at it) */
        body_t *body = NULL;
        for(int i = 0; i < game->players_len; i++){
            player_t *player = game->players[i];
            if(player->keymap != 0)continue;
            body = player->body;
            break;
        }

        /* Create camera */
        ARRAY_PUSH_NEW(camera_t*, game->cameras, camera)
        err = camera_init(camera, game, map, body);
        if(err)return err;

        app->camera = camera;
    }

    return 0;
}


int test_app_set_players(test_app_t *app, int n_players){
    /* Specifically, sets up n_players players with bodies */
    int err;
    hexgame_t *game = &app->hexgame;
    for(int i = 0; i < n_players; i++){
        player_t *player = game->players[i];
        if(player->body != NULL)continue;

        hexmap_t *respawn_map;
        err = hexgame_get_or_load_map(player->game,
            player->respawn_location.map_filename, &respawn_map);
        if(err)return err;

        ARRAY_PUSH_NEW(body_t*, respawn_map->bodies, body)
        err = body_init(body, player->game, respawn_map,
            strdup(app->stateset_filename), NULL, NULL);
        if(err)return err;

        /* Attach body to player */
        player->body = body;

        /* Move body to the respawn location */
        err = body_respawn(body,
            player->respawn_location.pos, player->respawn_location.rot,
            player->respawn_location.turn, respawn_map);
        if(err)return err;
    }
    for(int i = n_players; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->body == NULL)continue;
        err = body_remove(player->body);
        if(err)return err;
        player->body = NULL;
    }
    return 0;
}


int test_app_process_console_input(test_app_t *app){
    int err;

    rendergraph_t *rgraph =
        app->prend.rendergraphs[app->cur_rgraph_i];

    fus_lexer_t _lexer;
    fus_lexer_t *lexer = &_lexer;
    err = fus_lexer_init(lexer, app->console.input, "<console input>");
    if(err)goto lexer_err;

    if(fus_lexer_got(lexer, "exit")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        app->loop = false;
    }else if(fus_lexer_got(lexer, "cls")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        console_clear(&app->console);
    }else if(fus_lexer_got(lexer, "run")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        console_write_msg(&app->console, "Try F5\n");
        return 0;
    }else if(fus_lexer_got(lexer, "add_player")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        char *stateset_filename;

        if(!fus_lexer_done(lexer)){
            err = fus_lexer_get_str(lexer, &stateset_filename);
            if(err)goto lexer_err;
        }else{
            stateset_filename = strdup(app->stateset_filename);
        }

        hexgame_t *game = &app->hexgame;
        /* HACK: we just grab maps[0] */
        hexmap_t *map = game->maps[0];
        vec_ptr_t respawn_pos = map->spawn;
        rot_t respawn_rot = 0;
        bool respawn_turn = false;
        char *respawn_map_filename = NULL;
        if(game->players_len > 0){
            /* HACK: we just grab players[0] */
            player_t *player = game->players[0];
            respawn_pos = player->respawn_location.pos;
            respawn_rot = player->respawn_location.rot;
            respawn_turn = player->respawn_location.turn;
            respawn_map_filename = strdup(player->respawn_location.map_filename);
        }else{
            respawn_map_filename = strdup(map->name);
        }

        hexmap_t *respawn_map;
        err = hexgame_get_or_load_map(game, respawn_map_filename,
            &respawn_map);
        if(err)return err;

        int keymap = -1;
        for(int i = 0; i < game->players_len; i++){
            player_t *player = game->players[i];
            if(player->keymap > keymap)keymap = player->keymap;
        }
        keymap++;

        ARRAY_PUSH_NEW(body_t*, respawn_map->bodies, body)
        err = body_init(body, game, respawn_map, stateset_filename,
            NULL, NULL);
        if(err)return err;

        ARRAY_PUSH_NEW(player_t*, game->players, player)
        err = player_init(player, game, keymap,
            respawn_pos, respawn_rot, respawn_turn, respawn_map_filename,
            NULL);
        if(err)return err;

        /* Attach body to player */
        player->body = body;

        /* Move body to the respawn location */
        err = body_respawn(body, respawn_pos, respawn_rot, respawn_turn, map);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "save")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        char *filename = NULL;

        if(!fus_lexer_done(lexer)){
            err = fus_lexer_get_str(lexer, &filename);
            if(err)goto lexer_err;
        }

        if(filename == NULL){
            err = prismelrenderer_write(&app->prend, stdout);
            if(err)return err;
        }else{
            err = prismelrenderer_save(&app->prend, filename);
            if(err)return err;
        }
    }else if(fus_lexer_got(lexer, "dump")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        int dump_bitmaps = 1;
        int dump_what = 0; /* rgraph, prend */
        while(1){
            if(fus_lexer_done(lexer))break;
            else if(fus_lexer_got(lexer, "rgraph"))dump_what = 0;
            else if(fus_lexer_got(lexer, "prend"))dump_what = 1;
            else if(fus_lexer_got(lexer, "nobitmaps"))dump_bitmaps = 0;
            else if(fus_lexer_got(lexer, "surfaces")){
                /* WARNING: doing this with "prend" after "renderall" causes
                my laptop to hang... */
                dump_bitmaps = 2;}
            else {
                console_write_msg(&app->console, "Dumper says: idunno\n");
                dump_what = -1; break;}
            err = fus_lexer_next(lexer);
            if(err)return err;
        }
        if(dump_what == 0){
            rendergraph_dump(rgraph, stdout, 0, dump_bitmaps);
        }else if(dump_what == 1){
            prismelrenderer_dump(&app->prend, stdout, dump_bitmaps);
        }
    }else if(fus_lexer_got(lexer, "map")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        char *mapper_name;
        char *mapped_rgraph_name;
        char *resulting_rgraph_name = NULL;

        err = fus_lexer_get_str(lexer, &mapper_name);
        if(err)return err;
        err = fus_lexer_get_str(lexer, &mapped_rgraph_name);
        if(err)return err;
        if(!fus_lexer_done(lexer)){
            err = fus_lexer_get_str(lexer, &resulting_rgraph_name);
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
            NULL, &rgraph);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "renderall")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        SDL_Renderer *renderer = NULL;
        err = prismelrenderer_render_all_bitmaps(
            &app->prend, app->sdl_palette);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "get_shape")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        char *name;
        err = fus_lexer_get_str(lexer, &name);
        if(err)return err;
        bool found = false;
        for(int i = 0; i < app->prend.rendergraphs_len; i++){
            rendergraph_t *rgraph = app->prend.rendergraphs[i];
            if(!strcmp(rgraph->name, name)){
                app->cur_rgraph_i = i;
                found = true;
                break;
            }
        }
        if(!found){
            fprintf(stderr, "Couldn't find shape: %s\n", name);
            return 2;}
        return 0;
    }else{
        fus_lexer_unexpected(lexer, NULL);
        console_write_msg(&app->console, "Sorry, what?\n");
        return 0;
    }

    console_write_msg(&app->console, "OK\n");
    return 0;
lexer_err:
    console_write_msg(&app->console, "That didn't work\n");
    return 0;
}


int test_app_mainloop(test_app_t *app){
    SDL_StartTextInput();
    while(app->loop){
        int err = test_app_mainloop_step(app);
        if(err)return err;
    }
    SDL_StopTextInput();
    return 0;
}

int test_app_mainloop_step(test_app_t *app){
        int err;
        Uint32 tick0 = SDL_GetTicks();

        hexgame_t *game = &app->hexgame;

        rendergraph_t *rgraph =
            app->prend.rendergraphs[app->cur_rgraph_i];
        int animated_frame_i = get_animated_frame_i(
            rgraph->animation_type, rgraph->n_frames, app->frame_i);

        err = palette_update_sdl_palette(&app->palette, app->sdl_palette);
        if(err)return err;
        err = palette_step(&app->palette);
        if(err)return err;

        if(app->hexgame_running){
            err = hexgame_step(game);
            if(err)return err;

            if(app->surface != NULL){
                RET_IF_SDL_NZ(SDL_FillRect(app->surface, NULL, 255));
            }else{
                SDL_Color *bgcolor = &app->sdl_palette->colors[255];
                RET_IF_SDL_NZ(SDL_SetRenderDrawColor(app->renderer,
                    bgcolor->r, bgcolor->g, bgcolor->b, 255));
                RET_IF_SDL_NZ(SDL_RenderClear(app->renderer));
            }

            err = camera_render(app->camera,
                app->renderer, app->surface,
                app->sdl_palette, app->scw/2 + app->x0, app->sch/2 + app->y0,
                1 /* app->zoom */);
            if(err)return err;

            if(app->surface != NULL){
                int line_y = 0;
                for(int i = 0; i < game->players_len; i++){
                    player_t *player = game->players[i];
                    body_t *body = player->body;
                    if(!body)continue;
                    if(body->dead == BODY_MOSTLY_DEAD){
                        FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y),
                            "You ran into a wall!\n"
                            "Press jump to retry from where you jumped.\n"
                            "Press %i to retry from last save point.\n",
                            i+1);
                        line_y += app->font.char_h * 2;
                    }else if(body->dead == BODY_ALL_DEAD){
                        FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y),
                            "You were crushed!\n"
                            "Press jump or %i to retry from last save point.\n",
                            i+1);
                        line_y += app->font.char_h * 3;
                    }else if(body->out_of_bounds){
                        FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y),
                            "You jumped off the map!\n"
                            "Press jump to retry from where you jumped.\n"
                            "Press %i to retry from last save point.\n",
                            i+1);
                        line_y += app->font.char_h * 3;
                    }
                }
                if(app->show_controls){
                    FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y),
                        "*Controls:\n"
                        "  Left/right  -> Walk\n"
                        "  Up          -> Jump\n"
                        "  Down        -> Crawl\n"
                        "  Spacebar    -> Spit\n"
                        "  Shift       -> Look up\n"
                        "  1           -> Return to checkpoint\n"
                        "  Enter       -> Show/hide this message\n"
                        "  Escape      -> Quit\n"
                    );
                }

                SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
                    app->renderer, app->surface);
                RET_IF_SDL_NULL(render_texture);
                SDL_RenderCopy(app->renderer, render_texture, NULL, NULL);
                SDL_DestroyTexture(render_texture);
            }

            SDL_RenderPresent(app->renderer);
        }else{
            /*****************
             * DEBUG CONSOLE *
             *****************/

            /******************************************************************
            * Clear screen
            */

            RET_IF_SDL_NZ(SDL_FillRect(app->render_surface, NULL, 0));

            if(app->surface != NULL){
                RET_IF_SDL_NZ(SDL_FillRect(app->surface, NULL, 0));
            }else{
                RET_IF_SDL_NZ(SDL_SetRenderDrawColor(app->renderer,
                    0, 0, 0, 255));
                RET_IF_SDL_NZ(SDL_RenderClear(app->renderer));
            }

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

            FONT_PRINTF(FONT_ARGS(app->render_surface, 0, 0),
                "Game running? %c\n"
                "Frame rendered in: %i ms\n"
                "  (Aiming for sub-%i ms)\n"
                "# Textures in use: %i\n"
                "Controls:\n"
                "  up/down - zoom (hold shift for tap mode)\n"
                "  left/right - rotate (hold shift for tap mode)\n"
                "  control + up/down/left/right - pan (hold shift...)\n"
                "  page up/down - cycle through available rendergraphs\n"
                "  0 - reset rotation\n"
                "Currently displaying rendergraphs from file: %s\n"
                "Currently displaying rendergraph %i / %i:\n"
                "  %s\n"
                "  pan=(%i,%i), rot = %i, flip = %c, zoom = %i\n"
                "  frame_i = %i (%i) / %i (%s)",
                app->hexgame_running? 'y': 'n', app->took, app->delay_goal,
                app->prend.n_textures,
                app->prend_filename, app->cur_rgraph_i,
                app->prend.rendergraphs_len, rgraph->name,
                app->x0, app->y0, app->rot, app->flip? 'y': 'n', app->zoom,
                app->frame_i, animated_frame_i,
                rgraph->n_frames, rgraph->animation_type);

            {
                FONT_BLITTER_T blitter;
                FONT_BLITTER_INIT(&blitter, FONT_ARGS(app->render_surface,
                    0, 20 * app->font.char_h));
                console_blit(&app->console, &FONT_BLITTER_PUTC_CALLBACK,
                    &blitter);
            }

            /******************************************************************
            * Draw to renderer and present it
            */

            if(app->surface != NULL){
                SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
                    app->renderer, app->surface);
                RET_IF_SDL_NULL(render_texture);
                SDL_RenderCopy(app->renderer, render_texture, NULL, NULL);
                SDL_DestroyTexture(render_texture);
            }

            {
                SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
                    app->renderer, app->render_surface);
                RET_IF_SDL_NULL(render_texture);
                SDL_RenderCopy(app->renderer, render_texture, NULL, NULL);
                SDL_DestroyTexture(render_texture);
            }

            SDL_RenderPresent(app->renderer);

        }

        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_QUIT){
                app->loop = false; break;}

            if(event.type == SDL_KEYDOWN){
                if(event.key.keysym.sym == SDLK_ESCAPE){
                    app->loop = false; break;
                }else if(event.key.keysym.sym == SDLK_RETURN){
                    app->show_controls = !app->show_controls;
                }else if(event.key.keysym.sym == SDLK_F5){
                    if(app->hexgame_running){
                        app->hexgame_running = false;
                        console_write_msg(&app->console, "Game stopped\n");
                        SDL_StartTextInput();
                    }else{
                        app->hexgame_running = true;
                        test_app_init_input(app);
                        console_write_msg(&app->console, "Game started\n");
                        SDL_StopTextInput();
                    }
                    continue;
                }else if(event.key.keysym.sym == SDLK_F6){
                    app->camera->zoomout = true;
                }else if(event.key.keysym.sym == SDLK_F7){
                    app->camera->follow = !app->camera->follow;
                }else if(event.key.keysym.sym == SDLK_F8){
                    app->camera->smooth_scroll = !app->camera->smooth_scroll;
                }else if(event.key.keysym.sym == SDLK_F11){
                    printf("Frame rendered in: %i ms\n", app->took);
                    printf("  (Aiming for sub-%i ms)\n", app->delay_goal);

                    prismelrenderer_dump_stats(&app->prend, stdout);
                }
            }else if(event.type == SDL_KEYUP){
                if(event.key.keysym.sym == SDLK_F6){
                    app->camera->zoomout = false;
                }
            }

            if(app->hexgame_running){
                err = hexgame_process_event(game, &event);
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
        IF_APP_KEY(u, if(app->keydown_ctrl){app->y0 += 6;}else if(app->zoom < MAX_ZOOM){app->zoom += 1;})
        IF_APP_KEY(d, if(app->keydown_ctrl){app->y0 -= 6;}else if(app->zoom > 1){app->zoom -= 1;})
        #undef IF_APP_KEY

        Uint32 tick1 = SDL_GetTicks();
        app->took = tick1 - tick0;
        if(app->took < app->delay_goal)SDL_Delay(app->delay_goal - app->took);
#ifdef GEOM_HEXGAME_DEBUG_FRAMERATE
        if(app->took > app->delay_goal){
            fprintf(stderr, "WARNING: Frame rendered in %i ms "
                "(aiming for sub-%i ms)\n",
                app->took, app->delay_goal);
        }
#endif

        return 0;
}

