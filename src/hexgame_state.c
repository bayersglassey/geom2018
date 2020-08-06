

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


static int body_match_cond(body_t *body,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, state_cond_t *cond,
    bool *rule_matched_ptr
){
    int err;

    if(DEBUG_RULES)printf("  if: %s\n", cond->type);

    #define RULE_PERROR() \
        fprintf(stderr, " (cond=%s, state=%s, stateset=%s)\n", \
            cond->type, rule->state->name, \
            rule->state->stateset->filename);

    bool rule_matched = false;

    if(cond->type == state_cond_type_false){
        rule_matched = false;
    }else if(cond->type == state_cond_type_key){
        if(body == NULL){
            fprintf(stderr, "No body");
            RULE_PERROR()
            return 2;}

        int kstate_i = cond->u.key.kstate;
        bool *kstate =
            kstate_i == 0? body->keyinfo.isdown:
            kstate_i == 1? body->keyinfo.wasdown:
            kstate_i == 2? body->keyinfo.wentdown:
            NULL;
        if(kstate == NULL){
            fprintf(stderr, "kstate out of range: %i", kstate_i);
            RULE_PERROR()
            return 2;}

        char c = cond->u.key.c;
        int key_i = body_get_key_i(body, c, false);
        if(key_i == -1){
            fprintf(stderr, "Unrecognized key char: %c", c);
            RULE_PERROR()
            return 2;}

        rule_matched = kstate[key_i];
        if(!cond->u.key.yes)rule_matched = !rule_matched;
    }else if(cond->type == state_cond_type_coll){
        if(body == NULL){
            fprintf(stderr, "No body");
            RULE_PERROR()
            return 2;}

        if(body->state == NULL){
            rule_matched = false;
        }else{
            hexmap_t *map = body->map;
            vecspace_t *space = map->space;

            trf_t hitbox_trf;
            body_init_trf(body, &hitbox_trf);
            hexcollmap_t *hitbox = cond->u.coll.collmap;

            int flags = cond->u.coll.flags;
            bool all = flags & ANIM_COND_FLAGS_ALL;
            bool yes = flags & ANIM_COND_FLAGS_YES;
            bool water = flags & ANIM_COND_FLAGS_WATER;
            bool against_bodies = flags & ANIM_COND_FLAGS_BODIES;

            if(against_bodies){
                int n_matches = 0;
                for(int j = 0; j < map->bodies_len; j++){
                    body_t *body_other = map->bodies[j];
                    if(body == body_other)continue;
                    if(body_other->state == NULL)continue;
                    hexcollmap_t *hitbox_other = body_other->state->hitbox;
                    if(hitbox_other == NULL)continue;

                    trf_t hitbox_other_trf;
                    body_init_trf(body_other, &hitbox_other_trf);

                    /* The other body has a hitbox! Do the collision... */
                    bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                        hitbox_other, &hitbox_other_trf, space,
                        yes? all: !all);
                    if(yes? collide: !collide)n_matches++;
                }
                rule_matched = n_matches > 0;
            }else if(water){
                hexmap_collision_t collision;
                hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);
                bool collide = collision.water.submap != NULL;
                rule_matched = yes? collide: !collide;
            }else{
                bool collide = hexmap_collide(map,
                    hitbox, &hitbox_trf, yes? all: !all);
                rule_matched = yes? collide: !collide;
            }
        }
    }else if(cond->type == state_cond_type_chance){
        int n = rand() % 100;
        rule_matched = n <= cond->u.percent;
    }else if(
        cond->type == state_cond_type_any ||
        cond->type == state_cond_type_all
    ){
        bool all = cond->type == state_cond_type_all;
        rule_matched = all? true: false;
        for(int i = 0; i < cond->u.subconds.conds_len; i++){
            state_cond_t *subcond = cond->u.subconds.conds[i];
            err = body_match_cond(body, actor, game, rule, subcond,
                &rule_matched);
            if(err)return err;
            if((all && !rule_matched) || (!all && rule_matched))break;
        }
    }else if(cond->type == state_cond_type_expr){
        if(body == NULL){
            fprintf(stderr, "No body");
            RULE_PERROR()
            return 2;}
        int value = vars_get_int(&body->vars, cond->u.expr.var_name);
        int cond_value = cond->u.expr.value;
        int op = cond->u.expr.op;
        switch(op){
            case STATE_COND_EXPR_OP_EQ: rule_matched = value == cond_value; break;
            case STATE_COND_EXPR_OP_NE: rule_matched = value != cond_value; break;
            case STATE_COND_EXPR_OP_LT: rule_matched = value  < cond_value; break;
            case STATE_COND_EXPR_OP_LE: rule_matched = value <= cond_value; break;
            case STATE_COND_EXPR_OP_GT: rule_matched = value  > cond_value; break;
            case STATE_COND_EXPR_OP_GE: rule_matched = value >= cond_value; break;
            default:
                fprintf(stderr, "Bad expr op: %i\n", op);
                RULE_PERROR()
                return 2;
        }
    }else{
        fprintf(stderr, "Unrecognized state rule condition: %s",
            cond->type);
        RULE_PERROR()
        return 2;
    }
    #undef RULE_PERROR

    *rule_matched_ptr = rule_matched;
    return 0;
}

