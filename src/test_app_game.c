
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"
#include "hexgame.h"



/********************
* HEXGAME CALLBACKS *
********************/

int test_app_new_game_callback(hexgame_t *game, player_t *player,
    const char *map_filename
){
    int err;
    hexmap_t *map;
    err = hexgame_get_or_load_map(game, map_filename, &map);
    if(err)return err;
    return hexgame_reset_players(game, RESET_HARD, map);
}

int test_app_continue_callback(hexgame_t *game, player_t *player){
    int err;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(!player->body){
            /* This can definitely happen, e.g. generally 2 players are
            created at start of game, but only player 0 has a body...
            unless "--players 2" is supplied at commandline, or player
            0 touches the 2-player door, etc). */
            continue;
        }
        err = player_reload(player);
        if(err)return err;
    }
    return 0;
}

int test_app_set_players_callback(hexgame_t *game, player_t *player,
    int n_players
){
    test_app_t *app = game->app;
    return test_app_set_players(app, n_players);
}

int test_app_exit_callback(hexgame_t *game, player_t *player){
    test_app_t *app = game->app;
    app->loop = false;
    return 0;
}



/****************
* TEST_APP.GAME *
****************/


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

        /* Set player->body */
        if(i >= n_players){
            player->body = NULL;
            continue;
        }else{
            ARRAY_PUSH_NEW(body_t*, respawn_map->bodies, body)
            err = body_init(body, player->game, respawn_map,
                app->stateset_filename, NULL, NULL);
            if(err)return err;

            /* Attach body to player */
            player->body = body;

            /* Move body to the respawn location */
            err = body_respawn(body,
                player->respawn_location.loc.pos, player->respawn_location.loc.rot,
                player->respawn_location.loc.turn, respawn_map);
            if(err)return err;
        }
    }
    return 0;
}


static int _print_text_expr(test_app_t *app, hexmap_submap_t *submap,
    valexpr_t *text_expr
){
    int err;
    val_t *result;
    valexpr_context_t context = {
        .mapvars = &submap->map->vars,
        .globalvars = &submap->map->game->vars
    };
    err = valexpr_get(text_expr, &context, &result);
    if(err){
        fprintf(stderr,
            "Error while evaluating text for submap: %s\n",
            submap->filename);
        valexpr_fprintf(text_expr, stderr);
        fputc('\n', stderr);
        return err;
    }
    const char *text = val_get_str(result);
    if(text){
        test_app_printf(app, 0, 0, text);
    }
    return 0;
}

int test_app_render_game(test_app_t *app){
    int err;

    hexgame_t *game = &app->hexgame;

    RET_IF_SDL_NZ(SDL_FillRect(app->surface, NULL, 255));

    if(app->camera_mapper){
        /* camera->mapper is set to NULL at start of each step, it's up
        to us to set it if desired before calling camera_render */
        app->camera->mapper = app->camera_mapper;
    }
    err = camera_render(app->camera,
        app->surface,
        app->sdl_palette, app->scw/2, app->sch/2,
        1 /* app->zoom */);
    if(err)return err;

    bool showed_dead_msg = false;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        body_t *body = player->body;
        if(!body)continue;
        if(body->dead == BODY_MOSTLY_DEAD){
            showed_dead_msg = true;
            test_app_printf(app, 0, app->lines_printed * app->font.char_h,
                "You ran into a wall!\n"
                "Press jump to retry from where you jumped.\n"
                "Press %i to retry from last save point.\n",
                i+1);
        }else if(body->dead == BODY_ALL_DEAD){
            showed_dead_msg = true;
            test_app_printf(app, 0, app->lines_printed * app->font.char_h,
                "You were crushed!\n"
                "Press jump or %i to retry from last save point.\n",
                i+1);
        }else if(body->out_of_bounds && !body->state->flying){
            showed_dead_msg = true;
            test_app_printf(app, 0, app->lines_printed * app->font.char_h,
                "You jumped off the map!\n"
                "Press jump to retry from where you jumped.\n"
                "Press %i to retry from last save point.\n",
                i+1);
        }
    }

    if(app->show_console){
        err = test_app_blit_console(app, 0, app->lines_printed * app->font.char_h);
        if(err)return err;
    }else if(!showed_dead_msg){
        hexmap_submap_t *submap = app->camera->cur_submap;
        if(submap){
            for(int i = 0; i < submap->text_exprs_len; i++){
                valexpr_t *text_expr = submap->text_exprs[i];
                err = _print_text_expr(app, submap, text_expr);
                if(err)return err;
            }
            hexcollmap_t *collmap = &submap->collmap;
            for(int i = 0; i < collmap->text_exprs_len; i++){
                valexpr_t *text_expr = collmap->text_exprs[i];
                err = _print_text_expr(app, submap, text_expr);
                if(err)return err;
            }
        }
    }

    return 0;
}


