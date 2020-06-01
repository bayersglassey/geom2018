
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



/*******************
* STATIC UTILITIES *
*******************/

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
    if(app->list){
        test_app_list_cleanup(app->list);
        free(app->list);
    }
}


void test_app_init_input(test_app_t *app){
    app->keydown_shift = false;
    app->keydown_ctrl = false;
    app->keydown_u = 0;
    app->keydown_d = 0;
    app->keydown_l = 0;
    app->keydown_r = 0;
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
        &test_app_new_game_callback,
        &test_app_continue_callback,
        &test_app_set_players_callback,
        &test_app_exit_callback);
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
    app->show_console = false;
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


int test_app_mainloop(test_app_t *app){
    SDL_StartTextInput();
    while(app->loop){
        int err = test_app_mainloop_step(app);
        if(err)return err;
    }
    SDL_StopTextInput();
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
                bool mod_ctrl = event->key.keysym.mod & KMOD_CTRL;
                if(mod_ctrl){
                    if(app->show_console){
                        test_app_hide_console(app);
                    }else{
                        test_app_show_console(app);
                        app->hexgame_running = false;
                    }
                }else{
                    app->hexgame_running = !app->hexgame_running;
                    if(app->show_console){
                        if(app->hexgame_running){
                            test_app_stop_console(app);
                            console_newline(&app->console);
                            console_write_line(&app->console, "Game unpaused");
                            console_write_line(&app->console,
                                "(Note: console doesn't accept input while game unpaused)");
                        }else{
                            console_write_line(&app->console, "Game paused");
                            test_app_start_console(app);
                        }
                    }
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
        }else if(app->show_console){
            if(app->list){
                err = test_app_process_event_list(app, event);
                if(err)return err;
            }else{
                err = test_app_process_event_console(app, event);
                if(err)return err;
            }
        }
    }

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
    }

    if(app->list){
        err = test_app_step_list(app);
        if(err)return err;
        err = test_app_render_list(app);
        if(err)return err;
    }

    err = hexgame_step_cameras(game);
    if(err)return err;

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