static int body_match_rule(body_t *body,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, bool *rule_matched_ptr
){
    int err;

    /* NOTE: body and/or actor may be NULL.
    We are basically reusing the rule/cond/effect structure for bodies
    and actors; most conds/effects naturally apply to one or the other.
    E.g. keypress stuff is for the body; "play" is for actor.
    However, actor may want to check some stuff about the body, so
    it may make use of "body-oriented" rules. */

    bool rule_matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        err = body_match_cond(body, actor, game, rule, cond, &rule_matched);
        if(err)return err;
        if(!rule_matched)break;
    }

    if(DEBUG_RULES && !rule_matched)printf("    NO MATCH\n");

    *rule_matched_ptr = rule_matched;
    return 0;
}

static void effect_apply_boolean(int boolean, bool *b_ptr){
    if(boolean == EFFECT_BOOLEAN_TRUE)*b_ptr = true;
    else if(boolean == EFFECT_BOOLEAN_FALSE)*b_ptr = false;
    else if(boolean == EFFECT_BOOLEAN_TOGGLE)*b_ptr = !*b_ptr;
}

static int body_apply_rule(body_t *body,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, state_effect_goto_t **gotto_ptr, bool *continues_ptr
){
    int err;

    /* NOTE: body and/or actor may be NULL.
    See comment on body_match_rule. */

    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(DEBUG_RULES)printf("  then: %s\n", effect->type);

        #define RULE_PERROR() \
            fprintf(stderr, " (effect=%s, state=%s, stateset=%s)\n", \
                effect->type, rule->state->name, \
                rule->state->stateset->filename);

        if(
            effect->type == state_effect_type_print ||
            effect->type == state_effect_type_print_int
        ){
            if(body != NULL)printf("body %p", body);
            else printf("unknown body");
            if(actor != NULL)printf(" (actor %p)", actor);
            if(effect->type == state_effect_type_print_int){
                int value = body? vars_get_int(&body->vars, effect->u.var_name): 0;
                printf(" says: %i\n", value);
            }else{
                printf(" says: %s\n", effect->u.msg);
            }
        }else if(effect->type == state_effect_type_move){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            vecspace_t *space = body->map->space;
            vec_t vec;
            vec_cpy(space->dims, vec, effect->u.vec);
            rot_t rot = body_get_rot(body);
            space->vec_flip(vec, body->turn);
            space->vec_rot(vec, rot);
            vec_add(space->dims, body->pos, vec);
        }else if(effect->type == state_effect_type_rot){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            vecspace_t *space = body->map->space;
            rot_t effect_rot = effect->u.rot;
            body->rot = rot_rot(space->rot_max,
                body->rot, effect_rot);
        }else if(effect->type == state_effect_type_turn){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            vecspace_t *space = body->map->space;
            body->turn = !body->turn;
            body->rot = rot_flip(space->rot_max, body->rot, true);
        }else if(effect->type == state_effect_type_goto){
            *gotto_ptr = &effect->u.gotto;
        }else if(effect->type == state_effect_type_delay){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}
            body->cooldown = effect->u.delay;
        }else if(effect->type == state_effect_type_spawn){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            state_effect_spawn_t *spawn = &effect->u.spawn;

            /* TODO: look up palmapper from spawn->palmapper_name */
            palettemapper_t *palmapper = NULL;

            body_t *new_body;
            err = body_add_body(body, &new_body,
                spawn->stateset_filename,
                spawn->state_name, palmapper,
                spawn->pos, spawn->rot, spawn->turn);
            if(err)return err;
        }else if(effect->type == state_effect_type_play){
            if(actor == NULL){
                fprintf(stderr, "No actor");
                RULE_PERROR()
                return 2;}
            const char *play_filename = effect->u.play_filename;
            err = body_load_recording(body, play_filename, false);
            if(err)return err;
            err = body_play_recording(body);
            if(err)return err;
        }else if(effect->type == state_effect_type_die){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}
            body->dead = effect->u.dead;
        }else if(effect->type == state_effect_type_inc){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}
            int value = vars_get_int(&body->vars, effect->u.var_name);
            err = vars_set_int(&body->vars, effect->u.var_name, value + 1);
            if(err)return err;
        }else if(effect->type == state_effect_type_continue){
            *continues_ptr = true;
        }else if(effect->type == state_effect_type_confused){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}
            effect_apply_boolean(effect->u.boolean, &body->confused);
        }else{
            fprintf(stderr, "Unrecognized state rule effect: %s\n",
                effect->type);
            return 2;
        }
        #undef RULE_PERROR
    }
    return 0;
}

int state_handle_rules(state_t *state, body_t *body,
    actor_t *actor, hexgame_t *game, state_effect_goto_t **gotto_ptr
){
    int err;

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];

        if(DEBUG_RULES){printf("body %p, actor %p, rule %i:\n",
            body, actor, i);}

        bool rule_matched;
        err = body_match_rule(body, actor, game, rule,
            &rule_matched);
        if(err)return err;

        if(rule_matched){
            bool continues = false;
            err = body_apply_rule(body, actor, game, rule,
                gotto_ptr, &continues);
            if(err)return err;
            if(!continues)break;
        }
    }

    return 0;
}

