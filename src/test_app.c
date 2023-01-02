
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
#include "save_slots.h"

const char *RECORDING_FILENAME_TEMPLATE = "recs/000.fus";

/* These are declared as extern in test_app.h */
const char *TEST_MAP_HEXMAP_FILENAME_TITLE = "data/maps/title/worldmap_menu.fus";
const char *TEST_MAP_HEXMAP_FILENAME_NEW_GAME = "data/maps/tutorial/worldmap.fus";


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
    test_app_menu_cleanup(&app->menu);
    if(app->list){
        test_app_list_cleanup(app->list);
        free(app->list);
    }
}

static hexgame_save_callback_t _save_callback;
static int _save_callback(hexgame_t *game){
    test_app_t *app = (test_app_t *)game->save_callback_data;
    const char *filename = get_save_slot_filename(app->save_slot);
    return hexgame_save(game, filename);
}
static int _test_app_restart(test_app_t *app,
    prismelrenderer_t *prend, prismelrenderer_t *minimap_prend,
    const char *hexmap_filename, const char *submap_filename
){
    int err;
    hexgame_t *game = &app->hexgame;

    int n_players = app->n_players;

    bool force_new_game = false;
    if(app->state == TEST_APP_STATE_TITLE_SCREEN){
        hexmap_filename = TEST_MAP_HEXMAP_FILENAME_TITLE;
        n_players = 0;
    }else if(hexmap_filename){
        /* Caller wants to start the game immediately on a specific map */
        force_new_game = true;
    }else{
        /* Default map */
        hexmap_filename = TEST_MAP_HEXMAP_FILENAME_NEW_GAME;
    }

    hexmap_t *map;
    err = hexgame_init(game, prend,
        "data/worldmaps.fus",
        minimap_prend,
        "data/tileset_minimap.fus",
        hexmap_filename, &map,
        &_save_callback, app);
    if(err)return err;
    game->animate_palettes = app->animate_palettes;

    int new_state = TEST_APP_STATE_RUNNING;

    if(
        app->state == TEST_APP_STATE_START_GAME &&
        !force_new_game &&
        get_save_slot_file_exists(app->save_slot)
    ){
        const char *save_filename = get_save_slot_filename(app->save_slot);
        bool bad_version = false;
        err = hexgame_load(&app->hexgame, save_filename, &bad_version);
        if(bad_version){
            test_app_menu_set_screen(&app->menu,
                TEST_APP_MENU_SCREEN_TITLE);
            app->show_menu = true;
            app->menu.message =
                "The save file format has changed.\n"
                "You need to load this save file with the\n"
                "old version of the game.\n"
                "You can also delete the save file and start\n"
                "a new game.";
            new_state = TEST_APP_STATE_TITLE_SCREEN;
        }else if(err)return err;
    }else{
        /* Add players */
        for(int i = 0; i < n_players; i++){
            ARRAY_PUSH_NEW(player_t*, game->players, player)

            hexgame_location_t spawn = map->spawn;

            /* Maybe get respawn_pos from a submap */
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
                vec_add(map->space->dims, spawn.pos, spawn_submap->pos);
            }

            hexgame_savelocation_t respawn_location;
            hexgame_savelocation_set(&respawn_location, map->space,
                spawn.pos, spawn.rot, spawn.turn, map->filename,
                NULL, NULL);

            err = player_init(player, game, i, &respawn_location);
            if(err)return err;
        }

        /* Spawn bodies for all players */
        for(int i = 0; i < game->players_len; i++){
            player_t *player = game->players[i];

            /* Set this here so player_spawn_body will use it below */
            player->respawn_location.stateset_filename =
                app->stateset_filename;

            err = player_spawn_body(player);
            if(err)return err;
        }
    }

    /* Find player0 */
    player_t *player0 = hexgame_get_player_by_keymap(game, 0);

    {
        /* Create camera */
        body_t *body = player0? player0->body: NULL;
        ARRAY_PUSH_NEW(camera_t*, game->cameras, camera)
        err = camera_init(camera, game, map, body);
        if(err)return err;

        app->camera = camera;
    }

    app->state = new_state;
    return 0;
}
static int test_app_restart(test_app_t *app){
    int err;
    hexgame_t *game = &app->hexgame;
    prismelrenderer_t *prend = game->prend;
    prismelrenderer_t *minimap_prend = game->minimap_prend;
    hexgame_cleanup(game);
    return _test_app_restart(app, prend, minimap_prend, NULL, NULL);
}

