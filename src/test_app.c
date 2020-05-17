
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#ifdef GEOM_DEBUG_MALLOC
    #include <malloc.h>
#endif

#include "test_app.h"
#include "test_app_list.h"
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
    #define CONSOLE_H 20
#else
    #define FONT_BLITTER_T sdlfont_blitter_t
    #define FONT_BLITTER_INIT sdlfont_blitter_init
    #define FONT_BLITTER_PUTC_CALLBACK sdlfont_blitter_putc_callback
    #define FONT_PRINTF sdlfont_printf
    #define FONT_ARGS(SURFACE, X0, Y0) &app->sdlfont, (SURFACE), (X0), (Y0)
    #define CONSOLE_W 80
    #define CONSOLE_H 40
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

static int blit_console(test_app_t *app, SDL_Surface *surface, int x, int y){
    int err;

    FONT_BLITTER_T blitter;
    FONT_BLITTER_INIT(&blitter, FONT_ARGS(surface, x, y));
    err = console_blit(&app->console, &FONT_BLITTER_PUTC_CALLBACK,
        &blitter);
    if(err)return err;

    return 0;
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
    const char *submap_filename, bool use_textures,
    bool cache_bitmaps, int n_players
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
    app->submap_filename = submap_filename;

    SDL_Palette *sdl_palette = SDL_AllocPalette(256);
    app->sdl_palette = sdl_palette;
    RET_IF_SDL_NULL(sdl_palette);

    palette_t *palette = &app->palette;
    err = palette_load(palette, "data/pal1.fus", NULL);
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
    err = prismelrenderer_load(prend, app->prend_filename, NULL);
    if(err)return err;
    if(prend->rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", app->prend_filename);
        return 2;}
    prend->cache_bitmaps = cache_bitmaps;

    err = font_load(&app->font, strdup("data/font.fus"), NULL);
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

    err = console_init(&app->console, CONSOLE_W, CONSOLE_H, 20000);
    if(err)return err;

    hexgame_t *game = &app->hexgame;
    err = hexgame_init(game, prend,
        app->hexmap_filename, app,
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

        vec_t *respawn_pos = &map->spawn;

        /* Maybe get respawn_pos from a submap */
        const char *submap_filename = app->submap_filename;
        if(submap_filename != NULL){
            hexmap_submap_t *spawn_submap = NULL;
            for(int i = 0; i < map->submaps_len; i++){
                hexmap_submap_t *submap = map->submaps[i];
                if(strcmp(submap->filename, submap_filename) == 0){
                    spawn_submap = submap; break;}
            }
            if(spawn_submap == NULL){
            fprintf(stderr, "Couldn't find submap with filename: %s\n",
                    submap_filename);
                return 2;}
            respawn_pos = &spawn_submap->pos;
        }

        err = player_init(player, game, i,
            *respawn_pos, 0, false,
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
    app->mode = TEST_APP_MODE_GAME;

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

    app->camera_mapper = NULL;

    app->list = NULL;

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


int test_app_mainloop(test_app_t *app){
    SDL_StartTextInput();
    while(app->loop){
        int err = test_app_mainloop_step(app);
        if(err)return err;
    }
    SDL_StopTextInput();
    return 0;
}

static int test_app_render_game(test_app_t *app){
    int err;

    hexgame_t *game = &app->hexgame;

    if(app->surface != NULL){
        RET_IF_SDL_NZ(SDL_FillRect(app->surface, NULL, 255));
    }else{
        SDL_Color *bgcolor = &app->sdl_palette->colors[255];
        RET_IF_SDL_NZ(SDL_SetRenderDrawColor(app->renderer,
            bgcolor->r, bgcolor->g, bgcolor->b, 255));
        RET_IF_SDL_NZ(SDL_RenderClear(app->renderer));
    }

    if(app->camera_mapper){
        /* camera->mapper is set to NULL at start of each step, it's up
        to us to set it if desired before calling camera_render */
        app->camera->mapper = app->camera_mapper;
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
                FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y * app->font.char_h),
                    "You ran into a wall!\n"
                    "Press jump to retry from where you jumped.\n"
                    "Press %i to retry from last save point.\n",
                    i+1);
                line_y += 3;
            }else if(body->dead == BODY_ALL_DEAD){
                FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y * app->font.char_h),
                    "You were crushed!\n"
                    "Press jump or %i to retry from last save point.\n",
                    i+1);
                line_y += 2;
            }else if(body->out_of_bounds && !body->state->flying){
                FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y * app->font.char_h),
                    "You jumped off the map!\n"
                    "Press jump to retry from where you jumped.\n"
                    "Press %i to retry from last save point.\n",
                    i+1);
                line_y += 3;
            }
        }

        if(app->hexgame_running && app->show_controls){
            FONT_PRINTF(FONT_ARGS(app->surface, 0, line_y * app->font.char_h),
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
            line_y += 9;
        }

        if(!app->hexgame_running){
            err = blit_console(app, app->surface, 0, line_y * app->font.char_h);
            if(err)return err;
        }

        SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
            app->renderer, app->surface);
        RET_IF_SDL_NULL(render_texture);
        SDL_RenderCopy(app->renderer, render_texture, NULL, NULL);
        SDL_DestroyTexture(render_texture);
    }

    SDL_RenderPresent(app->renderer);
    return 0;
}

