

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



/**********
 * CAMERA *
 **********/

void camera_cleanup(camera_t *camera){
    /* Nuthin */
}

int camera_init(camera_t *camera, hexgame_t *game){
    camera->game = game;
    camera->map = game->map;
    camera->cur_submap = NULL;

    camera->zoomout = false;
    camera->follow = false;
    camera->smooth_scroll = true;

    memset(camera->colors, 0, sizeof(camera->colors));
    camera->colors_fade = 0;

    camera_set(camera, (vec_t){0}, 0);
    return 0;
}

void camera_set(camera_t *camera, vec_t pos,
    rot_t rot
){
    vecspace_t *space = camera->map->space;
    vec_cpy(space->dims, camera->pos, pos);
    vec_cpy(space->dims, camera->scrollpos, pos);
    camera->rot = rot;
    camera->should_reset = false;
}

body_t *camera_get_body(camera_t *camera){
    /* HERE'S THE HACKY PART WHERE WE ASSUME CAMERA ALWAYS POINTS AT
    BODY 0. TODO: implement camera->body instead */
    hexmap_t *map = camera->map;
    if(map->bodies_len >= 1)return map->bodies[0];
    return NULL;
}

void camera_colors_flash(camera_t *camera, Uint8 r, Uint8 g, Uint8 b,
    int percent
){
    for(int i = 0; i < 256; i++){
        SDL_Color *c = &camera->colors[i];
        interpolate_color(c, r, g, b, percent, 100);
    }
    camera->colors_fade = 0;
}

void camera_colors_flash_white(camera_t *camera, int percent){
    camera_colors_flash(camera, 255, 255, 255, percent);
}

static int camera_colors_step(camera_t *camera, palette_t *palette){
    int err;
    if(camera->cur_submap != NULL){
        err = palette_update_colors(palette, camera->colors,
            camera->colors_fade, HEXGAME_MAX_COLORS_FADE);
        if(err)return err;
        if(camera->colors_fade < HEXGAME_MAX_COLORS_FADE)camera->colors_fade++;
    }
    return 0;
}

int camera_step(camera_t *camera){
    int err;

    hexgame_t *game = camera->game;
    hexmap_t *map = camera->map;
    vecspace_t *space = map->space;
    body_t *body = camera_get_body(camera);

#ifndef DONT_ANIMATE_PALETTE
    /* Animate palette */
    /* NOTE: compiling with -DDONT_ANIMATE_PALETTE is useful for producing
    animated GIF gameplay clips, since our palette changes are too subtle
    for my GIF capturing software (byzanz), leading to periodic flashes of
    different colours. */

    /* NOTE: The following call to palette_step shouldn't be in here.
    This means palettes are updated once for every camera which is looking
    at them, which makes no sense, but will work ok so long as we only
    have 1 camera. */
    if(camera->cur_submap != NULL){
        err = palette_step(&camera->cur_submap->palette);
        if(err)return err;
    }
#endif

    err = camera_colors_step(camera, &camera->cur_submap->palette);
    if(err)return err;

    /* Figure out camera's current submap */
    if(body != NULL){
        if(camera->cur_submap != body->cur_submap){
            camera->cur_submap = body->cur_submap;

#ifndef DONT_ANIMATE_PALETTE
            if(camera->smooth_scroll && !camera->should_reset){
                /* Smoothly transition between old & new palettes */
                camera->colors_fade = 0;
            }
#endif

            err = palette_reset(&camera->cur_submap->palette);
            if(err)return err;
        }
    }

    /* Set camera */
    int camera_type = -1;
    if(camera->follow)camera_type = 1;
    else if(camera->cur_submap != NULL){
        camera_type = camera->cur_submap->camera_type;}
    if(camera_type == 0){
        vec_cpy(space->dims, camera->pos,
            camera->cur_submap->camera_pos);
        camera->rot = 0;
    }else if(camera_type == 1){
        if(body != NULL){
            vec_cpy(space->dims, camera->pos,
                body->pos);
            camera->rot = body->rot;
        }
    }

    /* Scroll renderpos */
    if(camera->smooth_scroll && !camera->should_reset){
        vec_ptr_t scrollpos = camera->scrollpos;

        vec_t target_scrollpos;
        vec_cpy(space->dims, target_scrollpos, camera->pos);

        vec_t diff;
        vec_cpy(space->dims, diff, target_scrollpos);
        vec_sub(space->dims, diff, scrollpos);

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
            vec_add(space->dims, scrollpos, add);
        }
    }else{
        vec_cpy(space->dims, camera->scrollpos, camera->pos);
        camera->should_reset = false;
    }

    return 0;
}

