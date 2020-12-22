

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
#include "valexpr.h"
#include "var_utils.h"
#include "write.h"


/* TODO: these error handling macros are silly :P */

#define CHECK_BODY \
    if(body == NULL){ \
        RULE_PERROR() \
        fprintf(stderr, "No body\n"); \
        return 2;}

#define CHECK_ACTOR \
    if(actor == NULL){ \
        RULE_PERROR() \
        fprintf(stderr, "No actor\n"); \
        return 2;}


static int _get_vars(body_t *body, actor_t *actor,
    vars_t **mapvars_ptr, vars_t **myvars_ptr
){
    if(actor){
        *myvars_ptr = &actor->vars;
        body = actor->body;
        *mapvars_ptr = !actor->body? NULL: !actor->body->map? NULL:
            &actor->body->map->vars;
    }else{
        if(!body){
            fprintf(stderr, "No body\n"); \
            return 2;
        }
        *myvars_ptr = &body->vars;
        *mapvars_ptr = !body->map? NULL: &body->map->vars;
    }
    return 0;
}


int state_cond_match(state_cond_t *cond,
    hexgame_t *game, body_t *body, actor_t *actor,
    bool *matched_ptr
){
    int err;

    /* NOTE: body and/or actor may be NULL.
    We are basically reusing the rule/cond/effect structure for bodies
    and actors; most conds/effects naturally apply to one or the other.
    E.g. keypress stuff is for the body; "play" is for actor.
    However, actor may want to check some stuff about the body, so
    it may make use of "body-oriented" rules. */

    #define RULE_PERROR() \
        fprintf(stderr, "Error in condition: %s\n", cond->type);

    bool matched = false;

    if(cond->type == state_cond_type_false){
        matched = false;
    }else if(cond->type == state_cond_type_key){
        CHECK_BODY
        int kstate_i = cond->u.key.kstate;
        bool *kstate =
            kstate_i == 0? body->keyinfo.isdown:
            kstate_i == 1? body->keyinfo.wasdown:
            kstate_i == 2? body->keyinfo.wentdown:
            NULL;
        if(kstate == NULL){
            fprintf(stderr, "kstate out of range: %i\n", kstate_i);
            RULE_PERROR()
            return 2;}

        char c = cond->u.key.c;
        int key_i = body_get_key_i(body, c);
        if(key_i == -1){
            fprintf(stderr, "Unrecognized key char: %c\n", c);
            RULE_PERROR()
            return 2;}

        matched = kstate[key_i];
        if(!cond->u.key.yes)matched = !matched;
    }else if(cond->type == state_cond_type_coll){
        CHECK_BODY
        if(body->state == NULL){
            matched = false;
        }else{
            hexmap_t *map = body->map;
            vecspace_t *space = map->space;

            trf_t hitbox_trf;
            hexgame_location_init_trf(&body->loc, &hitbox_trf);
            hexcollmap_t *hitbox = cond->u.coll.collmap;

            int flags = cond->u.coll.flags;
            bool all = flags & ANIM_COND_FLAGS_ALL;
            bool yes = flags & ANIM_COND_FLAGS_YES;
            bool water = flags & ANIM_COND_FLAGS_WATER;
            bool against_bodies = flags & ANIM_COND_FLAGS_BODIES;

            const char *collmsg = cond->u.coll.collmsg;

            if(against_bodies){
                int n_matches = 0;
                for(int j = 0; j < map->bodies_len; j++){
                    body_t *body_other = map->bodies[j];
                    if(body == body_other)continue;
                    if(body_other->state == NULL)continue;
                    hexcollmap_t *hitbox_other = body_other->state->hitbox;
                    if(hitbox_other == NULL)continue;
                    if(collmsg &&
                        !body_sends_collmsg(body_other, collmsg)
                    )continue;

                    trf_t hitbox_other_trf;
                    hexgame_location_init_trf(&body_other->loc, &hitbox_other_trf);

                    /* The other body has a hitbox! Do the collision... */
                    bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                        hitbox_other, &hitbox_other_trf, space,
                        yes? all: !all);
                    if(yes? collide: !collide)n_matches++;
                }
                matched = n_matches > 0;
            }else if(water){
                hexmap_collision_t collision;
                hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);
                bool collide = collision.water.submap != NULL;
                matched = yes? collide: !collide;
            }else{
                bool collide = hexmap_collide(map,
                    hitbox, &hitbox_trf, yes? all: !all);
                matched = yes? collide: !collide;
            }
        }
    }else if(cond->type == state_cond_type_chance){
        int n = rand() % cond->u.ratio.b;
        matched = n <= cond->u.ratio.a;
    }else if(
        cond->type == state_cond_type_any ||
        cond->type == state_cond_type_all ||
        cond->type == state_cond_type_not
    ){
        bool all = cond->type == state_cond_type_all;
        matched = all? true: false;
        for(int i = 0; i < cond->u.subconds.conds_len; i++){
            state_cond_t *subcond = cond->u.subconds.conds[i];
            err = state_cond_match(subcond, game, body, actor,
                &matched);
            if(err)return err;
            if((all && !matched) || (!all && matched))break;
        }
        if(cond->type == state_cond_type_not){
            matched = !matched;
        }
    }else if(cond->type == state_cond_type_expr){
        CHECK_BODY
        int value = vars_get_int(&body->vars, cond->u.expr.var_name);
        int cond_value = cond->u.expr.value;
        int op = cond->u.expr.op;
        switch(op){
            case STATE_COND_EXPR_OP_EQ: matched = value == cond_value; break;
            case STATE_COND_EXPR_OP_NE: matched = value != cond_value; break;
            case STATE_COND_EXPR_OP_LT: matched = value  < cond_value; break;
            case STATE_COND_EXPR_OP_LE: matched = value <= cond_value; break;
            case STATE_COND_EXPR_OP_GT: matched = value  > cond_value; break;
            case STATE_COND_EXPR_OP_GE: matched = value >= cond_value; break;
            default:
                fprintf(stderr, "Bad expr op: %i\n", op);
                RULE_PERROR()
                return 2;
        }
    }else if(cond->type == state_cond_type_get_bool){
        vars_t *mapvars;
        vars_t *myvars;
        err = _get_vars(body, actor, &mapvars, &myvars);
        if(err)return err;

        val_t *val;
        err = valexpr_get(&cond->u.valexpr,
            mapvars, myvars, &val);
        if(err)return err;
        if(val == NULL){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for expression\n");
            return 2;
        }

        if(val->type != VAL_TYPE_BOOL){
            RULE_PERROR()
            fprintf(stderr, "Expected bool value, got: %s\n",
                val_type_name(val->type));
            return 2;
        }

        matched = val->u.b;
    }else{
        fprintf(stderr, "Unrecognized condition: %s\n",
            cond->type);
        RULE_PERROR()
        return 2;
    }
    #undef RULE_PERROR

    *matched_ptr = matched;
    return 0;
}

