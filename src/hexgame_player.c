

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "array.h"


static void print_tabs(FILE *file, int depth){
    for(int i = 0; i < depth; i++)fprintf(file, "  ");
}



void player_cleanup(player_t *player){
    hexgame_savelocation_cleanup(&player->respawn_location);
    hexgame_savelocation_cleanup(&player->safe_location);
}

int player_init(player_t *player, hexgame_t *game, int keymap,
    hexgame_savelocation_t *respawn_location
){
    int err;

    player->game = game;
    player->keymap = keymap;
    player->body = NULL;
    player->savepoint_cooldown = 0;

    for(int i = 0; i < KEYINFO_KEYS; i++)player->key_code[i] = 0;
    if(keymap == HEXGAME_PLAYER_0){
        player->key_code[KEYINFO_KEY_ACTION1] = SDLK_RSHIFT;
        player->key_code[KEYINFO_KEY_ACTION2] = SDLK_SPACE;
        player->key_code[KEYINFO_KEY_U] = SDLK_UP;
        player->key_code[KEYINFO_KEY_D] = SDLK_DOWN;
        player->key_code[KEYINFO_KEY_L] = SDLK_LEFT;
        player->key_code[KEYINFO_KEY_R] = SDLK_RIGHT;
    }else if(keymap == 1){
        player->key_code[KEYINFO_KEY_ACTION1] = SDLK_LSHIFT;
        player->key_code[KEYINFO_KEY_ACTION2] = SDLK_f;
        player->key_code[KEYINFO_KEY_U] = SDLK_w;
        player->key_code[KEYINFO_KEY_D] = SDLK_s;
        player->key_code[KEYINFO_KEY_L] = SDLK_a;
        player->key_code[KEYINFO_KEY_R] = SDLK_d;
    }

    player->respawn_location = *respawn_location;
    player->safe_location = *respawn_location;
    return 0;
}

int player_get_index(player_t *player){
    hexgame_t *game = player->game;
    for(int i = 0; i < game->players_len; i++){
        player_t *_player = game->players[i];
        if(player == _player)return i;
    }
    return -1;
}

void player_dump(player_t *player, int depth){
    print_tabs(stderr, depth);
    if(player->keymap >= 0)fprintf(stderr, "Player %i\n", player->keymap);
    else fprintf(stderr, "CPU Player\n");

    print_tabs(stderr, depth);
    fprintf(stderr, "index: %i\n", player_get_index(player));

    body_t *body = player->body;
    if(body){
        print_tabs(stderr, depth);
        fprintf(stderr, "body:\n");
        body_dump(body, depth + 1);
    }else{
        print_tabs(stderr, depth);
        fprintf(stderr, "no body!\n");
    }
}

static int _player_set_location(player_t *player, hexgame_savelocation_t *location,
    vec_ptr_t pos, rot_t rot, bool turn, const char *map_filename,
    const char *stateset_filename, const char *state_name
){
    hexgame_t *game = player->game;
    vecspace_t *space = game->space;
    hexgame_savelocation_set(location, space, pos, rot, turn, map_filename,
        stateset_filename, state_name);
    return 0;
}

static int player_set_respawn(player_t *player){
    body_t *body = player->body;
    return _player_set_location(player, &player->respawn_location,
        body->loc.pos, body->loc.rot, body->loc.turn, body->map->filename,
        body->stateset->filename, body->state->name);
}

static int player_set_safe_location(player_t *player){
    body_t *body = player->body;
    return _player_set_location(player, &player->safe_location,
        body->loc.pos, body->loc.rot, body->loc.turn, body->map->filename,
        body->stateset->filename, body->state->name);
}

int player_spawn_body(player_t *player){
    int err;

    if(player->body != NULL){
        fprintf(stderr, "Player %i already has a body\n",
            player_get_index(player));
        return 2;
    }

    hexgame_savelocation_t *respawn = &player->respawn_location;

    hexmap_t *respawn_map;
    err = hexgame_get_or_load_map(player->game,
        respawn->map_filename, &respawn_map);
    if(err)return err;

    /* Set player->body */
    ARRAY_PUSH_NEW(body_t*, respawn_map->bodies, body)
    err = body_init(body, player->game, respawn_map,
        respawn->stateset_filename,
        respawn->state_name,
        NULL);
    if(err)return err;

    /* Attach body to player */
    player->body = body;

    /* Move body to the respawn location */
    body->loc = respawn->loc;

    return 0;
}