static int test_app_render_editor(test_app_t *app){
    int err;

    rendergraph_t *rgraph =
        app->prend.rendergraphs[app->cur_rgraph_i];
    int animated_frame_i = get_animated_frame_i(
        rgraph->animation_type, rgraph->n_frames, app->frame_i);

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

    int line_y = 0;

    if(app->show_controls){
        FONT_PRINTF(FONT_ARGS(app->render_surface, 0, line_y * app->font.char_h),
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

        line_y += 15;
    }

    err = blit_console(app, app->render_surface, 0, line_y * app->font.char_h);
    if(err)return 2;

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
    return 0;
}

static int test_app_render(test_app_t *app){
    if(app->mode == TEST_APP_MODE_GAME){
        return test_app_render_game(app);
    }else if(app->mode == TEST_APP_MODE_EDITOR){
        return test_app_render_editor(app);
    }else{
        fprintf(stderr, "%s: Unrecognized app mode: %i\n",
            __func__, app->mode);
        return 2;
    }
}

static int test_app_process_event_game(test_app_t *app, SDL_Event *event){
    int err;

    hexgame_t *game = &app->hexgame;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_RETURN){
            if(app->hexgame_running){
                app->show_controls = !app->show_controls;
            }
        }else if(event->key.keysym.sym == SDLK_F6){
            /* Hack, we really want to force camera->mapper to NULL, but
            instead we assume the existence of this mapper called "single" */
            const char *mapper_name = "single";
            app->camera_mapper = prismelrenderer_get_mapper(&app->prend, mapper_name);
            if(app->camera_mapper == NULL){
                fprintf(stderr, "%s: Couldn't find mapper: %s\n",
                    __func__, mapper_name);
                return 2;
            }
        }else if(event->key.keysym.sym == SDLK_F7){
            app->camera->follow = !app->camera->follow;
        }else if(event->key.keysym.sym == SDLK_F8){
            app->camera->smooth_scroll = !app->camera->smooth_scroll;
        }else if(event->key.keysym.sym == SDLK_F9){
            /* save recording */
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap != 0)continue;

                body_t *body = player->body;
                if(!body){
                    fprintf(stderr,
                        "Can't record without a body!\n");
                }else if(body->recording.action != 2){
                    const char *recording_filename = get_next_recording_filename();
                    fprintf(stderr, "Recording to file: %s "
                        " (When finished, press F9 to save!)\n",
                        recording_filename);
                    err = body_start_recording(body, strdup(recording_filename));
                    if(err)return err;
                }else{
                    fprintf(stderr, "Finished recording. "
                        "Press F10 to play it back.\n");
                    err = body_stop_recording(body);
                    if(err)return err;
                }
            }
        }else if(event->key.keysym.sym == SDLK_F10){
            /* load recording */
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            const char *recording_filename = get_last_recording_filename();
            if(recording_filename == NULL){
                fprintf(stderr, "Couldn't find file of last recording. "
                    "Maybe you need to record your first one with F9?\n");
            }else{
                fprintf(stderr, "Playing back from file: %s\n",
                    recording_filename);
                for(int i = 0; i < game->players_len; i++){
                    player_t *player = game->players[i];
                    if(player->keymap != 0)continue;

                    body_t *body = player->body;
                    if(!body){
                        fprintf(stderr,
                            "Can't play back recording without a body!\n");
                    }else if(shift){
                        err = body_load_recording(body, recording_filename,
                            true);
                        if(err)return err;
                        err = body_play_recording(body);
                        if(err)return err;
                    }else{
                        /* TODO: Recordings need to state which map they
                        expect! The following is a hack: you must play
                        recordings belonging to your correct map... */
                        err = hexmap_load_recording(body->map,
                            recording_filename, NULL, true, 0, NULL);
                        if(err)return err;
                    }
                }
            }
        }else if(event->key.keysym.sym == SDLK_F12){
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap < 0)continue;
                hexgame_player_dump(player, 0);
            }
        }else if(!event->key.repeat){
            int keymap = -1;
            if(event->key.keysym.sym == SDLK_1)keymap = 0;
            if(event->key.keysym.sym == SDLK_2)keymap = 1;
            if(keymap > -1){
                err = hexgame_reset_player_by_keymap(game, keymap,
                    RESET_SOFT, NULL);
                if(err)return err;
            }
        }
    }else if(event->type == SDL_KEYUP){
        if(event->key.keysym.sym == SDLK_F6){
            app->camera_mapper = NULL;
        }
    }
    return 0;
}

