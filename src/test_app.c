
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
#include "geomfont.h"
#include "console.h"
#include "util.h"
#include "anim.h"
#include "hexmap.h"
#include "hexgame.h"
#include "hexspace.h"
#include "generic_printf.h"

const char *RECORDING_FILENAME_TEMPLATE = "recs/000.fus";


/*******************
* STATIC UTILITIES *
*******************/

static char *generate_respawn_filename(const char *base_name, int i, const char *ext){
    /* Generate a name, e.g. "saves/1.txt" */

    /* Defaults */
    if(base_name == NULL)base_name = "saves/";
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
    SDL_FreeSurface(app->surface);
    prismelrenderer_cleanup(&app->prend);
    prismelrenderer_cleanup(&app->minimap_prend);
    console_cleanup(&app->console);
    hexgame_cleanup(&app->hexgame);
    font_cleanup(&app->font);
    minieditor_cleanup(&app->editor);
    if(app->list){
        test_app_list_cleanup(app->list);
        free(app->list);
    }
}


int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename,
    const char *submap_filename, bool developer_mode,
    bool minimap_alt, bool cache_bitmaps,
    int n_players, int n_players_playing, bool load_game
){
    int err;

    app->scw = scw;
    app->sch = sch;
    app->delay_goal = delay_goal;
    app->took = 0;
    app->developer_mode = developer_mode;

    app->window = window;
    app->renderer = renderer;
    app->prend_filename = prend_filename;
    app->stateset_filename = stateset_filename;
    app->hexmap_filename = hexmap_filename;
    app->submap_filename = submap_filename;

    strcpy(app->_recording_filename, RECORDING_FILENAME_TEMPLATE);

    SDL_Palette *sdl_palette = SDL_AllocPalette(256);
    app->sdl_palette = sdl_palette;
    RET_IF_SDL_NULL(sdl_palette);

    palette_t *palette = &app->palette;
    err = palette_load(palette, "data/pal1.fus", NULL);
    if(err)return err;

    app->surface = surface8_create(scw, sch, false, false,
        app->sdl_palette);
    if(app->surface == NULL)return 1;

    prismelrenderer_t *prend = &app->prend;
    err = prismelrenderer_init(prend, &vec4);
    if(err)return err;
    err = prismelrenderer_load(prend, app->prend_filename, NULL);
    if(err)return err;
    if(prend->rendergraphs_len < 1){
        fprintf(stderr, "No rendergraphs in %s\n", app->prend_filename);
        return 2;}
    prend->cache_bitmaps = cache_bitmaps;

    prismelrenderer_t *minimap_prend = &app->minimap_prend;
    err = prismelrenderer_init(minimap_prend,
        minimap_alt? &vec4_alt: &vec4);
    if(err)return err;
    err = prismelrenderer_load(minimap_prend,
        minimap_alt? "data/minimap_alt.fus": "data/minimap.fus",
        NULL);
    if(err)return err;

    err = font_load(&app->font, "data/font.fus", NULL);
    if(err)return err;

    const char *geomfont_name = "geomfont1";
    app->geomfont = prismelrenderer_get_geomfont(prend, geomfont_name);
    if(app->geomfont == NULL){
        fprintf(stderr, "Couldn't find geomfont: %s\n", geomfont_name);
        return 2;
    }

    err = console_init(&app->console,
        TEST_APP_CONSOLE_W, TEST_APP_CONSOLE_H, 20000);
    if(err)return err;

    hexgame_t *game = &app->hexgame;
    err = hexgame_init(game, prend,
        "data/worldmaps.fus",
        minimap_prend,
        "data/tileset_minimap.fus",
        app->hexmap_filename, app,
        &test_app_new_game_callback,
        &test_app_continue_callback,
        &test_app_set_players_callback,
        &test_app_exit_callback);
    if(err)return err;

    hexmap_t *map = game->maps[0];
    vecspace_t *space = map->space;

    for(int i = 0; i < n_players; i++){
        char *_respawn_filename = generate_respawn_filename(
            NULL, i, NULL);
        if(!_respawn_filename)return 1;
        const char *respawn_filename = stringstore_get_donate(
            &prend->filename_store, _respawn_filename);
        if(!respawn_filename)return 1;

        ARRAY_PUSH_NEW(player_t*, game->players, player)

        hexgame_location_t spawn = map->spawn;

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

            hexgame_location_t *spawn_submap_spawn =
                &spawn_submap->collmap.spawn;

            spawn = *spawn_submap_spawn;
            vec_add(space->dims, spawn.pos, spawn_submap->pos);
        }

        err = player_init(player, game, i,
            spawn.pos, spawn.rot, spawn.turn,
            map->name, respawn_filename);
        if(err)return err;
    }

    app->loop = true;
    app->hexgame_running = true;
    app->show_console = false;
    app->process_console = false;
    app->mode = TEST_APP_MODE_GAME;

    /* Player 0 gets a body right off the bat, everyone else has to
    wait for him to choose multiplayer mode.
    (See set_players_callback) */
    err = test_app_set_players(app, n_players_playing);
    if(err)return err;

    /* Find player0 */
    player_t *player0 = NULL;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->keymap != 0)continue;
        player0 = player;
        break;
    }
    if(player0 == NULL){
        fprintf(stderr, "Couldn't find player 0\n");
        return 2;
    }

    {
        /* Create camera */
        ARRAY_PUSH_NEW(camera_t*, game->cameras, camera)
        err = camera_init(camera, game, map, player0->body);
        if(err)return err;

        app->camera = camera;
    }

    app->camera_mapper = NULL;

    minieditor_init(&app->editor,
        app->surface, app->sdl_palette,
        app->prend_filename,
        &app->font, app->geomfont, &app->prend,
        app->delay_goal, app->scw, app->sch);

    app->list = NULL;

    if(load_game){
        /* Load game immediately, instead of starting at title screen and
        having to jump into the "LOAD" door or whatever. */
        err = test_app_continue_callback(game, player0);
        if(err)return err;
    }

    return 0;
}


