

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


#define DEBUG_RULES false

#define MAX_FRAME_I 554400
    /* Largest highly composite number smaller than 2^16 */


/**********
 * PLAYER *
 **********/

void player_cleanup(player_t *player){
    stateset_cleanup(&player->stateset);
}

int player_init(player_t *player, prismelrenderer_t *prend,
    char *stateset_filename, int keymap, vec_t respawn_pos
){
    int err;

    for(int i = 0; i < PLAYER_KEYS; i++)player->key_code[i] = 0;
    if(keymap == 0){
        player->key_code[PLAYER_KEY_U] = SDLK_UP;
        player->key_code[PLAYER_KEY_D] = SDLK_DOWN;
        player->key_code[PLAYER_KEY_L] = SDLK_LEFT;
        player->key_code[PLAYER_KEY_R] = SDLK_RIGHT;
    }else if(keymap == 1){
        player->key_code[PLAYER_KEY_U] = SDLK_w;
        player->key_code[PLAYER_KEY_D] = SDLK_s;
        player->key_code[PLAYER_KEY_L] = SDLK_a;
        player->key_code[PLAYER_KEY_R] = SDLK_d;
    }

    err = stateset_load(&player->stateset, stateset_filename,
        prend, &hexspace);
    if(err)return err;

    player->state = player->stateset.states[0];
    player->frame_i = 0;
    player->cooldown = 0;
    vec_cpy(hexspace.dims, player->respawn_pos, respawn_pos);
    vec_cpy(hexspace.dims, player->pos, respawn_pos);
    return 0;
}

rot_t player_get_rot(player_t *player, const vecspace_t *space){
    rot_t rot = player->rot;
    if(player->turn){
        rot = rot_contain(space->rot_max,
            space->rot_max/2 - rot);}
    return rot;
}

static int player_match_rule(player_t *player, hexgame_t *game,
    state_rule_t *rule, bool *rule_matched_ptr
){
    const static vecspace_t *space = &hexspace;

    bool rule_matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        if(DEBUG_RULES)printf("  if: %s\n", cond->type);
        if(cond->type == state_cond_type_false){
            rule_matched = false;
        }else if(cond->type == state_cond_type_key){

            int kstate_i = cond->u.key.kstate;
            bool *kstate =
                kstate_i == 0? player->key_isdown:
                kstate_i == 1? player->key_wasdown:
                kstate_i == 2? player->key_wentdown:
                NULL;
            if(kstate == NULL){
                fprintf(stderr, "kstate out of range: %i", kstate_i);
                return 2;}

            char c = cond->u.key.c;
            int key_i =
                c == 'u'? PLAYER_KEY_U:
                c == 'd'? PLAYER_KEY_D:
                c == 'l'? PLAYER_KEY_L:
                c == 'r'? PLAYER_KEY_R:
                c == 'f'? (player->turn? PLAYER_KEY_L: PLAYER_KEY_R):
                c == 'b'? (player->turn? PLAYER_KEY_R: PLAYER_KEY_L):
                -1;
            if(key_i == -1){
                fprintf(stderr, "Unrecognized key char: %c", c);
                return 2;}

            rule_matched = kstate[key_i];
            if(!cond->u.key.yes)rule_matched = !rule_matched;
        }else if(cond->type == state_cond_type_coll){
            trf_t trf;
            vec_cpy(space->dims, trf.add, player->pos);
            trf.rot = player_get_rot(player, space);
            trf.flip = player->turn;

            int flags = cond->u.coll.flags;
            bool all = flags & 1;
            bool yes = flags & 2;

            bool collide = hexmap_collide(game->map,
                cond->u.coll.collmap, &trf, yes? all: !all);
            rule_matched = yes? collide: !collide;
        }else{
            fprintf(stderr, "Unrecognized state rule condition: %s\n",
                cond->type);
            return 2;
        }
        if(!rule_matched)break;
    }

    if(DEBUG_RULES && !rule_matched)printf("    NO MATCH\n");

    *rule_matched_ptr = rule_matched;
    return 0;
}