int camera_render(camera_t *camera,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom
){
    int err;

    err = update_sdl_palette(pal, camera->colors);
    if(err)return err;

    hexgame_t *game = camera->game;
    hexmap_t *map = camera->map;
    vecspace_t *space = map->space;

    vec_t camera_renderpos;
    vec4_vec_from_hexspace(camera_renderpos, camera->scrollpos);

    prismelmapper_t *mapper = NULL;

    hexmap_submap_t *submap = camera->cur_submap;
    if(submap != NULL){
        if(!camera->zoomout)mapper = submap->mapper;
    }

    /* Render map's submaps */
    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        rendergraph_t *rgraph = submap->rgraph_map;

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(submap != camera->cur_submap)continue;
#endif

        vec_t pos;
        vec4_vec_from_hexspace(pos, submap->pos);
        vec_sub(rgraph->space->dims, pos, camera_renderpos);
        vec_mul(rgraph->space, pos, camera->map->unit);

        rot_t rot = vec4_rot_from_hexspace(0);
        //rot_t rot = vec4_rot_from_hexspace(
        //    rot_inv(space->rot_max, camera->rot));
        flip_t flip = false;
        int frame_i = game->frame_i;

        err = rendergraph_render(rgraph, renderer, surface,
            pal, camera->map->prend,
            x0, y0, zoom,
            pos, rot, flip, frame_i, mapper);
        if(err)return err;
    }

    /* Render map's bodies */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(body->cur_submap != camera->cur_submap)continue;
#endif

        err = body_render(body,
            renderer, surface,
            pal, x0, y0, zoom,
            map, camera_renderpos, mapper);
        if(err)return err;
    }

    return 0;
}


/***********
 * HEXGAME *
 ***********/

void hexgame_cleanup(hexgame_t *game){
    camera_cleanup(&game->camera);
    ARRAY_FREE_PTR(player_t*, game->players, player_cleanup)
    ARRAY_FREE_PTR(actor_t*, game->actors, actor_cleanup)
}

int hexgame_init(hexgame_t *game, hexmap_t *map){
    int err;

    game->frame_i = 0;
    game->map = map;

    err = camera_init(&game->camera, game);
    if(err)return err;

    ARRAY_INIT(game->players)
    ARRAY_INIT(game->actors)
    return 0;
}

int hexgame_load_actors(hexgame_t *game){
    int err;
    hexmap_t *map = game->map;
    for(int i = 0; i < map->actor_recordings_len; i++){
        hexmap_recording_t *actor_recording =
            map->actor_recordings[i];
        const char *filename = actor_recording->filename;

        ARRAY_PUSH_NEW(body_t*, map->bodies, body)
        err = body_init(body, game, map, NULL, NULL,
            actor_recording->palmapper);
        if(err)return err;

        ARRAY_PUSH_NEW(actor_t*, game->actors, actor)
        err = actor_init(actor, game->map, body, filename, NULL);
        if(err)return err;
    }
    return 0;
}