static int test_app_process_event_editor(test_app_t *app, SDL_Event *event){
    int err;
    switch(event->type){
        case SDL_KEYDOWN: {

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
    return 0;
}

static int test_app_process_event_console(test_app_t *app, SDL_Event *event){
    int err;
    switch(event->type){
        case SDL_KEYDOWN: {

            /* Enter a line of console input */
            if(event->key.keysym.sym == SDLK_RETURN){
                console_newline(&app->console);

                err = test_app_process_console_input(app);
                if(err)return err;

                console_input_clear(&app->console);
                console_write_msg(&app->console, CONSOLE_START_TEXT);
            }

            /* Tab completion */
            if(event->key.keysym.sym == SDLK_TAB){
                console_newline(&app->console);
                test_app_write_console_commands(app, app->console.input);
                console_write_msg(&app->console, CONSOLE_START_TEXT);
                console_write_msg(&app->console, app->console.input);
            }

            /* Copy/paste a line of console input */
            if(
                event->key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)
                && event->key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT)
            ){
                if(event->key.keysym.sym == SDLK_c){
                    SDL_SetClipboardText(app->console.input);}
                if(event->key.keysym.sym == SDLK_v
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

            if(event->key.keysym.sym == SDLK_BACKSPACE){
                console_input_backspace(&app->console);}
            if(event->key.keysym.sym == SDLK_DELETE){
                console_input_delete(&app->console);}

        } break;
        case SDL_TEXTINPUT: {
            for(char *c = event->text.text; *c != '\0'; c++){
                console_input_char(&app->console, *c);
            }
        } break;
        default: break;
    }
    return 0;
}

static int test_app_process_event_list(test_app_t *app, SDL_Event *event){
    int err;

    /* Guaranteed to be non-NULL if we make it into this function: */
    test_app_list_t *list = app->list;

    switch(event->type){
        case SDL_KEYDOWN: {
            switch(event->key.keysym.sym){

                /* For now, we're lazy: we don't check indexes against length, so the
                callbacks need to handle wraparound themselves */
                case SDLK_UP: {
                    list->index_y--;
                } break;
                case SDLK_DOWN: {
                    list->index_y++;
                } break;
                case SDLK_LEFT: {
                    list->index_x--;
                } break;
                case SDLK_RIGHT: {
                    list->index_x++;
                } break;

                case SDLK_RETURN: {
                    if(list->select_item){
                        err = list->select_item(list);
                        if(err)return err;
                    }
                } break;

                case SDLK_BACKSPACE: {
                    if(list->back){
                        err = list->back(list);
                        if(err)return err;
                    }else{
                        /* Default "back" action if no callback */
                        err = test_app_close_list(app);
                        if(err)return err;
                    }
                } break;

                default: break;
            }
        } break;
        default: break;
    }
    return 0;
}

static int test_app_poll_events(test_app_t *app){
    int err;

    hexgame_t *game = &app->hexgame;

    SDL_Event _event, *event = &_event;
    while(SDL_PollEvent(event)){

        if(event->type == SDL_QUIT){
            app->loop = false;
            break;
        }

        if(event->type == SDL_KEYDOWN){
            if(event->key.keysym.sym == SDLK_ESCAPE){
                app->loop = false;
                break;
            }else if(event->key.keysym.sym == SDLK_F5){
                if(app->hexgame_running){
                    app->hexgame_running = false;
                    console_write_msg(&app->console, "Game stopped\n");
                    console_write_msg(&app->console, CONSOLE_START_TEXT);
                    SDL_StartTextInput();
                }else{
                    app->hexgame_running = true;
                    app->mode = TEST_APP_MODE_GAME;
                    test_app_init_input(app);
                    console_write_msg(&app->console, "Game started\n");
                    SDL_StopTextInput();
                }
                continue;
            }else if(event->key.keysym.sym == SDLK_F11){
                printf("Frame rendered in: %i ms\n", app->took);
                printf("  (Aiming for sub-%i ms)\n", app->delay_goal);

                prismelrenderer_dump_stats(&app->prend, stdout);

#ifdef GEOM_DEBUG_MALLOC
                malloc_stats();
#endif
            }
        }

        if(app->mode == TEST_APP_MODE_GAME){
            err = test_app_process_event_game(app, event);
            if(err)return err;
        }else if(app->mode == TEST_APP_MODE_EDITOR){
            err = test_app_process_event_editor(app, event);
            if(err)return err;

            #define IF_APP_KEY(KEY, BODY) \
                if(app->keydown_##KEY >= (app->keydown_shift? 2: 1)){ \
                    app->keydown_##KEY = 1; \
                    BODY}
            IF_APP_KEY(l, if(app->keydown_ctrl){app->x0 += 6;}else{app->rot += 1;})
            IF_APP_KEY(r, if(app->keydown_ctrl){app->x0 -= 6;}else{app->rot -= 1;})
            IF_APP_KEY(u, if(app->keydown_ctrl){app->y0 += 6;}else if(app->zoom < MAX_ZOOM){app->zoom += 1;})
            IF_APP_KEY(d, if(app->keydown_ctrl){app->y0 -= 6;}else if(app->zoom > 1){app->zoom -= 1;})
            #undef IF_APP_KEY
        }

        if(app->hexgame_running){
            err = hexgame_process_event(game, event);
            if(err)return err;
        }else if(app->list){
            err = test_app_process_event_list(app, event);
            if(err)return err;
        }else{
            err = test_app_process_event_console(app, event);
            if(err)return err;
        }
    }

    return 0;
}

static void _render_list_title(console_t *console, test_app_list_t *list){
    if(list->prev != NULL){
        _render_list_title(console, list->prev);
        console_write_msg(console, " -> ");
    }
    console_write_msg(console, list->title);
}

static int test_app_render_list(test_app_t *app){
    int err;

    console_t *console = &app->console;
    console_clear(console);

    console_write_char(console, '(');
    _render_list_title(console, app->list);
    console_write_char(console, ')');
    console_newline(console);

    err = app->list->render(app->list);
    if(err)return err;

    return 0;
}

int test_app_mainloop_step(test_app_t *app){
    int err;
    Uint32 tick0 = SDL_GetTicks();

    hexgame_t *game = &app->hexgame;

    err = palette_update_sdl_palette(&app->palette, app->sdl_palette);
    if(err)return err;
    err = palette_step(&app->palette);
    if(err)return err;

    if(app->hexgame_running){
        err = hexgame_step(game);
        if(err)return err;
    }else{
        if(app->list){
            err = test_app_render_list(app);
            if(err)return err;
        }
    }

    err = test_app_render(app);
    if(err)return err;

    err = test_app_poll_events(app);
    if(err)return err;

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

int test_app_open_list(test_app_t *app, const char *title,
    int index_x, int index_y,
    void *data,
    test_app_list_callback_t *render,
    test_app_list_callback_t *select_item,
    test_app_list_callback_t *cleanup
){
    test_app_list_t *new_list = malloc(sizeof(*new_list));
    if(new_list == NULL)return 1;
    test_app_list_init(new_list, title, app->list,
        index_x, index_y,
        data, render, select_item, cleanup);
    app->list = new_list;
    return 0;
}

int test_app_close_list(test_app_t *app){
    int err;

    test_app_list_t *prev = app->list->prev;
    app->list->prev = NULL;
        /* Set list->prev to NULL so it's not cleaned up, since we're
        going to use it */
    test_app_list_cleanup(app->list);
    app->list = prev;

    if(!app->list){
        /* App is back in "console mode", so re-render console's input, which
        list's render probably erased */
        console_clear(&app->console);
        console_write_msg(&app->console, CONSOLE_START_TEXT);
        console_write_msg(&app->console, app->console.input);
    }

    return 0;
}