static int player_apply_rule(player_t *player, hexgame_t *game,
    state_rule_t *rule
){
    const static vecspace_t *space = &hexspace;
    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(DEBUG_RULES)printf("  then: %s\n", effect->type);
        if(effect->type == state_effect_type_print){
            printf("player %p says: %s\n", player, effect->u.msg);
        }else if(effect->type == state_effect_type_move){
            vec_t vec;
            vec_cpy(space->dims, vec, effect->u.vec);
            rot_t rot = player_get_rot(player, space);
            space->vec_flip(vec, player->turn);
            space->vec_rot(vec, rot);
            vec_add(space->dims, player->pos, vec);
        }else if(effect->type == state_effect_type_rot){
            rot_t effect_rot = effect->u.rot;
            player->rot = rot_rot(space->rot_max,
                player->rot, effect_rot);
        }else if(effect->type == state_effect_type_turn){
            player->turn = !player->turn;
            player->rot = rot_flip(space->rot_max, player->rot, true);
        }else if(effect->type == state_effect_type_goto){
            state_t *state = stateset_get_state(&player->stateset,
                effect->u.goto_name);
            if(state == NULL){
                fprintf(stderr, "Unrecognized player state: %s\n",
                    effect->u.goto_name);
                return 2;
            }
            player->state = state;
            player->frame_i = 0;
        }else if(effect->type == state_effect_type_delay){
            player->cooldown = effect->u.delay;
        }else{
            fprintf(stderr, "Unrecognized state rule effect: %s\n",
                effect->type);
            return 2;
        }
    }
    return 0;
}

int player_step(player_t *player, hexgame_t *game){
    int err;

    player->frame_i++;
    if(player->frame_i == MAX_FRAME_I)player->frame_i = 0;

    if(player->cooldown > 0){
        player->cooldown--;
    }else{

        state_t *state = player->state;
        for(int i = 0; i < state->rules_len; i++){
            state_rule_t *rule = state->rules[i];

            if(DEBUG_RULES)printf("player %p rule %i:\n", player, i);

            bool rule_matched;
            err = player_match_rule(player, game, rule, &rule_matched);
            if(err)return err;

            if(rule_matched){
                err = player_apply_rule(player, game, rule);
                if(err)return err;
                break;
            }
        }

        /* start of new frame, no keys have gone down yet. */
        for(int i = 0; i < PLAYER_KEYS; i++){
            player->key_wasdown[i] = player->key_isdown[i];
            player->key_wentdown[i] = false;}
    }
    return 0;
}


/***********
 * HEXGAME *
 ***********/


void hexgame_cleanup(hexgame_t *game){
    ARRAY_FREE(player_t, *game, players, player_cleanup)
}

int hexgame_init(hexgame_t *game, hexmap_t *map, char *respawn_filename){
    game->frame_i = 0;
    game->zoomout = false;
    game->follow = false;
    game->map = map;
    game->respawn_filename = respawn_filename;
    vec_zero(map->space->dims, game->camera_pos);
    game->camera_rot = 0;
    ARRAY_INIT(*game, players)
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
    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = true;}
        if(event->key.keysym.sym == SDLK_F7){
            game->follow = true;}
        if(!event->key.repeat){
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            if(event->key.keysym.sym == SDLK_1
                && game->players_len >= 1){
                    hexgame_reset_player(game, game->players[0], shift);}
            if(event->key.keysym.sym == SDLK_2
                && game->players_len >= 2){
                    hexgame_reset_player(game, game->players[1], shift);}
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                for(int i = 0; i < PLAYER_KEYS; i++){
                    if(event->key.keysym.sym == player->key_code[i]){
                        player->key_isdown[i] = true;
                        player->key_wasdown[i] = true;
                        player->key_wentdown[i] = true;}}}}
    }else if(event->type == SDL_KEYUP){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = false;}
        if(event->key.keysym.sym == SDLK_F7){
            game->follow = false;}
        if(!event->key.repeat){ /* ??? */
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                for(int i = 0; i < PLAYER_KEYS; i++){
                    if(event->key.keysym.sym == player->key_code[i]){
                        player->key_isdown[i] = false;}}}}
    }
    return 0;
}