int hexgame_reset_player(hexgame_t *game, player_t *player, bool hard){
    body_t *body = player->body;
    if(hard){
        vec_cpy(game->map->space->dims, body->pos, game->map->spawn);
        body->rot = 0;
        body->turn = false;
    }else{
        vec_cpy(game->map->space->dims, body->pos, player->respawn_pos);
        body->rot = player->respawn_rot;
        body->turn = player->respawn_turn;
    }
    body->state = body->stateset.states[0];
    body->frame_i = 0;
    body->cooldown = 0;

    keyinfo_reset(&body->keyinfo);

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

int hexgame_load_recording(hexgame_t *game, const char *filename,
    int keymap, palettemapper_t *palmapper, bool loop
){
    int err;

    hexmap_t *map = game->map;

    ARRAY_PUSH_NEW(body_t*, map->bodies, body)
    err = body_init(body, game, map, NULL, NULL, palmapper);
    if(err)return err;

    err = recording_load(&body->recording, filename, body, loop);
    if(err)return err;

    err = body_play_recording(body);
    if(err)return err;

    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    int err;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_F6){
            game->camera.zoomout = true;
        }else if(event->key.keysym.sym == SDLK_F7){
            game->camera.follow = !game->camera.follow;
        }else if(event->key.keysym.sym == SDLK_F8){
            game->camera.smooth_scroll = !game->camera.smooth_scroll;
        }else if(event->key.keysym.sym == SDLK_F9){
            /* save recording */
            if(game->players_len >= 1){
                player_t *player = game->players[0];
                body_t *body = player->body;
                if(body->recording.action != 2){
                    fprintf(stderr,
                        "Can't stop recording without starting first! "
                        "(Try pressing 'R' before 'F9')\n");
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
                    "Maybe you need to record your first one with 'R'?\n");
            }else{
                fprintf(stderr, "Playing back from file: %s\n",
                    recording_filename);
                if(shift){
                    if(game->players_len < 1){
                        fprintf(stderr, "No player!\n");
                        return 2;}
                    player_t *player = game->players[0];
                    body_t *body = player->body;
                    err = body_load_recording(body, recording_filename,
                        true);
                    if(err)return err;
                    err = body_play_recording(body);
                    if(err)return err;
                }else{
                    err = hexgame_load_recording(game,
                        recording_filename, -1, NULL, true);
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
                body_t *body = game->players[0]->body;
                const char *recording_filename = get_next_recording_filename();
                fprintf(stderr, "Recording to file: %s "
                    " (When finished, press F9 to save!)\n",
                    recording_filename);
                err = body_start_recording(body, strdup(recording_filename));
                if(err)return err;
            }else{
                fprintf(stderr, "Can't start recording -- no player!\n");
            }
        }else if(!event->key.repeat){
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            if(event->key.keysym.sym == SDLK_1){
                hexgame_reset_player_by_keymap(game, 0, shift);
                game->camera.should_reset = true;
                    /* hack - we happen to know player 0 is always followed
                    by camera */
                camera_colors_flash_white(&game->camera, 30);
            }
            if(event->key.keysym.sym == SDLK_2){
                hexgame_reset_player_by_keymap(game, 1, shift);
                camera_colors_flash_white(&game->camera, 30);
            }
        }
    }else if(event->type == SDL_KEYUP){
        if(event->key.keysym.sym == SDLK_F6){
            game->camera.zoomout = false;
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

    /* Do 1 gameplay step for each actor */
    for(int i = 0; i < game->actors_len; i++){
        actor_t *actor = game->actors[i];
        err = actor_step(actor, game);
        if(err)return err;
    }

    /* Collide bodies with each other */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];
        if(body->state == NULL)continue;
        if(body->recording.action == 1){
            /* No hitboxes for bodies whose recording is playing */
            /* MAYBE TODO: These bodies should die too, but then their
            recording should restart after a brief pause.
            Maybe we can reuse body->cooldown for the pause. */
            continue;
        }
        hexcollmap_t *hitbox = body->state->hitbox;
        if(hitbox == NULL)continue;

        trf_t hitbox_trf;
        body_init_trf(body, &hitbox_trf);

        /* This body has a hitbox! So collide it against all other bodies'
        hitboxes. */
        for(int j = i + 1; j < map->bodies_len; j++){
            body_t *body_other = map->bodies[j];
            if(body_other->state == NULL)continue;
            hexcollmap_t *hitbox_other = body_other->state->hitbox;
            if(hitbox_other == NULL)continue;

            trf_t hitbox_other_trf;
            body_init_trf(body_other, &hitbox_other_trf);

            /* The other body has a hitbox! Do the collision... */
            bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                hitbox_other, &hitbox_other_trf, space, false);
            if(collide){
                /* There was a collision!
                Now we find out who was right... and who was dead. */

                /* Hardcoded "dead" state name... I suppose we could
                have a char* body->dead_anim_name, but whatever. */
                if(body->state->crushes){
                    err = body_set_state(body_other, "dead");
                    if(err)return err;
                }
                if(body_other->state->crushes){
                    err = body_set_state(body, "dead");
                    if(err)return err;
                }
            }
        }
    }

    /* Do 1 gameplay step for each body */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];
        err = body_step(body, game);
        if(err)return err;
    }

    /* Do 1 gameplay step for each player */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game);
        if(err)return err;
    }

    /* Do 1 step for the camera */
    err = camera_step(&game->camera);
    if(err)return err;

    return 0;
}

