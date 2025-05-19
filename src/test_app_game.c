
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"
#include "hexgame.h"




/****************
* TEST_APP.GAME *
****************/

static int _print_text_expr(test_app_t *app, hexmap_submap_t *submap,
    valexpr_t *text_expr
){
    int err;
    valexpr_result_t result = {0};
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
    const char *text = val_get_str(result.val);
    if(text){
        test_app_printf(app, 0, 0, text);
    }
    return 0;
}

static bool _show_dead_msgs(test_app_t *app, bool *showed_dead_msg_ptr){
    int err;
    hexgame_t *game = &app->hexgame;

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

    *showed_dead_msg_ptr = showed_dead_msg;
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

    /* NOTE/HACK: camera_render takes care of rendering the minimap if
    game->minimap_state.zoom is truthy */
    err = camera_render(app->camera,
        app->surface,
        app->sdl_palette, app->scw/2, app->sch/2,
        1 /* app->zoom */);
    if(err)return err;

    if(app->show_console){
        err = test_app_blit_console(app, 0, app->lines_printed * app->font.char_h);
        if(err)return err;
    }

    hexmap_submap_t *submap = app->camera->cur_submap;

    if(app->have_audio){
        SDL_LockAudioDevice(app->audio_id);
        err = hexgame_update_audio_data(game, app->camera);
        if(err)return err;
        SDL_UnlockAudioDevice(app->audio_id);
    }

    if(app->show_menu){
        test_app_menu_render(&app->menu);
    }else if(game->minimap_state.zoom){
        /* Rendering of the minimap is handled, interestingly, by
        camera_render, which was already called above */
        test_app_printf(app, 0, app->lines_printed * app->font.char_h,
            "Press Enter...\n");
    }else{

        /* Show messages like "you hit a wall!", "you were crushed!" etc */
        bool showed_dead_msg;
        err = _show_dead_msgs(app, &showed_dead_msg);
        if(err)return err;

        if(!showed_dead_msg && submap){
            /* Show submap texts */
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


static int _save_recording(test_app_t *app){
    /* When you press F9 at the menu */
    int err;

    hexgame_t *game = &app->hexgame;
    player_t *player = hexgame_get_player_by_keymap(game, HEXGAME_PLAYER_0);
    if(!player)return 0;

    body_t *body = player->body;
    if(!body){
        fprintf(stderr,
            "Can't record without a body!\n");
    }else if(body->recording.action != RECORDING_ACTION_RECORD){
        const char *_recording_filename =
            test_app_get_save_recording_filename(app);

        /* In case this is where we got the filename, we now set
        it to NULL so next time we start recording, we won't
        overwrite this one */
        app->save_recording_filename = NULL;

        const char *recording_filename = stringstore_get(
            &app->prend.filename_store, _recording_filename);
        if(!recording_filename)return 1;

        /* So that the next time we play a recording with F10,
        we play the one we're recording now */
        app->load_recording_filename = recording_filename;

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

    return 0;
}


static int _load_recording(test_app_t *app, bool shift){
    /* When you press F10 at the menu.
    When shift is true, use player's existing body; otherwise, create a
    new body (not controlled by player). */
    int err;

    hexgame_t *game = &app->hexgame;
    player_t *player = hexgame_get_player_by_keymap(game, HEXGAME_PLAYER_0);
    if(!player)return 0;

    body_t *body = player->body;
    if(!body){
        fprintf(stderr,
            "Can't play back recording without a body!\n");
        return 0;
    }

    const char *_recording_filename =
        test_app_get_load_recording_filename(app);
    if(_recording_filename == NULL){
        fprintf(stderr, "Couldn't find file of last recording. "
            "Maybe you need to record your first one with F9?\n");
    }else{
        const char *recording_filename = stringstore_get(
            &app->prend.filename_store, _recording_filename);
        if(!recording_filename)return 1;
        fprintf(stderr, "Playing back from file: %s\n",
            recording_filename);
        /* If we're recording, save the recording so it can be loaded. */
        if(body->recording.action == RECORDING_ACTION_RECORD){
            err = body_stop_recording(body);
            if(err)return err;
        }

        if(shift){
            /* Play the recording using player's existing body */
            err = body_load_recording(body, recording_filename,
                true);
            if(err)return err;
            err = body_play_recording(body);
            if(err)return err;
        }else{
            /* Create a new body & play the recording with it */
            /* TODO: Recordings need to state which map they
            expect! The following is a hack: you must play
            recordings belonging to your correct map... */
            err = hexmap_load_recording(body->map,
                recording_filename, NULL, true, 0, NULL, NULL);
            if(err)return err;
        }
    }

    return 0;
}


static int _handle_tab_key(hexgame_t *game, SDL_Event *event){
    /* When you press the TAB key */
    int err;

    /* Try to detect when player alt-tabbed to/from this window */
    if((event->key.keysym.mod & KMOD_ALT) || (event->key.repeat))return 0;

    if(event->key.keysym.mod & KMOD_SHIFT){
        player_t *player = hexgame_get_player_by_keymap(game, HEXGAME_PLAYER_0);
        body_t *body = player? player->body: NULL;
        if(body){
            err = hexgame_use_mappoint(game, body->map, body->cur_submap);
            if(err)return err;
        }
    }else{
        /* Cycle between 3 values: 0 means don't show minimap, and
        1 and 2 are zoom values. */
        int zoom = (game->minimap_state.zoom + 3 - 1) % 3;
        hexgame_set_minimap_zoom(game, zoom);
    }
    return 0;
}


int test_app_process_event_game(test_app_t *app, SDL_Event *event){
    /* Handle special keys (the F1-F12 keys, Escape, Tab, Enter, etc).
    Regular player keypresses are handled separately by caller; we don't
    affect them, can't override them, etc. */
    int err;

    hexgame_t *game = &app->hexgame;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_F4 && app->developer_mode){
            err = test_app_reload_prismelrenderers(app);
            if(err)return err;
            err = test_app_reload_map(app);
            if(err)return err;
        }else if(event->key.keysym.sym == SDLK_F5 && app->developer_mode){
            app->hexgame_running = !app->hexgame_running;
        }else if(event->key.keysym.sym == SDLK_PAGEUP && app->developer_mode){
            if(!app->hexgame_running){
                /* Do 1 step */
                err = hexgame_step(&app->hexgame);
                if(err)return err;
            }
        }else if(event->key.keysym.sym == SDLK_ESCAPE){
            test_app_menu_set_screen(&app->menu, TEST_APP_MENU_SCREEN_PAUSED);
            app->show_menu = true;
        }else if(event->key.keysym.sym == SDLK_TAB && app->developer_mode){
            err = _handle_tab_key(game, event);
            if(err)return err;
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
            fprintf(stderr, "Camera follow: %s\n", app->camera->follow? "on": "off");
        }else if(event->key.keysym.sym == SDLK_F8 && app->developer_mode){
            game->anim_debug = !game->anim_debug;
            fprintf(stderr, "Anim debug mode: %s\n", game->anim_debug? "on": "off");
        }else if(event->key.keysym.sym == SDLK_F9 && app->developer_mode){
            /* save recording */
            err = _save_recording(app);
            if(err)return err;
        }else if(event->key.keysym.sym == SDLK_F10 && app->developer_mode){
            /* load recording */
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            err = _load_recording(app, shift);
            if(err)return err;
        }else if(event->key.keysym.sym == SDLK_F12 && app->developer_mode){
            /* Dump information about all players and their bodies */
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap < 0)continue;
                if(event->key.keysym.mod & KMOD_CTRL){
                    /* Dump details about player's body's stateset */
                    if(!player->body)continue;
                    stateset_dump(player->body->stateset, stderr, 0);
                }else{
                    /* Dump details about player & their body */
                    player_dump(player, 0);
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


static int _use_cur_mappoint(hexgame_t *game, int cur_mappoint){
    int err;

    if(cur_mappoint < 0)return 0;

    minimap_state_t *minimap_state = &game->minimap_state;
    if(minimap_state->cur_mappoint == cur_mappoint)return 0;

    minimap_state->cur_mappoint = cur_mappoint;
    minimap_state_mappoint_t *mappoint = &minimap_state->mappoints[cur_mappoint];

    player_t *player = hexgame_get_player_by_keymap(game, HEXGAME_PLAYER_0);
    body_t *body = player == NULL? NULL: player->body;
    if(body == NULL)return 0;

    err = body_relocate(body, NULL, &mappoint->location, NULL, NULL);
    if(err)return err;

    return 0;
}


int test_app_process_event_minimap(test_app_t *app, SDL_Event *event){
    /* Handle keys pressed while the minimap is open */
    int err;

    hexgame_t *game = &app->hexgame;
    minimap_state_t *minimap_state = &game->minimap_state;

    /* We should only be handling minimap events if minimap is open! */
    if(!minimap_state->zoom)return 0;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_RETURN){
            hexgame_set_minimap_zoom(game, 0);
        }else if(event->key.keysym.sym == SDLK_TAB && app->developer_mode){
            err = _handle_tab_key(game, event);
            if(err)return err;
        }else if(event->key.keysym.sym == SDLK_LEFT){
            int cur_mappoint = minimap_state->cur_mappoint;
            if(cur_mappoint < 0){
                if(minimap_state->mappoints_len > 0)cur_mappoint = 0;
            }else{
                if(--cur_mappoint < 0){
                    cur_mappoint = minimap_state->mappoints_len - 1;
                }
            }
            err = _use_cur_mappoint(game, cur_mappoint);
            if(err)return err;
        }else if(event->key.keysym.sym == SDLK_RIGHT){
            int cur_mappoint = minimap_state->cur_mappoint;
            if(cur_mappoint < 0){
                if(minimap_state->mappoints_len > 0)cur_mappoint = 0;
            }else{
                if(++cur_mappoint >= minimap_state->mappoints_len){
                    cur_mappoint = 0;
                }
            }
            err = _use_cur_mappoint(game, cur_mappoint);
            if(err)return err;
        }
    }
    return 0;
}
