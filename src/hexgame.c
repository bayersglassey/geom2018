

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef GEOM_HEXGAME_DEBUG_MALLOC
    #include <malloc.h>
#endif

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
#include "prismelrenderer.h"
#include "array.h"
#include "util.h"
#include "write.h"


#ifndef COLLIDE_PLAYERS
#define COLLIDE_PLAYERS 1
#endif


/***********
 * HEXGAME *
 ***********/

void hexgame_cleanup(hexgame_t *game){
    ARRAY_FREE_PTR(player_t*, game->players, player_cleanup)
    ARRAY_FREE_PTR(actor_t*, game->actors, actor_cleanup)
}

int hexgame_init(hexgame_t *game, hexmap_t *map){
    game->frame_i = 0;
    game->zoomout = false;
    game->follow = false;
    game->smooth_scroll = true;
    game->map = map;
    game->cur_submap = NULL;
    hexgame_set_camera(game, (vec_t){0}, 0);
    ARRAY_INIT(game->players)
    ARRAY_INIT(game->actors)
    return 0;
}

void hexgame_set_camera(hexgame_t *game, vec_t camera_pos,
    rot_t camera_rot
){
    vecspace_t *space = game->map->space;
    vec_cpy(space->dims, game->camera_pos, camera_pos);
    vec_cpy(space->dims, game->camera_scrollpos, camera_pos);
    game->camera_rot = camera_rot;
    game->reset_camera = false;
}

int hexgame_load_actors(hexgame_t *game){
    int err;
    hexmap_t *map = game->map;
    for(int i = 0; i < map->actor_recordings_len; i++){
        hexmap_recording_t *actor_recording =
            map->actor_recordings[i];
        const char *filename = actor_recording->filename;

        ARRAY_PUSH_NEW(player_t*, game->players, player)
        err = player_init(player, game->map, NULL, NULL,
            -1, NULL, NULL);
        if(err)return err;

        player->palmapper = actor_recording->palmapper;

        ARRAY_PUSH_NEW(actor_t*, game->actors, actor)
        err = actor_init(actor, game->map, player, filename, NULL);
        if(err)return err;
    }
    return 0;
}

int hexgame_reset_player(hexgame_t *game, player_t *player, bool hard){
    vec_cpy(game->map->space->dims, player->pos,
        hard? game->map->spawn: player->respawn_pos);
    player->rot = 0;
    player->turn = false;
    player->state = player->stateset.states[0];
    player->frame_i = 0;
    player->cooldown = 0;

    player_keyinfo_reset(&player->keyinfo);

    return 0;
}

int hexgame_reset_player_by_keymap(hexgame_t *game, int keymap, bool hard){
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->keymap == keymap){
            return hexgame_reset_player(game, player, hard);
        }
    }

    /* Player not being found with given keymap is fine. */
    return 0;
}

