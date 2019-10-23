

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
#include "prismelrenderer.h"
#include "array.h"
#include "util.h"
#include "write.h"




void player_cleanup(player_t *player){
    location_cleanup(&player->respawn_location);
    location_cleanup(&player->safe_location);
    free(player->respawn_filename);
}

int player_init(player_t *player, body_t *body, int keymap,
    vec_t respawn_pos, rot_t respawn_rot, bool respawn_turn,
    char *respawn_map_filename, char *respawn_filename
){
    int err;
    hexgame_t *game = body->game;

    player->body = body;
    player->keymap = keymap;

    for(int i = 0; i < KEYINFO_KEYS; i++)player->key_code[i] = 0;
    if(keymap == 0){
        player->key_code[KEYINFO_KEY_ACTION1] = SDLK_LSHIFT;
        player->key_code[KEYINFO_KEY_ACTION2] = SDLK_SPACE;
        player->key_code[KEYINFO_KEY_U] = SDLK_UP;
        player->key_code[KEYINFO_KEY_D] = SDLK_DOWN;
        player->key_code[KEYINFO_KEY_L] = SDLK_LEFT;
        player->key_code[KEYINFO_KEY_R] = SDLK_RIGHT;
    }else if(keymap == 1){
        player->key_code[KEYINFO_KEY_ACTION1] = SDLK_f;
        player->key_code[KEYINFO_KEY_ACTION2] = SDLK_g;
        player->key_code[KEYINFO_KEY_U] = SDLK_w;
        player->key_code[KEYINFO_KEY_D] = SDLK_s;
        player->key_code[KEYINFO_KEY_L] = SDLK_a;
        player->key_code[KEYINFO_KEY_R] = SDLK_d;
    }

    hexmap_t *map;
    err = hexgame_get_or_load_map(game, respawn_map_filename, &map);
    if(err)return err;
    vecspace_t *space = map->space;
    if(respawn_pos == NULL)respawn_pos = map->spawn;

    location_init(&player->respawn_location);
    location_set(&player->respawn_location, space,
        respawn_pos, respawn_rot, respawn_turn, respawn_map_filename);

    /* Locations own their map filenames, so need to strdup */
    char *jump_map_filename = strdup(respawn_map_filename);

    location_init(&player->safe_location);
    location_set(&player->safe_location, space,
        respawn_pos, respawn_rot, respawn_turn, jump_map_filename);

    player->respawn_filename = respawn_filename;

    /* Move player's body to the respawn location */
    err = body_respawn(body, respawn_pos, respawn_rot, respawn_turn, map);
    if(err)return err;

    return 0;
}

static int _player_set_location(player_t *player, location_t *location,
    vec_ptr_t pos, rot_t rot, bool turn, const char *map_filename
){
    hexgame_t *game = player->body->game;
    vecspace_t *space = game->space;

    /* Only assign new map_filename if it compares as different to
    the old one */
    char *new_map_filename = location->map_filename;
    if(strcmp(location->map_filename, map_filename)){
        new_map_filename = strdup(map_filename);
        if(!new_map_filename){
            perror("strdup");
            return 1;
        }
    }

    location_set(location, space, pos, rot, turn, new_map_filename);
    return 0;
}

int player_set_respawn(player_t *player,
    vec_ptr_t pos, rot_t rot, bool turn, const char *map_filename
){
    return _player_set_location(player, &player->respawn_location,
        pos, rot, turn, map_filename);
}

int player_set_safe_location(player_t *player,
    vec_ptr_t pos, rot_t rot, bool turn, const char *map_filename
){
    return _player_set_location(player, &player->safe_location,
        pos, rot, turn, map_filename);
}

int player_respawn_save(const char *filename, vec_t pos,
    rot_t rot, bool turn, const char *map_filename
){
    FILE *f = fopen(filename, "w");
    if(f == NULL){
        fprintf(stderr, "Couldn't save player to %s: ", filename);
        perror(NULL);
        return 2;
    }
    fprintf(f, "%i %i %i %c ", pos[0], pos[1], rot, turn? 'y': 'n');
    fus_write_str(f, map_filename);
    fclose(f);
    return 0;
}

int player_respawn_load(const char *filename, vec_t pos,
    rot_t *rot_ptr, bool *turn_ptr, char **map_filename_ptr
){
    int err = 0;

    char *text = load_file(filename);
    if(text == NULL){
        fprintf(stderr, "Couldn't load player from %s: ", filename);
        return 2;
    }

    fus_lexer_t lexer;
    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    int x, y;
    rot_t rot;
    bool turn;
    char *map_filename;

    err = fus_lexer_get_int(&lexer, &x);
    if(err)goto err;
    err = fus_lexer_get_int(&lexer, &y);
    if(err)goto err;
    err = fus_lexer_get_int(&lexer, &rot);
    if(err)goto err;
    err = fus_lexer_get_yn(&lexer, &turn);
    if(err)goto err;
    err = fus_lexer_get_str(&lexer, &map_filename);
    if(err)goto err;

    pos[0] = x;
    pos[1] = y;
    *rot_ptr = rot;
    *turn_ptr = turn;
    *map_filename_ptr = map_filename;

err:
    fus_lexer_cleanup(&lexer);
    free(text);
    return err;
}