int test_app_mainloop(test_app_t *app){
    while(app->loop){
        Uint32 tick0 = SDL_GetTicks();

        int err = test_app_mainloop_step(app);
        if(err)return err;

        Uint32 tick1 = SDL_GetTicks();
        app->took = tick1 - tick0;
        if(app->took < app->delay_goal){
            SDL_Delay(app->delay_goal - app->took);
        }
    }
    return 0;
}

static int test_app_render(test_app_t *app){
    int err;

    /* We haven't printed any lines so far this frame... */
    app->lines_printed = 0;

    if(app->mode == TEST_APP_MODE_GAME){
        err = test_app_render_game(app);
        if(err)return err;
    }else if(app->mode == TEST_APP_MODE_EDITOR){
        err = test_app_render_editor(app);
        if(err)return err;
    }else{
        fprintf(stderr, "%s: Unrecognized app mode: %i\n",
            __func__, app->mode);
        return 2;
    }

    if(app->developer_mode){
        /* Dump some info to bottom of screen */

        int geomfont_prismel_height = 2; /* Hardcoded, actually depends on the geomfont */
        int bottom_of_screen_in_prismels = app->sch / geomfont_prismel_height;

        if(app->took > app->delay_goal && app->developer_mode){
            test_app_printf(app, 0,
                bottom_of_screen_in_prismels - 2 * app->font.char_h,
                "Time to render (in ticks): goal=%i, actual=%i",
                app->delay_goal, app->took);
        }
    }

    SDL_Texture *render_texture = SDL_CreateTextureFromSurface(
        app->renderer, app->surface);
    RET_IF_SDL_NULL(render_texture);
    SDL_RenderCopy(app->renderer, render_texture, NULL, NULL);
    SDL_DestroyTexture(render_texture);

    SDL_RenderPresent(app->renderer);
    return 0;
}