int hexgame_load_player_recording(hexgame_t *game, const char *filename,
    int keymap, bool loop
){
    int err;

    ARRAY_PUSH_NEW(player_t*, game->players, player)
    err = player_init(player, game->map, NULL, NULL,
        keymap, game->map->spawn, NULL);
    if(err)return err;

    err = player_recording_load(&player->recording, filename, game, loop);
    if(err)return err;

    err = player_play_recording(player);
    if(err)return err;

    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    int err;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = true;
        }else if(event->key.keysym.sym == SDLK_F7){
            game->follow = !game->follow;
        }else if(event->key.keysym.sym == SDLK_F8){
            game->smooth_scroll = !game->smooth_scroll;
        }else if(event->key.keysym.sym == SDLK_F9){
            /* save recording */
            if(game->players_len >= 1){
                player_t *player = game->players[0];
                if(player->recording.action != 2){
                    fprintf(stderr,
                        "Can't stop recording without starting first! "
                        "(Try pressing 'R' before 'F9')\n");
                }else{
                    fprintf(stderr, "Finished recording. "
                        "Press F10 to play it back.\n");
                    err = player_stop_recording(player);
                    if(err)return err;
                }
            }
        }else if(event->key.keysym.sym == SDLK_F10){
            /* load recording */
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            const char *recording_filename = get_last_recording_filename();
            if(recording_filename == NULL){
                fprintf(stderr, "Couldn't find file of last recording. "
                    "Maybe you need to record your first one with 'R'?\n");
            }else{
                fprintf(stderr, "Playing back from file: %s\n",
                    recording_filename);
                if(shift){
                    if(game->players_len < 1){
                        fprintf(stderr, "No player!\n");
                        return 2;}
                    player_t *player = game->players[0];
                    err = player_load_recording(player,
                        recording_filename, game, true);
                    if(err)return err;
                    err = player_play_recording(player);
                    if(err)return err;
                }else{
                    err = hexgame_load_player_recording(game,
                        recording_filename, -1, true);
                    if(err)return err;
                }
            }
#ifdef GEOM_HEXGAME_DEBUG_MALLOC
        }else if(event->key.keysym.sym == SDLK_F11){
            malloc_stats();
#endif
        }else if(event->key.keysym.sym == SDLK_r){
            /* start recording */
            if(game->players_len >= 1){
                const char *recording_filename = get_next_recording_filename();
                fprintf(stderr, "Recording to file: %s "
                    " (When finished, press F9 to save!)\n",
                    recording_filename);
                err = player_start_recording(game->players[0],
                    strdup(recording_filename));
                if(err)return err;
            }else{
                fprintf(stderr, "Can't start recording -- no player!\n");
            }
        }else if(!event->key.repeat){
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            if(event->key.keysym.sym == SDLK_1){
                hexgame_reset_player_by_keymap(game, 0, shift);
                game->reset_camera = true;}
            if(event->key.keysym.sym == SDLK_2){
                hexgame_reset_player_by_keymap(game, 1, shift);}
        }
    }else if(event->type == SDL_KEYUP){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = false;
        }
    }

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_process_event(player, event);
        if(err)return err;
    }

    return 0;
}

int hexgame_step(hexgame_t *game){
    int err;

    game->frame_i++;
    if(game->frame_i == MAX_FRAME_I)game->frame_i = 0;

    hexmap_t *map = game->map;
    vecspace_t *space = map->space;

#ifndef DONT_ANIMATE_PALETTE
    /* Animate palette */
    /* NOTE: compiling with -DDONT_ANIMATE_PALETTE is useful for producing
    animated GIF gameplay clips, since our palette changes are too subtle
    for my GIF capturing software (byzanz), leading to periodic flashes of
    different colours. */
    if(game->cur_submap != NULL){
        err = palette_step(&game->cur_submap->palette);
        if(err)return err;
    }
#endif

    /* Do 1 gameplay step for each actor */
    for(int i = 0; i < game->actors_len; i++){
        actor_t *actor = game->actors[i];
        err = actor_step(actor, game);
        if(err)return err;
    }

#if COLLIDE_PLAYERS
    /* Collide players with each other */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->state == NULL)continue;
        if(player->recording.action == 1){
            /* No hitboxes for players whose recording is playing */
            /* MAYBE TODO: These players should die too, but then their
            recording should restart after a brief pause.
            Maybe we can reuse player->colldown for the pause. */
            continue;
        }
        hexcollmap_t *hitbox = player->state->hitbox;
        if(hitbox == NULL)continue;

        trf_t hitbox_trf;
        player_init_trf(player, &hitbox_trf, space);

        /* This player has a hitbox! So collide it against all other players'
        hitboxes. */
        for(int j = i + 1; j < game->players_len; j++){
            player_t *player_other = game->players[j];
            if(player_other->state == NULL)continue;
            hexcollmap_t *hitbox_other = player_other->state->hitbox;
            if(hitbox_other == NULL)continue;

            trf_t hitbox_other_trf;
            player_init_trf(player_other, &hitbox_other_trf, space);

            /* The other player has a hitbox! Do the collision... */
            bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                hitbox_other, &hitbox_other_trf, space, false);
            if(collide){
                /* There was a collision!
                Now we find out who was right... and who was dead. */

                /* Hardcoded "dead" state name... I suppose we could
                have a char* player->dead_anim_name, but whatever. */
                if(player->state->crushes){
                    err = player_set_state(player_other, "dead");
                    if(err)return err;
                }
                if(player_other->state->crushes){
                    err = player_set_state(player, "dead");
                    if(err)return err;
                }
            }
        }
    }
