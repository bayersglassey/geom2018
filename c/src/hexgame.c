

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
#include "test_app.h"


#define DEBUG_RULES false


/**********
 * PLAYER *
 **********/

void player_cleanup(player_t *player){
}

int player_init(player_t *player, state_t *state, int keymap){
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
    player->state = state;
    player->frame_i = 0;
    player->cooldown = 0;
    player->dead = false;
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
        if(cond->type == state_cond_type_key){

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

            bool collide = hexcollmap_collide(&game->map->collmap,
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
            state_t *state = stateset_get_state(game->stateset,
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
        }else if(effect->type == state_effect_type_die){
            player->dead = true;
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

    if(player->dead)return 0;

    player->frame_i++;

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

int hexgame_init(hexgame_t *game, stateset_t *stateset, hexmap_t *map,
    int n_players
){
    game->stateset = stateset;
    game->map = map;
    ARRAY_INIT(*game, players)

    state_t *default_state = stateset->states[0];

    for(int i = 0; i < n_players; i++){
        ARRAY_PUSH_NEW(player_t, *game, players, player)
        player_init(player, default_state, i);
    }

    return 0;
}

int hexgame_reset_player(hexgame_t *game, player_t *player, int keymap){
    player_cleanup(player);
    memset(player, 0, sizeof(*player));
    state_t *default_state = game->stateset->states[0];
    return player_init(player, default_state, keymap);
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    if(event->type == SDL_KEYDOWN){
        if(!event->key.repeat){
            if(event->key.keysym.sym == SDLK_1
                && game->players_len >= 1){
                    hexgame_reset_player(game, game->players[0], 0);}
            if(event->key.keysym.sym == SDLK_2
                && game->players_len >= 2){
                    hexgame_reset_player(game, game->players[1], 1);}
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                for(int i = 0; i < PLAYER_KEYS; i++){
                    if(event->key.keysym.sym == player->key_code[i]){
                        player->key_isdown[i] = true;
                        player->key_wasdown[i] = true;
                        player->key_wentdown[i] = true;}}}}
    }else if(event->type == SDL_KEYUP){
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
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game);
        if(err)return err;
    }
    return 0;
}

int hexgame_render(hexgame_t *game, test_app_t *app){
    int err;

    RET_IF_SDL_NZ(SDL_SetRenderDrawColor(app->renderer,
        30, 50, 80, 255));
    RET_IF_SDL_NZ(SDL_RenderClear(app->renderer));

    rendergraph_t *rgraph = game->map->rgraph_map;
    err = test_app_blit_rgraph(app, game->map->rgraph_map,
        (vec_t){0}, app->rot, false, app->frame_i, game->map->mapper);
    if(err)return err;

    vecspace_t *space = &hexspace;

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];

        if(player->dead)continue;

        rendergraph_t *rgraph = player->state->rgraph;

        vec_t pos;
        vec4_vec_from_hexspace(pos, player->pos);
        vec_mul(rgraph->space, pos, game->map->unit);
        rot_t player_rot = player_get_rot(player, space);
        rot_t rot = vec4_rot_from_hexspace(player_rot);
        flip_t flip = player->turn;
        int frame_i = player->frame_i;

        err = test_app_blit_rgraph(app, rgraph, pos, rot, flip, frame_i,
            game->map->mapper);
        if(err)return err;
    }

    SDL_RenderPresent(app->renderer);
    return 0;
}

