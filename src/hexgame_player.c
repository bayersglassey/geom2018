

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
    /* Nuthin */
}

int player_init(player_t *player, body_t *body, int keymap,
    vec_t respawn_pos, rot_t respawn_rot, bool respawn_turn,
    char *respawn_filename
){
    int err;

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

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;
    if(respawn_pos == NULL)respawn_pos = map->spawn;
    vec_cpy(space->dims, player->respawn_pos, respawn_pos);
    player->respawn_rot = respawn_rot;
    player->respawn_turn = respawn_turn;
    player->respawn_filename = respawn_filename;

    /* Move player's body to the respawn location */
    vec_cpy(space->dims, body->pos, respawn_pos);
    body->rot = respawn_rot;
    body->turn = respawn_turn;

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
                vec_cpy(space->dims, player->respawn_pos, body->pos);
                player->respawn_rot = body->rot;
                player->respawn_turn = body->turn;
                if(player->respawn_filename != NULL){
                    FILE *f = fopen(player->respawn_filename, "w");
                    if(f != NULL){
                        fprintf(f, "%i %i %i %i\n",
                            player->respawn_pos[0], player->respawn_pos[1],
                            player->respawn_rot, player->respawn_turn);
                        fclose(f);
                    }
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