int test_app_process_event_game(test_app_t *app, SDL_Event *event){
    int err;

    hexgame_t *game = &app->hexgame;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_PAGEUP && app->developer_mode){
            if(!app->hexgame_running){
                /* Do 1 step */
                err = hexgame_step(&app->hexgame);
                if(err)return err;
            }
        }else if(event->key.keysym.sym == SDLK_TAB){
            /* Cycle between 3 values: 0 means don't show minimap, and
            1 and 2 are zoom values. */
            game->show_minimap = (game->show_minimap + 1) % 3;
        }else if(event->key.keysym.sym == SDLK_F6 && app->developer_mode){
            /* Hack, we really want to force camera->mapper to NULL, but
            instead we assume the existence of this mapper called "single" */
            const char *mapper_name = "single";
            app->camera_mapper = prismelrenderer_get_mapper(&app->prend, mapper_name);
            if(app->camera_mapper == NULL){
                fprintf(stderr, "%s: Couldn't find mapper: %s\n",
                    __func__, mapper_name);
                return 2;
            }
        }else if(event->key.keysym.sym == SDLK_F7 && app->developer_mode){
            app->camera->follow = !app->camera->follow;
        }else if(event->key.keysym.sym == SDLK_F9 && app->developer_mode){
            /* save recording */
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap != 0)continue;

                body_t *body = player->body;
                if(!body){
                    fprintf(stderr,
                        "Can't record without a body!\n");
                }else if(body->recording.action != 2){
                    const char *_recording_filename =
                        test_app_get_next_recording_filename(app);
                    const char *recording_filename = stringstore_get(
                        &app->prend.filename_store, _recording_filename);
                    if(!recording_filename)return 1;
                    fprintf(stderr, "Recording to file: %s "
                        " (When finished, press F9 to save!)\n",
                        recording_filename);
                    err = body_start_recording(body, recording_filename);
                    if(err)return err;
                }else{
                    fprintf(stderr, "Finished recording. "
                        "Press F10 to play it back, or Shift+F10 to "
                        "play it back using your current body.\n");
                    err = body_stop_recording(body);
                    if(err)return err;
                }
            }
        }else if(event->key.keysym.sym == SDLK_F10 && app->developer_mode){
            /* load recording */
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            const char *_recording_filename =
                test_app_get_last_recording_filename(app);
            if(_recording_filename == NULL){
                fprintf(stderr, "Couldn't find file of last recording. "
                    "Maybe you need to record your first one with F9?\n");
            }else{
                const char *recording_filename = stringstore_get(
                    &app->prend.filename_store, _recording_filename);
                if(!recording_filename)return 1;
                fprintf(stderr, "Playing back from file: %s\n",
                    recording_filename);
                for(int i = 0; i < game->players_len; i++){
                    player_t *player = game->players[i];
                    if(player->keymap != 0)continue;

                    body_t *body = player->body;
                    if(!body){
                        fprintf(stderr,
                            "Can't play back recording without a body!\n");
                        break;
                    }

                    /* If we're recording, save the recording so it can be loaded. */
                    if(body->recording.action == 2){
                        err = body_stop_recording(body);
                        if(err)return err;
                    }

                    if(shift){
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
                            recording_filename, NULL, true, 0, NULL, NULL);
                        if(err)return err;
                    }
                }
            }
        }else if(event->key.keysym.sym == SDLK_F12 && app->developer_mode){
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap < 0)continue;
                if(event->key.keysym.mod & KMOD_CTRL){
                    if(!player->body)continue;
                    stateset_dump(&player->body->stateset, stderr, 0);
                }else{
                    hexgame_player_dump(player, 0);
                }
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
        if(event->key.keysym.sym == SDLK_F6 && app->developer_mode){
            app->camera_mapper = NULL;
        }
    }
    return 0;
}