int test_app_init(test_app_t *app, int scw, int sch, int delay_goal,
    SDL_Window *window, SDL_Renderer *renderer, const char *prend_filename,
    const char *stateset_filename, const char *hexmap_filename,
    const char *submap_filename, bool developer_mode,
    bool minimap_alt, bool cache_bitmaps, bool animate_palettes,
    int n_players, int save_slot,
    const char *load_recording_filename,
    const char *save_recording_filename
){
    int err;

    app->scw = scw;
    app->sch = sch;
    app->delay_goal = delay_goal;
    app->took = 0;
    app->developer_mode = developer_mode;
    app->n_players = n_players;
    app->animate_palettes = animate_palettes;

    app->window = window;
    app->renderer = renderer;
    app->prend_filename = prend_filename;
    app->stateset_filename = stateset_filename;

    strcpy(app->_recording_filename, RECORDING_FILENAME_TEMPLATE);
    app->load_recording_filename = load_recording_filename;
    app->save_recording_filename = save_recording_filename;

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

    app->list = NULL;

    minieditor_init(&app->editor,
        app->surface, app->sdl_palette,
        app->prend_filename,
        &app->font, app->geomfont, &app->prend,
        app->delay_goal, app->scw, app->sch);

    test_app_menu_init(&app->menu, app);

    app->state = TEST_APP_STATE_RUNNING;
    app->save_slot = 0;

    app->hexgame_running = true;
    app->show_console = false;
    app->process_console = false;
    app->mode = TEST_APP_MODE_GAME;
    app->camera_mapper = NULL;

    if(save_slot >= 0){
        /* Load game immediately, for debugging purposes */
        app->state = TEST_APP_STATE_START_GAME;
        app->save_slot = save_slot;
        app->show_menu = false;
    }else if(hexmap_filename){
        /* Load game immediately, for debugging purposes */
        app->state = TEST_APP_STATE_START_GAME;
        app->show_menu = false;
    }else{
        app->state = TEST_APP_STATE_TITLE_SCREEN;
        submap_filename = NULL;
        app->show_menu = true;
    }

    return _test_app_restart(app, prend, minimap_prend,
        hexmap_filename, submap_filename);
}

int test_app_mainloop(test_app_t *app){
    /* Mainloop when running "imperatively" as opposed to using callbacks.
    If callbacks are used (e.g. Emscripten), you should figure out some other
    way to periodically call test_app_mainloop_step. */
    while(app->state != TEST_APP_STATE_QUIT){
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

static int test_app_process_event_menu(test_app_t *app, SDL_Event *event){
    int err;
    test_app_menu_t *menu = &app->menu;
    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_UP){
            test_app_menu_up(menu);
        }else if(event->key.keysym.sym == SDLK_DOWN){
            test_app_menu_down(menu);
        }else if(event->key.keysym.sym == SDLK_LEFT){
            test_app_menu_left(menu);
        }else if(event->key.keysym.sym == SDLK_RIGHT){
            test_app_menu_right(menu);
        }else if(event->key.keysym.sym == SDLK_ESCAPE){
            test_app_menu_back(menu);
        }else if(event->key.keysym.sym == SDLK_RETURN){
            err = test_app_menu_select(menu);
            if(err)return err;
        }
    }
    return 0;
}