int player_reload_from_location(player_t *player,
    hexgame_savelocation_t *location
){
    int err;

    if(player->body == NULL){
        fprintf(stderr, "player_reload_from_location: player has no body\n");
        return 2;
    }

    hexmap_t *respawn_map;
    err = hexgame_get_or_load_map(player->game,
        location->map_filename, &respawn_map);
    if(err)return err;

    err = body_respawn(player->body,
        location->loc.pos, location->loc.rot, location->loc.turn,
        respawn_map);
    if(err)return err;

    return body_set_stateset(player->body,
        location->stateset_filename, location->state_name);
}


int player_process_event(player_t *player, SDL_Event *event){
    body_t *body = player->body;

    /* If no body, do nothing! */
    if(!body)return 0;

    if(event->type == SDL_KEYDOWN || event->type == SDL_KEYUP){
        if(!event->key.repeat){
            for(int i = 0; i < KEYINFO_KEYS; i++){
                if(event->key.keysym.sym == player->key_code[i]){
                    if(event->type == SDL_KEYDOWN){
                        body_keydown(body, i);
                    }else{
                        body_keyup(body, i);
                    }
                }
            }
        }
    }
    return 0;
}


int player_use_savepoint(player_t *player){
    int err;

    /* Update respawn location */
    err = player_set_respawn(player);
    if(err)return err;

    /* Save player's new respawn location */
    err = player->game->save_callback(player->game);
    if(err)return err;

    hexgame_t *game = player->game;

    /* Flash screen white so player knows something happened */
    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        camera_colors_flash(camera, 255, 255, 255, 30);
    }

    return 0;
}

int player_step(player_t *player, hexgame_t *game){
    int err;

    body_t *body = player->body;

    if(player->savepoint_cooldown > 0)player->savepoint_cooldown--;

    /* If no body, do nothing */
    if(!body)return 0;

    /* Respawn body if player hit the right key while dead */
    if(
        body_is_done_for(body) &&
        body->keyinfo.wasdown[KEYINFO_KEY_U]
    ){
        /* Soft reset */
        int reset_level =
            body->dead == BODY_ALL_DEAD? RESET_SOFT: RESET_TO_SAFETY;
        err = hexgame_reset_player(game, player, reset_level, NULL);
        if(err)return err;

        /* Player may have gotten a new body object */
        body = player->body;
    }

    if(body->state == NULL){
        return 0;}

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    if(body->state->safe){
        /* We're safe (e.g. not jumping), so update our jump location
        (where we'll be respawned if we do jump and hit something) */
        err = player_set_safe_location(player);
        if(err)return err;
    }

    /* Collide body against map, looking for special tiles like
    savepoints & water */
    hexcollmap_t *hitbox = body->state->hitbox;
    if(hitbox != NULL){
        trf_t hitbox_trf;
        hexgame_location_init_trf(&body->loc, &hitbox_trf);

        hexmap_collision_t collision;
        err = hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);
        if(err)return err;

        hexmap_submap_t *savepoint_submap = collision.savepoint.submap;
        hexmap_submap_t *minimap_submap = collision.mappoint.submap;
        hexmap_submap_t *water_submap = collision.water.submap;

        bool touching_savepoint =
            (savepoint_submap != NULL || minimap_submap != NULL) &&

            /* Don't save in an unsafe position, like flying through
            the air */
            body->state->safe;

        bool use_savepoint =
            touching_savepoint &&

            /* Don't save if we just did not too long ago */
            player->savepoint_cooldown == 0;

        if(touching_savepoint){
            /* Set a cooldown until we can use the savepoint again */
            player->savepoint_cooldown = PLAYER_SAVEPOINT_COOLDOWN;
        }

        if(use_savepoint){
            err = player_use_savepoint(player);
            if(err)return err;
        }
    }

    return 0;
}