#endif

    /* Do 1 gameplay step for each player */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game);
        if(err)return err;
    }

    /* Figure out game's current submap */
    if(game->players_len >= 1){
        player_t *player = game->players[0];
        if(game->cur_submap != player->cur_submap){
            game->cur_submap = player->cur_submap;

            /* TODO: Smoothly transition between
            old & new palettes */
            err = palette_reset(&game->cur_submap->palette);
            if(err)return err;
        }
    }

    /* Set camera */
    int camera_type = -1;
    if(game->follow)camera_type = 1;
    else if(game->cur_submap != NULL){
        camera_type = game->cur_submap->camera_type;}
    if(camera_type == 0){
        vec_cpy(space->dims, game->camera_pos,
            game->cur_submap->camera_pos);
        game->camera_rot = 0;
    }else if(camera_type == 1){
        if(game->players_len >= 1){
            player_t *player = game->players[0];
            vec_cpy(space->dims, game->camera_pos,
                player->pos);
            game->camera_rot = player->rot;
        }
    }

    /* Scroll renderpos */
    if(game->smooth_scroll && !game->reset_camera){
        vec_ptr_t camera_scrollpos = game->camera_scrollpos;

        vec_t target_scrollpos;
        vec_cpy(space->dims, target_scrollpos, game->camera_pos);

        vec_t diff;
        vec_cpy(space->dims, diff, target_scrollpos);
        vec_sub(space->dims, diff, camera_scrollpos);

        rot_t rot;
        int dist, angle;
        hexspace_angle(diff, &rot, &dist, &angle);

        if(dist > 0){
            vec_t add;
            if(angle > 0){
                /* X + R X */
                hexspace_set(add, 2, 1);
            }else{
                /* X */
                hexspace_set(add, 1, 0);
            }
            hexspace_rot(add, rot);
            vec_add(space->dims, camera_scrollpos, add);
        }
    }else{
        vec_cpy(space->dims, game->camera_scrollpos, game->camera_pos);
        game->reset_camera = false;
    }

    return 0;
}

int hexgame_render(hexgame_t *game,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom
){
    int err;

    if(game->cur_submap != NULL){
        err = palette_update_sdl_palette(&game->cur_submap->palette, pal);
        if(err)return err;
    }

    hexmap_t *map = game->map;
    vecspace_t *space = map->space;

    vec_t camera_renderpos;
    vec4_vec_from_hexspace(camera_renderpos, game->camera_scrollpos);

    prismelmapper_t *mapper = NULL;

    hexmap_submap_t *submap = game->cur_submap;
    if(submap != NULL){
        if(!game->zoomout)mapper = submap->mapper;
    }

    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        rendergraph_t *rgraph = submap->rgraph_map;

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(submap != game->cur_submap)continue;
#endif

        vec_t pos;
        vec4_vec_from_hexspace(pos, submap->pos);
        vec_sub(rgraph->space->dims, pos, camera_renderpos);
        vec_mul(rgraph->space, pos, game->map->unit);

        rot_t rot = vec4_rot_from_hexspace(0);
        //rot_t rot = vec4_rot_from_hexspace(
        //    rot_inv(space->rot_max, game->camera_rot));
        flip_t flip = false;
        int frame_i = game->frame_i;

        err = rendergraph_render(rgraph, renderer, surface,
            pal, game->map->prend,
            x0, y0, zoom,
            pos, rot, flip, frame_i, mapper);
        if(err)return err;
    }

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(player->cur_submap != game->cur_submap)continue;
#endif

        err = player_render(player,
            renderer, surface,
            pal, x0, y0, zoom,
            map, camera_renderpos, mapper);
        if(err)return err;
    }

    return 0;
}