int player_reload(player_t *player, bool *file_found_ptr){
    int err;
    vec_t respawn_pos;
    rot_t respawn_rot;
    bool respawn_turn;
    char *respawn_map_filename;

    /* Attempt to load file */
    err = player_respawn_load(player->respawn_filename,
        respawn_pos, &respawn_rot, &respawn_turn,
        &respawn_map_filename);
    bool file_found = err == 0;

    /* If we couldn't load it, that's not an "error" per se, we'll just
    report to caller that file wasn't found. */
    *file_found_ptr = file_found;
    if(!file_found)return 0;

    hexmap_t *respawn_map;
    err = hexgame_get_or_load_map(player->body->game, respawn_map_filename,
        &respawn_map);
    if(err)return err;

    err = body_respawn(player->body,
        respawn_pos, respawn_rot, respawn_turn, respawn_map);
    if(err)return err;

    return 0;
}


int player_process_event(player_t *player, SDL_Event *event){
    body_t *body = player->body;
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


static int player_use_door(player_t *player, hexmap_door_t *door){
    int err;

    body_t *body = player->body;
    hexmap_t *map = body->map;
    vecspace_t *space = map->space;
    hexgame_t *game = body->game;

    if(door->type == HEXMAP_DOOR_TYPE_NEW_GAME){
        err = hexgame_new_game(game, player, door->respawn_map_filename);
        if(err)return err;
    }else if(door->type == HEXMAP_DOOR_TYPE_CONTINUE){
        err = hexgame_continue(game, player);
        if(err)return err;
    }else if(door->type == HEXMAP_DOOR_TYPE_EXIT){
        err = hexgame_exit(game, player);
        if(err)return err;
    }else if(door->type == HEXMAP_DOOR_TYPE_RESPAWN){
        hexmap_t *new_map = body->map;
        if(door->respawn_map_filename != NULL){
            /* Switch map */
            err = hexgame_get_or_load_map(game,
                door->respawn_map_filename, &new_map);
            if(err)return err;
        }

        /* Respawn body */
        err = body_respawn(body,
            door->respawn_pos, door->respawn_rot, door->respawn_turn, new_map);
        if(err)return err;

        if(door->respawn_anim_filename != NULL){
            /* Switch anim (stateset) */

            /* HACK: If you've become something other than a spider,
            anim-changing doors change you back into a spider. */
            if(strcmp(body->stateset.filename, "anim/player.fus")){
                door->respawn_anim_filename = "anim/player.fus";
            }

            err = body_set_stateset(body, door->respawn_anim_filename, NULL);
            if(err)return err;
        }

        /* Flash screen cyan so player knows something happened */
        body_flash_cameras(body, 0, 255, 255, 60);
        body_reset_cameras(body);
    }
    return 0;
}

int player_step(player_t *player, hexgame_t *game){
    int err;

    body_t *body = player->body;

    /* Respawn body if player hit the right key while dead */
    if(body && (body->dead || body->out_of_bounds)
        && body->keyinfo.wasdown[KEYINFO_KEY_U]
    ){
        /* Soft reset */
        int reset_level =
            body->dead == BODY_ALL_DEAD? RESET_SOFT: RESET_TO_SAFETY;
        hexgame_reset_player(game, player, reset_level, NULL);

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
        err = player_set_safe_location(player, body->pos, body->rot,
            body->turn, map->name);
        if(err)return err;
    }

    /* Collide body against map, looking for special tiles like
    savepoints & doors */
    hexcollmap_t *hitbox = body->state->hitbox;
    if(hitbox != NULL){
        trf_t hitbox_trf;
        body_init_trf(body, &hitbox_trf);

        hexmap_collision_t collision;
        hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);

        hexmap_submap_t *savepoint_submap = collision.savepoint.submap;
        hexmap_submap_t *door_submap = collision.door.submap;
        hexmap_submap_t *water_submap = collision.water.submap;

        /* A SERIES OF WILD HACKS FOLLOW
        TODO: Replace them with The Real Thing, whatever that should be */
        if(savepoint_submap || door_submap){
            bool standing_flat = body->rot == 0;

            /* HACK so you can use doors as a roller...
            Often it looks like you have rot == 0 when it's really 2.
            NOTE: If you save with rot == 2 as a roller, and then respawn
            as a spider, you'll probably be in some weird position.
            TODO: Fix whatever needs to be fixed so we don't need this hack */
            if(!strcmp(body->stateset.filename, "anim/roller.fus")){
                standing_flat = body->rot == 0 || body->rot == 2;
            }

            if(!standing_flat){
                savepoint_submap = NULL;
                door_submap = NULL;
            }else{
                /* HACK: Only spiders can use savepoints */
                if(strcmp(body->stateset.filename, "anim/player.fus")){
                    savepoint_submap = NULL;}
            }
        }

        if(savepoint_submap){
            /* Don't use the savepoint if it's already our respawn point!
            In particular, I want to avoid screen flashing white if e.g.
            player turns around in-place.
            The reason we can't just check equality of body->pos and
            player->respawn_location.pos is that turning around actually affects
            body's pos (moves it by 1). */
            int dist = hexspace_dist(body->pos, player->respawn_location.pos);
            bool at_respawn = dist <= 1;

            if(!at_respawn){
                /* We're not at previous respawn location, so update it */
                err = player_set_respawn(player, body->pos, body->rot,
                    body->turn, map->name);
                if(err)return err;

                /* Save player's new respawn location */
                if(player->respawn_filename != NULL){
                    err = player_respawn_save(player->respawn_filename,
                        player->respawn_location.pos, player->respawn_location.rot,
                        player->respawn_location.turn,
                        player->respawn_location.map_filename);
                    if(err)return err;
                }

                /* Flash screen white so player knows something happened */
                body_flash_cameras(body, 255, 255, 255, 30);
            }
        }

        if(door_submap){
            hexmap_door_t *door = hexmap_submap_get_door(
                door_submap, collision.door.elem);
            if(door){
                err = player_use_door(player, door);
                if(err)return err;
            }
        }
    }

    return 0;
}