int hexgame_step(hexgame_t *game){
    int err;

    game->frame_i++;
    if(game->frame_i == MAX_FRAME_I)game->frame_i = 0;

    hexmap_t *map = game->map;
    vecspace_t *space = map->space;

    /* Figure out current submap */
    if(game->players_len >= 1){
        player_t *player = game->players[0];

        bool collide = false;
        for(int i = 0; i < map->submaps_len; i++){
            hexmap_submap_t *submap = map->submaps[i];
            hexcollmap_t *collmap = &submap->collmap;

            trf_t index = {0};
            hexspace_set(index.add,
                 player->pos[0] - submap->pos[0],
                -player->pos[1] + submap->pos[1]);

            /* savepoints are currently this HACK */
            hexcollmap_elem_t *face =
                hexcollmap_get_face(collmap, &index);
            if(face != NULL && face->tile_c == 'S'){
                if(!vec_eq(space->dims, player->respawn_pos, player->pos)){
                    vec_cpy(space->dims, player->respawn_pos, player->pos);
                    if(game->respawn_filename != NULL){
                        FILE *f = fopen(game->respawn_filename, "w");
                        if(f != NULL){
                            fprintf(f, "%i %i\n",
                                player->pos[0], player->pos[1]);
                            fclose(f);
                        }
                    }
                }
            }

            hexcollmap_elem_t *vert =
                hexcollmap_get_vert(collmap, &index);
            if(hexcollmap_elem_is_solid(vert)){
                game->cur_submap = submap;
                break;
            }
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

    /* Do 1 gameplay step for each player */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game);
        if(err)return err;
    }

    return 0;
}

int hexgame_render(hexgame_t *game, SDL_Renderer *renderer,
    SDL_Palette *pal, int x0, int y0, int zoom
){
    int err;

    vecspace_t *space = &hexspace;

    hexmap_t *map = game->map;

    vec_t camera_renderpos;
    vec4_vec_from_hexspace(camera_renderpos, game->camera_pos);

    prismelmapper_t *mapper = NULL;

    hexmap_submap_t *submap = game->cur_submap;
    if(submap != NULL){
        if(!game->zoomout)mapper = submap->mapper;
    }

    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        rendergraph_t *rgraph = submap->rgraph_map;

        vec_t pos;
        vec4_vec_from_hexspace(pos, submap->pos);
        vec_sub(rgraph->space->dims, pos, camera_renderpos);
        vec_mul(rgraph->space, pos, game->map->unit);

        rot_t rot = vec4_rot_from_hexspace(0);
        //rot_t rot = vec4_rot_from_hexspace(
        //    rot_inv(space->rot_max, game->camera_rot));
        flip_t flip = false;
        int frame_i = game->frame_i;

        err = rendergraph_render(rgraph, renderer, pal, game->map->prend,
            x0, y0, zoom,
            pos, rot, flip, frame_i, mapper);
        if(err)return err;
    }

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        rendergraph_t *rgraph = player->state->rgraph;

        vec_t pos;
        vec4_vec_from_hexspace(pos, player->pos);
        vec_sub(rgraph->space->dims, pos, camera_renderpos);
        vec_mul(rgraph->space, pos, map->unit);

        rot_t player_rot = player_get_rot(player, space);
        rot_t rot = vec4_rot_from_hexspace(player_rot);
        //rot_t rot = vec4_rot_from_hexspace(
        //    rot_contain(space->rot_max,
        //        player_rot + rot_inv(space->rot_max, game->camera_rot)));
        flip_t flip = player->turn;
        int frame_i = player->frame_i;

        err = rendergraph_render(rgraph, renderer, pal, game->map->prend,
            x0, y0, zoom,
            pos, rot, flip, frame_i, mapper);
        if(err)return err;
    }

    return 0;
}