static int test_app_poll_events(test_app_t *app){
    int err;

    hexgame_t *game = &app->hexgame;

    bool dont_process_console_this_frame = false;

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
            }else if(event->key.keysym.sym == SDLK_F5 && app->developer_mode){
                app->hexgame_running = !app->hexgame_running;
            }else if(event->key.keysym.sym == SDLK_BACKQUOTE && app->developer_mode){
                dont_process_console_this_frame = true;
                if(event->key.keysym.mod & KMOD_CTRL){
                    if(app->show_console){
                        if(!app->process_console){
                            console_write_line(&app->console, "Console started");
                            test_app_start_console(app);
                        }else{
                            test_app_stop_console(app);
                            console_newline(&app->console);
                            console_write_line(&app->console, "Console stopped");
                        }
                    }
                }else{
                    if(app->show_console)test_app_hide_console(app);
                    else test_app_show_console(app);
                }
            }else if(event->key.keysym.sym == SDLK_F11 && app->developer_mode){
                printf("Frame rendered in: %i ms\n", app->took);
                printf("  (Aiming for sub-%i ms)\n", app->delay_goal);

                prismelrenderer_dump_stats(&app->prend, stdout);

#ifdef GEOM_DEBUG_MALLOC
                malloc_stats();
#endif
            }
        }

        if(app->process_console){
            if(app->list){
                err = test_app_process_event_list(app, event);
                if(err)return err;
            }else if(!dont_process_console_this_frame){
                err = test_app_process_event_console(app, event);
                if(err)return err;
            }
            continue;
        }

        if(app->mode == TEST_APP_MODE_GAME){
            err = test_app_process_event_game(app, event);
            if(err)return err;
        }else if(app->mode == TEST_APP_MODE_EDITOR){
            err = test_app_process_event_editor(app, event);
            if(err)return err;
        }

        err = hexgame_process_event(game, event);
        if(err)return err;
    }

    return 0;
}

int test_app_mainloop_step(test_app_t *app){
    int err;

    hexgame_t *game = &app->hexgame;

    err = palette_update_sdl_palette(&app->palette, app->sdl_palette);
    if(err)return err;
    err = palette_step(&app->palette);
    if(err)return err;

    /* Consume events right before using them in hexgame_step
    (so e.g. we have most up-to-date information about which keys
    were pressed) */
    err = test_app_poll_events(app);
    if(err)return err;

    if(app->hexgame_running){
        err = hexgame_step(game);
        if(err)return err;
    }

    err = hexgame_step_cameras(game);
    if(err)return err;

    err = test_app_render(app);
    if(err)return err;

    if(app->list){
        err = test_app_step_list(app);
        if(err)return err;

        err = test_app_render_list(app);
        if(err)return err;
    }

    return 0;
}

static const char *test_app_get_recording_filename(
    test_app_t *app, int n
){
    char *recording_filename = app->_recording_filename;
    int zeros_pos = strchr(RECORDING_FILENAME_TEMPLATE, '0') - RECORDING_FILENAME_TEMPLATE;
    int n_zeros = strrchr(RECORDING_FILENAME_TEMPLATE, '0') - RECORDING_FILENAME_TEMPLATE - zeros_pos + 1;
    for(int i = 0; i < n_zeros; i++){
        int rem = n % 10;
        n = n / 10;
        recording_filename[zeros_pos + n_zeros - 1 - i] = '0' + rem;
    }
    return recording_filename;
}

static const char *test_app_get_last_or_next_recording_filename(
    test_app_t *app, bool next
){
    /* This function is... horrific */
    const char *recording_filename;
    int n = 0;
    while(1){
        recording_filename = test_app_get_recording_filename(app, n);
        FILE *f = fopen(recording_filename, "r");
        if(f == NULL)break;
        n++;
    }
    if(!next){
    if(n == 0)return NULL;
        recording_filename = test_app_get_recording_filename(app, n-1);
    }
    return recording_filename;
}

const char *test_app_get_last_recording_filename(test_app_t *app){
    return test_app_get_last_or_next_recording_filename(app, false);
}

const char *test_app_get_next_recording_filename(test_app_t *app){
    return test_app_get_last_or_next_recording_filename(app, true);
}


int test_app_printf(test_app_t *app, int col, int row, const char *msg, ...){
    int err = 0;
    va_list vlist;
    va_start(vlist, msg);

    geomfont_blitter_t blitter;
    geomfont_blitter_render_init(&blitter, app->geomfont,
        app->surface, app->sdl_palette,
        0, 0, col, row, 1, NULL, NULL);
    err = generic_vprintf(&geomfont_blitter_putc_callback, &blitter,
        msg, vlist);

    /* Use the external variable (ooh ganky) generic_printf_lines_counted
    which counts the number of newline characters printed */
    app->lines_printed += generic_printf_lines_counted;

    va_end(vlist);
    return err;
}
