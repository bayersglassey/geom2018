

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
#include "lexer.h"
#include "lexer_macros.h"
#include "var_utils.h"
#include "hexgame_vars_props.h"


static void print_tabs(FILE *file, int depth){
    for(int i = 0; i < depth; i++)fprintf(file, "  ");
}



void player_cleanup(player_t *player){
    hexgame_savelocation_cleanup(&player->respawn_location);
    hexgame_savelocation_cleanup(&player->safe_location);
}

int player_init(player_t *player, hexgame_t *game, int keymap,
    vec_t respawn_pos, rot_t respawn_rot, bool respawn_turn,
    const char *respawn_map_filename, const char *respawn_filename
){
    int err;

    player->game = game;
    player->keymap = keymap;
    player->body = NULL;

    for(int i = 0; i < KEYINFO_KEYS; i++)player->key_code[i] = 0;
    if(keymap == 0){
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

    hexmap_t *map;
    err = hexgame_get_or_load_map(game, respawn_map_filename, &map);
    if(err)return err;
    vecspace_t *space = map->space;
    if(respawn_pos == NULL){
        hexgame_location_t *spawn = &map->spawn;
        respawn_pos = spawn->pos;
        respawn_rot = spawn->rot;
        respawn_turn = spawn->turn;
    }

    hexgame_savelocation_init(&player->respawn_location);
    hexgame_savelocation_set(&player->respawn_location, space,
        respawn_pos, respawn_rot, respawn_turn, respawn_map_filename,
        NULL, NULL);

    hexgame_savelocation_init(&player->safe_location);
    hexgame_savelocation_set(&player->safe_location, space,
        respawn_pos, respawn_rot, respawn_turn, respawn_map_filename,
        NULL, NULL);

    player->respawn_filename = respawn_filename;

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

void hexgame_player_dump(player_t *player, int depth){
    print_tabs(stderr, depth);
    if(player->keymap >= 0)fprintf(stderr, "Player %i\n", player->keymap);
    else fprintf(stderr, "CPU Player\n");

    print_tabs(stderr, depth);
    fprintf(stderr, "index: %i\n", player_get_index(player));

    body_t *body = player->body;
    if(body){
        print_tabs(stderr, depth);
        fprintf(stderr, "body:\n");
        hexgame_body_dump(body, depth + 1);
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
        body->loc.pos, body->loc.rot, body->loc.turn, body->map->name,
        body->stateset.filename, body->state->name);
}

static int player_set_safe_location(player_t *player){
    body_t *body = player->body;
    return _player_set_location(player, &player->safe_location,
        body->loc.pos, body->loc.rot, body->loc.turn, body->map->name,
        body->stateset.filename, body->state->name);
}

int player_reload(player_t *player){
    /* Reload player's location from file */
    int err;

    if(!player->body){
        fprintf(stderr, "%s: no body\n", __func__);
        return 2;
    }

    /* Attempt to load file */
    bool file_not_found = false;
    err = player_load(player, player->respawn_filename, &file_not_found);
    if(err && !file_not_found)return err;

    /* If save file doesn't exist, just reset player.
    (This is what happens if you press "1" at start of game.) */
    if(file_not_found){
        return hexgame_reset_player(player->game, player, RESET_SOFT, NULL);
    }

    return player_reload_from_location(player, &player->respawn_location);
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


static int player_use_door(player_t *player, hexmap_door_t *door){
    int err;

    body_t *body = player->body;
    hexmap_t *map = body->map;
    vecspace_t *space = map->space;
    hexgame_t *game = body->game;

    if(door->type == HEXMAP_DOOR_TYPE_NEW_GAME){
        err = game->new_game_callback(game, player,
            door->u.location.map_filename);
        if(err)return err;
    }else if(door->type == HEXMAP_DOOR_TYPE_CONTINUE){
        err = game->continue_callback(game, player);
        if(err)return err;
    }else if(door->type == HEXMAP_DOOR_TYPE_PLAYERS){
        err = game->set_players_callback(game, player,
            door->u.n_players);
        if(err)return err;
    }else if(door->type == HEXMAP_DOOR_TYPE_EXIT){
        err = game->exit_callback(game, player);
        if(err)return err;
    }else if(door->type == HEXMAP_DOOR_TYPE_CAMERA_MAPPER){
        prismelmapper_t *mapper = prismelrenderer_get_mapper(game->prend,
            door->u.mapper_name);
        if(mapper == NULL){
            fprintf(stderr, "%s: Couldn't find camera mapper: %s\n",
                __func__, door->u.mapper_name);
            return 2;
        }
        FOREACH_BODY_CAMERA(body, camera, {
            camera->mapper = mapper;
        })
    }else if(door->type == HEXMAP_DOOR_TYPE_RESPAWN){
        err = body_relocate(body, door->u.location.map_filename,
            &door->u.location.loc, door->u.location.stateset_filename,
            door->u.location.state_name);
        if(err)return err;
    }
    return 0;
}

int player_save(player_t *player, const char *filename){
    int err;

    FILE *file = fopen(filename, "w");
    if(file == NULL){
        fprintf(stderr, "Couldn't save game to %s: ", filename);
        perror(NULL);
        return 2;
    }

    hexgame_savelocation_t *location = &player->respawn_location;
    hexgame_t *game = player->game;

    hexgame_savelocation_write(location, file);
    fputc('\n', file);

    fprintf(file, "vars:\n");
    vars_write(&game->vars, file, 4*1);

    fprintf(file, "maps:\n");
    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        fprintf(file, "    ");
        fus_write_str(file, map->name);
        fprintf(file, ":\n");
        fprintf(file, "        vars:\n");

        /* Don't write the vars which say they shouldn't be saved. */
        var_props_t nowrite_props_mask = 1 << HEXGAME_VARS_PROP_NOSAVE;
        vars_write_with_mask(&map->vars, file, 4*3, nowrite_props_mask);
    }

    fclose(file);

#   ifdef __EMSCRIPTEN__
    /* Needed to actually save the IDBFS data to browser's indexedDb. */
    emccdemo_syncfs();
#   endif

    return 0;
}

int player_load(player_t *player, const char *filename,
    bool *file_not_found_ptr
){
    int err;

    char *text = load_file(filename);
    if(text == NULL){
        fprintf(stderr, "Couldn't load game from %s\n", filename);

        /* Indicate to caller that the reason for returning a failure code
        is a missing file.
        So caller can choose to treat that situation as something other
        than an outright error. */
        *file_not_found_ptr = true;

        return 2;
    }

    fus_lexer_t _lexer, *lexer=&_lexer;
    err = fus_lexer_init(lexer, text, filename);
    if(err)return err;

    hexgame_t *game = player->game;
    prismelrenderer_t *prend = game->prend;

    err = hexgame_savelocation_parse(&player->respawn_location, lexer,
        &prend->filename_store, &prend->name_store);
    if(err)return err;

    if(GOT("vars")){
        NEXT
        OPEN
        err = vars_parse(&game->vars, lexer);
        if(err)return err;
        CLOSE
    }

    if(GOT("maps")){
        NEXT
        OPEN
        while(1){
            if(GOT(")"))break;
            const char *name;
            GET_STR_CACHED(name, &prend->name_store)

            hexmap_t *map;
            err = hexgame_get_or_load_map(game, name, &map);
            if(err)return err;

            OPEN
            {
                if(GOT("vars")){
                    NEXT
                    OPEN
                    err = vars_parse(&map->vars, lexer);
                    if(err)return err;
                    CLOSE
                }
            }
            CLOSE
        }
        NEXT
    }

    fus_lexer_cleanup(lexer);
    free(text);
    return 0;
}

int player_use_savepoint(player_t *player){
    int err;

    /* Update respawn location */
    err = player_set_respawn(player);
    if(err)return err;

    /* Save player's new respawn location */
    if(player->respawn_filename != NULL){
        player_save(player, player->respawn_filename);
    }

    /* Flash screen white so player knows something happened */
    body_flash_cameras(player->body, 255, 255, 255, 30);

    return 0;
}

int player_step(player_t *player, hexgame_t *game){
    int err;

    body_t *body = player->body;

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
    savepoints & doors */
    hexcollmap_t *hitbox = body->state->hitbox;
    if(hitbox != NULL){
        trf_t hitbox_trf;
        hexgame_location_init_trf(&body->loc, &hitbox_trf);

        hexmap_collision_t collision;
        err = hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);
        if(err)return err;

        hexmap_submap_t *savepoint_submap = collision.savepoint.submap;
        hexmap_submap_t *door_submap = collision.door.submap;
        hexmap_submap_t *water_submap = collision.water.submap;

        if(!body->state->safe){
            /* Don't save in an unsafe position, like flying through the air */
            savepoint_submap = NULL;
        }

        if(savepoint_submap){
            /* Don't use the savepoint if it's already our respawn point!
            In particular, I want to avoid screen flashing white if e.g.
            player turns around in-place.
            HACK: we check distance between body's current position and
            player's respawn position. */
            int dist = hexspace_dist(body->loc.pos, player->respawn_location.loc.pos);
            bool at_respawn = dist <= 2;
            if(at_respawn)savepoint_submap = NULL;
        }

        if(savepoint_submap){
            err = player_use_savepoint(player);
            if(err)return err;
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

