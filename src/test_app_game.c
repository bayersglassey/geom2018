
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

        ARRAY_PUSH_NEW(body_t*, respawn_map->bodies, body)
        err = body_init(body, player->game, respawn_map,
            strdup(app->stateset_filename), NULL, NULL);
        if(err)return err;

        /* Attach body to player */
        player_set_body(player, body);

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


int test_app_render_game(test_app_t *app){
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

        if(app->show_controls && !app->show_console){
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
                "  F5          -> Pause/unpause\n"
            );
            line_y += 10;
        }

        if(app->show_console){
            err = test_app_blit_console(app, app->surface,
                0, line_y * app->font.char_h);
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


int test_app_process_event_game(test_app_t *app, SDL_Event *event){
    int err;

    hexgame_t *game = &app->hexgame;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_RETURN){
            app->show_controls = !app->show_controls;
        }else if(event->key.keysym.sym == SDLK_PAGEUP){
            if(!app->hexgame_running){
                /* Do 1 step */
                err = hexgame_step(&app->hexgame);
                if(err)return err;
            }
        }else if(event->key.keysym.sym == SDLK_F6){
            if(event->key.keysym.mod & KMOD_CTRL){
                game->show_minimap = !game->show_minimap;
            }else{
                /* Hack, we really want to force camera->mapper to NULL, but
                instead we assume the existence of this mapper called "single" */
                const char *mapper_name = "single";
                app->camera_mapper = prismelrenderer_get_mapper(&app->prend, mapper_name);
                if(app->camera_mapper == NULL){
                    fprintf(stderr, "%s: Couldn't find mapper: %s\n",
                        __func__, mapper_name);
                    return 2;
                }
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
