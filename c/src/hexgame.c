

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



/**********
 * PLAYER *
 **********/

void player_cleanup(player_t *player){
}

int player_init(player_t *player, rendergraph_t *rgraph,
    state_t *state, int keymap
){
    player->rgraph = rgraph;
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
    player->cooldown = 0;
    player->dead = false;
    return 0;
}

static int player_match_rule(player_t *player, hexgame_t *game,
    state_rule_t *rule, bool *rule_matched_ptr
){
    bool rule_matched = false;

    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        if(cond->type == state_cond_type_kdown){
            switch(cond->u.key){
                case 'u':
                    rule_matched =
                        player->key_isdown[PLAYER_KEY_U];
                    break;
                case 'd':
                    rule_matched =
                        player->key_isdown[PLAYER_KEY_D];
                    break;
                case 'f':
                    rule_matched =
                        player->key_isdown[player->turn?
                            PLAYER_KEY_L: PLAYER_KEY_R];
                    break;
                case 'b':
                    rule_matched =
                        player->key_isdown[player->turn?
                            PLAYER_KEY_R: PLAYER_KEY_L];
                    break;
                default: break;
            }
        }else if(cond->type == state_cond_type_coll){
            /* TODO!!!... */
        }else{
            fprintf(stderr, "Unrecognized state rule condition: %s\n",
                cond->type);
            return 2;
        }
        if(rule_matched)break;
    }

    *rule_matched_ptr = rule_matched;
    return 0;
}

static int player_apply_rule(player_t *player, hexgame_t *game,
    state_rule_t *rule
){
    const static vecspace_t *space = &hexspace;
    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(effect->type == state_effect_type_move){
            vec_t vec;
            vec_cpy(space->dims, vec, effect->u.vec);
            rot_t rot = player->rot;
            space->vec_rot(vec, rot);
            vec_add(space->dims, player->pos, vec);
        }else if(effect->type == state_effect_type_rot){
            player->rot = rot_rot(space->rot_max,
                player->rot, effect->u.rot);
        }else if(effect->type == state_effect_type_turn){
            player->turn = !player->turn;
        }else if(effect->type == state_effect_type_goto){
            state_t *state = stateset_get_state(game->stateset,
                effect->u.goto_name);
            if(state == NULL){
                fprintf(stderr, "Unrecognized player state: %s\n",
                    effect->u.goto_name);
                return 2;
            }
            player->state = state;
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

    if(player->cooldown > 0){
        player->cooldown--;
    }else{

        state_t *state = player->state;
        for(int i = 0; i < state->rules_len; i++){
            state_rule_t *rule = state->rules[i];

            bool rule_matched;
            err = player_match_rule(player, game, rule, &rule_matched);
            if(err)return err;

            if(rule_matched){
                err = player_apply_rule(player, game, rule);
                if(err)return err;
            }
        }

        /* start of new frame, no keys have gone down yet. */
        for(int i = 0; i < PLAYER_KEYS; i++){
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

int hexgame_init(hexgame_t *game, stateset_t *stateset, hexmap_t *map){
    game->stateset = stateset;
    game->map = map;
    ARRAY_INIT(*game, players)

    state_t *default_state = stateset->states[0];

    ARRAY_PUSH_NEW(player_t, *game, players, player0)
    player_init(player0, map->rgraph_player, default_state, 0);
    player0->pos[0] = 4;
    player0->pos[1] = 1;
    player0->rot = 0;

    ARRAY_PUSH_NEW(player_t, *game, players, player1)
    player_init(player1, map->rgraph_player, default_state, 1);
    player1->pos[0] = 0;
    player1->pos[1] = -2;
    player1->rot = 3;

    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    if(event->type == SDL_KEYDOWN){
        if(!event->key.repeat){
            for(int i = 0; i < game->players_len; i++){
                player_t *player = game->players[i];
                for(int i = 0; i < PLAYER_KEYS; i++){
                    if(event->key.keysym.sym == player->key_code[i]){
                        player->key_wentdown[i] = true;
                        player->key_isdown[i] = true;}}}}
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
        (vec_t){0}, 0, false, app->frame_i);
    if(err)return err;

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];

        if(player->dead)continue;

        rendergraph_t *rgraph = player->rgraph;
        vec_t pos;
        vec4_vec_from_hexspace(pos, player->pos);
        vec_mul(rgraph->space, pos, game->map->unit);
        rot_t rot = vec4_rot_from_hexspace(player->rot);
        flip_t flip = false;
        int frame_i = 0;

        /* TODO: player->turn affects rot + flip.
        Add player_get_rot, player_get_flip?.. */

        err = test_app_blit_rgraph(app, rgraph, pos, rot, flip, frame_i);
        if(err)return err;
    }

    SDL_RenderPresent(app->renderer);
    return 0;
}