static int state_rule_match(state_rule_t *rule,
    hexgame_t *game, body_t *body, actor_t *actor,
    bool *matched_ptr
){
    int err;

    bool matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        if(DEBUG_RULES)printf("  if: %s\n", cond->type);
        err = state_cond_match(cond, game, body, actor,
            &matched);
        if(err){
            if(err == 2){
                fprintf(stderr, "...in: state=%s, stateset=%s\n",
                    rule->state->name, rule->state->stateset->filename);
            }
            return err;
        }
        if(!matched)break;
    }

    if(DEBUG_RULES && !matched)printf("    NO MATCH\n");

    *matched_ptr = matched;
    return 0;
}

static void effect_apply_boolean(int boolean, bool *b_ptr){
    if(boolean == EFFECT_BOOLEAN_TRUE)*b_ptr = true;
    else if(boolean == EFFECT_BOOLEAN_FALSE)*b_ptr = false;
    else if(boolean == EFFECT_BOOLEAN_TOGGLE)*b_ptr = !*b_ptr;
}

int state_effect_apply(state_effect_t *effect,
    hexgame_t *game, body_t *body, actor_t *actor,
    state_effect_goto_t **gotto_ptr, bool *continues_ptr
){
    #define RULE_PERROR() \
        fprintf(stderr, "Error in effect: %s\n", effect->type);

    int err;

    if(
        effect->type == state_effect_type_print ||
        effect->type == state_effect_type_print_var ||
        effect->type == state_effect_type_print_vars
    ){
        if(body != NULL)fprintf(stderr, "body %p", body);
        else fprintf(stderr, "unknown body");
        if(actor != NULL)fprintf(stderr, " (actor %p)", actor);

        vars_t *vars = actor? &actor->vars: &body->vars;
        if(effect->type == state_effect_type_print_vars){
            fprintf(stderr, " vars:\n");
            vars_write(vars, stderr, 1);
        }else if(effect->type == state_effect_type_print_var){
            var_t *var = vars_get(vars, effect->u.var_name);
            if(var){
                fprintf(stderr, " says: ");
                val_fprintf(&var->value, stderr);
                fprintf(stderr, "\n");
            }else{
                // :P
                fprintf(stderr, " was tongue-tied\n");
            }
        }else{
            fprintf(stderr, " says: %s\n", effect->u.msg);
        }
    }else if(effect->type == state_effect_type_move){
        CHECK_BODY
        vecspace_t *space = body->map->space;
        vec_t vec;
        vec_cpy(space->dims, vec, effect->u.vec);
        rot_t rot = hexgame_location_get_rot(&body->loc);
        space->vec_flip(vec, body->loc.turn);
        space->vec_rot(vec, rot);
        vec_add(space->dims, body->loc.pos, vec);
    }else if(effect->type == state_effect_type_rot){
        CHECK_BODY
        vecspace_t *space = body->map->space;
        rot_t effect_rot = effect->u.rot;
        body->loc.rot = rot_rot(space->rot_max,
            body->loc.rot, effect_rot);
    }else if(effect->type == state_effect_type_turn){
        CHECK_BODY
        vecspace_t *space = body->map->space;
        body->loc.turn = !body->loc.turn;
        body->loc.rot = rot_flip(space->rot_max, body->loc.rot, true);
    }else if(effect->type == state_effect_type_goto){
        *gotto_ptr = &effect->u.gotto;
    }else if(effect->type == state_effect_type_delay){
        if(actor){
            actor->wait = effect->u.delay;
        }else{
            CHECK_BODY
            body->cooldown = effect->u.delay;
        }
    }else if(effect->type == state_effect_type_spawn){
        CHECK_BODY
        state_effect_spawn_t *spawn = &effect->u.spawn;

        /* TODO: look up palmapper from spawn->palmapper_name */
        palettemapper_t *palmapper = NULL;

        body_t *new_body;
        err = body_add_body(body, &new_body,
            spawn->stateset_filename,
            spawn->state_name, palmapper,
            spawn->loc.pos, spawn->loc.rot, spawn->loc.turn);
        if(err)return err;
    }else if(effect->type == state_effect_type_play){
        CHECK_ACTOR
        const char *play_filename = effect->u.play_filename;
        err = body_load_recording(body, play_filename, false);
        if(err)return err;
        hexgame_location_apply(&body->recording.loc0, &actor->trf);
        err = body_play_recording(body);
        if(err)return err;
    }else if(effect->type == state_effect_type_die){
        CHECK_BODY
        body->dead = effect->u.dead;
    }else if(effect->type == state_effect_type_zero){
        CHECK_BODY
        err = vars_set_int(&body->vars, effect->u.var_name, 0);
        if(err)return err;
    }else if(effect->type == state_effect_type_inc){
        CHECK_BODY
        int value = vars_get_int(&body->vars, effect->u.var_name);
        err = vars_set_int(&body->vars, effect->u.var_name, value + 1);
        if(err)return err;
    }else if(effect->type == state_effect_type_continue){
        *continues_ptr = true;
    }else if(effect->type == state_effect_type_confused){
        CHECK_BODY
        effect_apply_boolean(effect->u.boolean, &body->confused);
    }else if(effect->type == state_effect_type_key){
        CHECK_BODY
        int key_i = body_get_key_i(body, effect->u.key.c);
        bool keydown = effect->u.key.action & 0x1;
        if(keydown){
            body_keydown(body, key_i);
        }
        bool keyup = effect->u.key.action & 0x2;
        if(keyup){
            body_keyup(body, key_i);
        }
    }else if(effect->type == state_effect_type_set){
        vars_t *mapvars;
        vars_t *myvars;
        err = _get_vars(body, actor, &mapvars, &myvars);
        if(err)return err;

        val_t *var_val;
        err = valexpr_set(&effect->u.set.var_expr,
            mapvars, myvars, &var_val);
        if(err)return err;
        /* NOTE: valexpr_set guarantees that we find (or create, if
        necessary) a val, so we don't need to check whether var_val is
        NULL. */

        val_t *val_val;
        err = valexpr_get(&effect->u.set.val_expr,
            mapvars, myvars, &val_val);
        if(err)return err;
        if(val_val == NULL){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for expression\n");
            return 2;
        }

        err = val_copy(var_val, val_val);
        if(err)return err;
    }else{
        fprintf(stderr, "Unrecognized effect: %s\n",
            effect->type);
        return 2;
    }

    return 0;
    #undef RULE_PERROR
}