static int test_app_poll_events(test_app_t *app){
    int err;

    hexgame_t *game = &app->hexgame;

    bool dont_process_console_this_frame = false;

    SDL_Event _event, *event = &_event;
    while(SDL_PollEvent(event)){

        /* Quit immediately if we're asked to */
        if(event->type == SDL_QUIT){
            app->state = TEST_APP_STATE_QUIT;
            break;
        }

        /* Menu can steal the keyboard
        (*before* checking for console activation/deactivation with
        SDLK_BACKQUOTE...) */
        if(app->show_menu){
            err = test_app_process_event_menu(app, event);
            if(err)return err;
            continue;
        }

        /* Handle activating/deactivating console */
        if(
            app->developer_mode &&
            event->type == SDL_KEYDOWN &&
            (event->key.keysym.sym == SDLK_BACKQUOTE)
        ){
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

            /* We use this to avoid writing a backtick to the console.
            But wait, you say -- can't we just avoid sending this SDL_KEYDOWN
            event to console?..
            In fact, no!.. because the console looks at SDL_TEXTINPUT events
            instead... */
            dont_process_console_this_frame = true;

            continue;
        }

        /* Console can steal the keyboard */
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

        /* Handle various "special keys" valid for game or editor mode
        (if keyboard wasn't stolen by menu/console...) */
        if(event->type == SDL_KEYDOWN){
            if(event->key.keysym.sym == SDLK_F11 && app->developer_mode){
                printf("Frame rendered in: %i ms\n", app->took);
                printf("  (Aiming for sub-%i ms)\n", app->delay_goal);

                prismelrenderer_dump_stats(&app->prend, stdout);

#ifdef GEOM_DEBUG_MALLOC
                malloc_stats();
#endif
            }
        }

        if(app->mode == TEST_APP_MODE_GAME){
            /* Handle special keypresses (F1-F12, etc) */
            err = test_app_process_event_game(app, event);
            if(err)return err;

            /* Handle players' keypresses */
            err = hexgame_process_event(game, event);
            if(err)return err;
        }else if(app->mode == TEST_APP_MODE_EDITOR){
            /* Handle special keypresses (arrow keys, etc) */
            err = test_app_process_event_editor(app, event);
            if(err)return err;
        }
    }

    if(

        /* Don't know if we really care about this one */
        app->mode != TEST_APP_MODE_GAME ||

        app->show_menu ||
        app->hexgame.show_minimap ||
        app->process_console
        /* NOTE: *don't* look at app->hexgame_running for this check -- that's
        the F5 pause, and we want to be able to e.g. observe keyinfo in the
        console while paused. */
    ){
        /* Reset players' keypresses
        (so that e.g. if you're holding right, and then enter the menu,
        then when you exit the menu, your character isn't stuck running
        to the right until you tap right... knowaddamean?) */
        for(int i = 0; i < app->hexgame.players_len; i++){
            player_t *player = app->hexgame.players[i];
            body_t *body = player->body;
            if(!body)continue;
            keyinfo_reset(&body->keyinfo);
        }
    }

    return 0;
}

int test_app_mainloop_step(test_app_t *app){
    int err;

    hexgame_t *game = &app->hexgame;

    while(app->state != TEST_APP_STATE_RUNNING){
        if(app->state == TEST_APP_STATE_QUIT){
            /* This shouldn't really happen; app's mainloop step shouldn't
            even be called if app is quitting */
            fprintf(stderr, "ERROR: app state was QUIT at entry to step\n");
            return 2;
        }
        err = test_app_restart(app);
        if(err)return err;
    }

    err = palette_update_sdl_palette(&app->palette, app->sdl_palette);
    if(err)return err;
    err = palette_step(&app->palette);
    if(err)return err;

    /* Consume events right before using them in hexgame_step
    (so e.g. we have most up-to-date information about which keys
    were pressed) */
    err = test_app_poll_events(app);
    if(err)return err;

    err = hexgame_unpauseable_step(game);
    if(err)return err;

    if(app->show_menu && test_app_menu_pauses_game(&app->menu)){
        /* Menu doesn't need to do anything each frame */
    }else if(app->hexgame.show_minimap){
        /* Minimap doesn't need to do anything each frame */
    }else if(app->hexgame_running){
        err = hexgame_step(game);
        if(err)return err;
    }else{
        /* Menu doesn't need to do anything each frame */
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

static const char *_test_app_get_last_or_next_recording_filename(
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

const char *test_app_get_load_recording_filename(test_app_t *app){
    if(app->load_recording_filename)return app->load_recording_filename;
    return _test_app_get_last_or_next_recording_filename(app, false);
}

const char *test_app_get_save_recording_filename(test_app_t *app){
    if(app->save_recording_filename)return app->save_recording_filename;
    return _test_app_get_last_or_next_recording_filename(app, true);
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
