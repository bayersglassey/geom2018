

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
    free(player->respawn_map_filename);
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
        player->key_code[KEYINFO_KEY_ACTION] = SDLK_SPACE;
        player->key_code[KEYINFO_KEY_U] = SDLK_UP;
        player->key_code[KEYINFO_KEY_D] = SDLK_DOWN;
        player->key_code[KEYINFO_KEY_L] = SDLK_LEFT;
        player->key_code[KEYINFO_KEY_R] = SDLK_RIGHT;
    }else if(keymap == 1){
        player->key_code[KEYINFO_KEY_ACTION] = SDLK_f;
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
    vec_cpy(space->dims, player->respawn_pos, respawn_pos);
    player->respawn_rot = respawn_rot;
    player->respawn_turn = respawn_turn;
    player->respawn_map_filename = respawn_map_filename;
    player->respawn_filename = respawn_filename;

    /* Move player's body to the respawn location */
    err = body_respawn(body, respawn_pos, respawn_rot, respawn_turn, map);
    if(err)return err;

    return 0;
}

int player_set_respawn(player_t *player, vec_ptr_t pos, rot_t rot, bool turn,
    const char *map_filename
){
    hexgame_t *game = player->body->game;
    vecspace_t *space = game->space;

    vec_cpy(space->dims, player->respawn_pos, pos);
    player->respawn_rot = rot;
    player->respawn_turn = turn;
    if(!strcmp(player->respawn_map_filename, map_filename)){
        free(player->respawn_map_filename);
        player->respawn_map_filename = strdup(map_filename);
    }
    return 0;
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
    err = fus_lexer_get_bool(&lexer, &turn);
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



int player_step(player_t *player, hexgame_t *game){
    int err;

    body_t *body = player->body;
    if(body->state == NULL){
        return 0;}

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    /* Collide body against map, looking for special tiles like
    savepoints & doors */
    hexcollmap_t *hitbox = body->state->hitbox;
    if(hitbox != NULL){
        trf_t hitbox_trf;
        body_init_trf(body, &hitbox_trf);

        bool collide_savepoint = false;
        bool collide_door = false;
        hexmap_collide_special(map, hitbox, &hitbox_trf,
            &collide_savepoint, &collide_door);

        if(collide_savepoint && body->rot == 0){

            /* Don't use the savepoint if it's already our respawn point!
            In particular, I want to avoid screen flashing white if e.g.
            player turns around in-place.
            The reason we can't just check equality of body->pos and
            player->respawn_pos is that turning around actually affects
            body's pos (moves it by 1). */
            int dist = hexspace_dist(body->pos, player->respawn_pos);
            bool at_respawn = dist <= 1;

            if(!at_respawn){
                /* We're not at previous respawn location, so update it */
                err = player_set_respawn(player, body->pos, body->rot,
                    body->turn, map->name);
                if(err)return err;

                /* Save player's new respawn location */
                if(player->respawn_filename != NULL){
                    err = player_respawn_save(player->respawn_filename,
                        player->respawn_pos, player->respawn_rot,
                        player->respawn_turn,
                        player->respawn_map_filename);
                    if(err)return err;
                }

                for(int i = 0; i < game->cameras_len; i++){
                    camera_t *camera = game->cameras[i];
                    if(camera->body != body)continue;

                    /* Flash screen white so player knows something
                    happened */
                    camera_colors_flash_white(camera, 30);
                }
            }
        }

        if(collide_door){
            /* Switch hexmaps... */
        }
    }

    return 0;
}