static int state_rule_apply(state_rule_t *rule,
    hexgame_t *game, body_t *body, actor_t *actor,
    state_effect_goto_t **gotto_ptr, bool *continues_ptr
){
    int err;

    /* NOTE: body and/or actor may be NULL.
    See comment on rule_match. */

    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(DEBUG_RULES)printf("  then: %s\n", effect->type);
        err = state_effect_apply(effect, game, body, actor,
            gotto_ptr, continues_ptr);
        if(err){
            if(err == 2){
                fprintf(stderr, "...in: state=%s, stateset=%s\n",
                    rule->state->name, rule->state->stateset->filename);
            }
            return err;
        }
    }
    return 0;
}

int state_handle_rules(state_t *state,
    hexgame_t *game, body_t *body, actor_t *actor,
    state_effect_goto_t **gotto_ptr
){
    int err;

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];

        if(DEBUG_RULES){printf("body %p, actor %p, rule %i:\n",
            body, actor, i);}

        bool matched;
        err = state_rule_match(rule, game, body, actor,
            &matched);
        if(err)return err;

        if(matched){
            bool continues = false;
            err = state_rule_apply(rule, game, body, actor,
                gotto_ptr, &continues);
            if(err)return err;
            if(!continues)break;
        }
    }

    return 0;
}

int collmsg_handler_apply(collmsg_handler_t *handler,
    struct hexgame *game, struct body *body, struct actor *actor,
    bool *continues_ptr
){
    int err;

    for(int i = 0; i < handler->effects_len; i++){
        state_effect_t *effect = handler->effects[i];

        state_effect_goto_t *gotto = NULL;
        err = state_effect_apply(effect, game, body, actor,
            &gotto, continues_ptr);
        if(err)return err;

        if(gotto != NULL){
            if(actor != NULL){
                /* ACTOR */
                err = actor_set_state(actor, gotto->name);
                if(err)return err;
                if(gotto->immediate){
                    /* If there was an "immediate goto" effect,
                    then we immediately handle the new state's rules */
                    err = actor_handle_rules(actor);
                    if(err)return err;
                }
            }else{
                /* BODY */
                err = body_set_state(body, gotto->name, false);
                if(err)return err;
                if(gotto->immediate){
                    /* If there was an "immediate goto" effect,
                    then we immediately handle the new state's rules */
                    err = body_handle_rules(body);
                    if(err)return err;
                }
            }
        }
    }
    return 0;
}
