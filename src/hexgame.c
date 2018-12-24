

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

int camera_init(camera_t *camera, hexgame_t *game, hexmap_t *map,
    body_t *body
){
    camera->game = game;
    camera->map = map;
    camera->cur_submap = NULL;
    camera->body = body;

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
    body_t *body = camera->body;

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
    ARRAY_FREE_PTR(hexmap_t*, game->maps, hexmap_cleanup)
    ARRAY_FREE_PTR(camera_t*, game->cameras, camera_cleanup)
    ARRAY_FREE_PTR(player_t*, game->players, player_cleanup)
    ARRAY_FREE_PTR(actor_t*, game->actors, actor_cleanup)
}

int hexgame_init(hexgame_t *game, prismelrenderer_t *prend,
    const char *map_filename
){
    int err;

    game->frame_i = 0;
    game->prend = prend;
    game->space = &hexspace; /* NOT the same as prend->space! */

    ARRAY_INIT(game->maps)
    ARRAY_INIT(game->cameras)
    ARRAY_INIT(game->players)
    ARRAY_INIT(game->actors)

    hexmap_t *map;
    err = hexgame_load_map(game, map_filename, &map);
    if(err)return err;

    return 0;
}

int hexgame_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr
){
    int err;
    ARRAY_PUSH_NEW(hexmap_t*, game->maps, map)
    err = hexmap_load(map, game, map_filename);
    if(err)return err;
    *map_ptr = map;
    return 0;
}

int hexgame_get_or_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr
){
    int err;

    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        if(!strcmp(map->name, map_filename)){
            *map_ptr = map;
            return 0;
        }
    }

    return hexgame_load_map(game, map_filename, map_ptr);
}

int hexgame_reset_player(hexgame_t *game, player_t *player, bool hard){
    /* WARNING: this function calls body_respawn, which calls
    body_move_to_map, which modifies map->bodies for two maps.
    So if caller is trying to loop over map->bodies in the usual way,
    the behaviour of that loop is probably gonna be super wrong. */

    int err;
    vecspace_t *space = game->space;
    body_t *body = player->body;
    hexmap_t *map = NULL;
    vec_ptr_t pos = NULL;
    rot_t rot = 0;
    bool turn = false;

    if(hard){
        map = game->maps[0];
        pos = map->spawn;
    }else{
        err = hexgame_get_or_load_map(game, player->respawn_map_filename,
            &map);
        if(err)return err;
        pos = player->respawn_pos;
        rot = player->respawn_rot;
        turn = player->respawn_turn;
    }

    err = body_respawn(body, pos, rot, turn, map);
    if(err)return err;

    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        if(camera->body == body){
            camera->should_reset = true;
            camera_colors_flash_white(camera, 30);
        }
    }

    return 0;
}

int hexgame_reset_players_by_keymap(hexgame_t *game, int keymap, bool hard){
    int err;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->keymap == keymap){
            err = hexgame_reset_player(game, player, hard);
            if(err)return err;
        }
    }

    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    int err;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_F9){
            /* save recording */
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap != 0)continue;

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
                for(int i = 0; i < game->players_len; i++){
                    player_t *player = game->players[i];
                    if(player->keymap != 0)continue;

                    if(shift){
                        body_t *body = player->body;
                        err = body_load_recording(body, recording_filename,
                            true);
                        if(err)return err;
                        err = body_play_recording(body);
                        if(err)return err;
                    }else{
                        /* TODO: Recordings need to state which map they
                        expect! The following is a hack: you must play
                        recordings belonging to your correct map... */
                        body_t *body = player->body;
                        err = hexmap_load_recording(body->map,
                            recording_filename, NULL, true);
                        if(err)return err;
                    }
                }
            }
#ifdef GEOM_HEXGAME_DEBUG_MALLOC
        }else if(event->key.keysym.sym == SDLK_F11){
            malloc_stats();
#endif
        }else if(event->key.keysym.sym == SDLK_F12){
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap < 0)continue;

                body_t *body = player->body;
                hexmap_t *map = body->map;
                hexmap_submap_t *submap = body->cur_submap;
                fprintf(stderr, "Player %i submap: %s\n",
                    player->keymap, submap->filename);
            }
        }else if(event->key.keysym.sym == SDLK_r){
            /* start recording */
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                if(player->keymap != 0)continue;

                body_t *body = player->body;
                const char *recording_filename = get_next_recording_filename();
                fprintf(stderr, "Recording to file: %s "
                    " (When finished, press F9 to save!)\n",
                    recording_filename);
                err = body_start_recording(body, strdup(recording_filename));
                if(err)return err;
            }
        }else if(!event->key.repeat){
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            if(event->key.keysym.sym == SDLK_1){
                hexgame_reset_players_by_keymap(game, 0, shift);}
            if(event->key.keysym.sym == SDLK_2){
                hexgame_reset_players_by_keymap(game, 1, shift);}
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

    vecspace_t *space = game->space;

    /* Do 1 gameplay step for each actor */
    for(int i = 0; i < game->actors_len; i++){
        actor_t *actor = game->actors[i];
        err = actor_step(actor, game);
        if(err)return err;
    }

    /* Do 1 gameplay step for each map */
    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        err = hexmap_step(map);
        if(err)return err;
    }

    /* Do 1 gameplay step for each player */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game);
        if(err)return err;
    }

    /* Do 1 step for each camera */
    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        err = camera_step(camera);
        if(err)return err;
    }

    return 0;
}

