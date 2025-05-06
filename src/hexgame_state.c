

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "hexgame_state.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
#include "prismelrenderer.h"
#include "array.h"
#include "util.h"
#include "valexpr.h"
#include "var_utils.h"
#include "write.h"


bool hexgame_state_context_debug(hexgame_state_context_t *context){
    return
        (context->body && context->body == context->game->anim_debug_body) ||
        (context->actor && context->actor == context->game->anim_debug_actor);
}


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


static int _get_vars(hexgame_state_context_t *context,
    valexpr_context_t *valexpr_context
){
    /* NOTE: caller promises that valexpr_context is zeroed out */
    int err;

    actor_t *actor = context->actor;
    body_t *body = context->body;
    body_t *your_body = context->your_body;
    hexmap_t *map = body? body->map: NULL;
    hexgame_t *game =
        actor? actor->game:
        body? body->game:
        your_body? your_body->game:
        map? map->game:
        NULL;

    /* PROBABLY TODO: distinguish between "my actor vars" vs "my body vars"...
    Also, doesn't caller guarantee that if actor!=NULL, then body == actor->body?.. */

    /* "refresh" all vars (e.g. body's ".turn" var is set to body->turn, etc) */
    if(actor){
        err = actor_refresh_vars(actor);
        if(err)return err;
    }
    if(body){
        err = body_refresh_vars(body);
        if(err)return err;
    }
    if(your_body){
        err = body_refresh_vars(your_body);
        if(err)return err;
    }
    if(map){
        err = hexmap_refresh_vars(map);
        if(err)return err;
    }

    /* This "if actor" behaviour is ganky beyond belief :( */
    if(actor){
        valexpr_context->myvars = &actor->vars;
    }else{
        valexpr_context->myvars = body? &body->vars: NULL;
    }
    valexpr_context->yourvars = your_body? &your_body->vars: NULL;
    valexpr_context->mapvars = map? &map->vars: NULL;
    valexpr_context->globalvars = game? &game->vars: NULL;

    return 0;
}


static int _get_rgraph_delay(rendergraph_t *rgraph){
    if(!rgraph)return 0;
    int delay = rgraph->n_frames;
    if(rgraph->animation_type == rendergraph_animation_type_oscillate){
        delay *= 2;
    }
    return delay > 0? delay - 1: 0;
}

int state_effect_goto_apply_to_body(state_effect_goto_t *gotto,
    body_t *body
){
    /* NOTE: Caller guarantees body != NULL */
    int err;
    err = body_set_state(body, gotto->name, false);
    if(err)return err;
    if(gotto->delay && body->state){
        body->cooldown = _get_rgraph_delay(body->state->rgraph)
            + gotto->add_delay;
    }
    return 0;
}

int state_effect_goto_apply_to_actor(state_effect_goto_t *gotto,
    actor_t *actor
){
    /* NOTE: Caller guarantees actor != NULL */
    int err;
    err = actor_set_state(actor, gotto->name);
    if(err)return err;
    if(gotto->delay && actor->state){
        actor->wait = _get_rgraph_delay(actor->state->rgraph)
            + gotto->add_delay;
    }
    return 0;
}


int state_cond_match(state_cond_t *cond,
    hexgame_state_context_t *context,
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
        fprintf(stderr, "Error in condition: %s\n", \
            state_cond_type_name(cond->type));

    bool matched = false;

    hexgame_t *game = context->game;
    actor_t *actor = context->actor;
    body_t *body = context->body;
    body_t *your_body = context->your_body;

    switch(cond->type){
    case STATE_COND_TYPE_FALSE: {
        matched = false;
        break;
    }
    case STATE_COND_TYPE_TRUE: {
        matched = true;
        break;
    }
    case STATE_COND_TYPE_KEY: {
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
        break;
    }
    case STATE_COND_TYPE_COLL: {
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

            if(against_bodies){
                valexpr_context_t valexpr_context = {0};
                err = _get_vars(context, &valexpr_context);
                if(err)return err;

                const char *collmsg = valexpr_get_str(
                    &cond->u.coll.collmsg_expr, &valexpr_context);

                int n_matches = 0;
                for(int j = 0; j < map->bodies_len; j++){
                    body_t *body_other = map->bodies[j];
                    if(body == body_other)continue;
                    if(body_other->state == NULL)continue;
                    hexcollmap_t *hitbox_other = body_other->state->hitbox;
                    if(hitbox_other == NULL)continue;

                    bool visible;
                    err = body_is_visible(body_other, &visible);
                    if(err)return err;
                    if(!visible)continue;

                    if(collmsg && !(
                        !strcmp(body_other->stateset->filename, collmsg) ||
                        body_sends_collmsg(body_other, collmsg)
                    ))continue;

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
                err = hexmap_collide_special(map, hitbox, &hitbox_trf,
                    &collision);
                if(err)return err;
                bool collide = collision.water.submap != NULL;
                matched = yes? collide: !collide;
            }else{
                bool collide;
                err = hexmap_collide(map, hitbox, &hitbox_trf,
                    yes? all: !all, &collide);
                if(err)return err;
                matched = yes? collide: !collide;
            }
        }
        break;
    }
    case STATE_COND_TYPE_DEAD: {
        CHECK_BODY
        matched = cond->u.dead == -1?
            body_is_done_for(body):
            body->dead == cond->u.dead;
        break;
    }
    case STATE_COND_TYPE_IS_PLAYER: {
        CHECK_BODY
        matched = body_get_player(body) != NULL;
        break;
    }
    case STATE_COND_TYPE_CHANCE: {
        int n = rand() % cond->u.ratio.b;
        matched = n <= cond->u.ratio.a;
        break;
    }
    case STATE_COND_TYPE_ANY:
    case STATE_COND_TYPE_ALL:
    case STATE_COND_TYPE_NOT: {
        bool all = cond->type != STATE_COND_TYPE_ANY;
        matched = all? true: false;
        for(int i = 0; i < cond->u.subconds.conds_len; i++){
            state_cond_t *subcond = cond->u.subconds.conds[i];
            err = state_cond_match(subcond, context, &matched);
            if(err)return err;
            if((all && !matched) || (!all && matched))break;
        }
        if(cond->type == STATE_COND_TYPE_NOT){
            matched = !matched;
        }
        break;
    }
    case STATE_COND_TYPE_EXPR: {
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        matched = valexpr_get_bool(&cond->u.valexpr, &valexpr_context);
        break;
    }
    case STATE_COND_TYPE_EXISTS: {
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        valexpr_result_t result = {0};
        err = valexpr_get(&cond->u.valexpr, &valexpr_context, &result);
        if(err)return err;

        matched = result.val != NULL && result.val->type != VAL_TYPE_NULL;
        break;
    }
    case STATE_COND_TYPE_AS: {
        switch(cond->u.as.type){
            case AS_YOU: {
                if(!your_body){
                    fprintf(stderr, "No your_body!\n");
                    return 2;
                }

                /* We apply our sub-conditions "as you", that is, as your_body.
                The terminology here is just too silly, eh?
                Anyway, long story short, sub_context is like context but with
                body and your_body swapped. */
                hexgame_state_context_t sub_context = *context;
                sub_context.body = your_body;
                sub_context.your_body = body;

                matched = true;
                for(int i = 0; i < cond->u.as.sub_conds_len; i++){
                    state_cond_t *sub_cond = cond->u.as.sub_conds[i];
                    err = state_cond_match(sub_cond, &sub_context, &matched);
                    if(err)return err;
                    if(!matched)break;
                }

                break;
            }
            default:
                fprintf(stderr, "Unrecognized \"as\" type: %i\n",
                    cond->u.as.type);
                return 2;
        }
        break;
    }
    default: {
        fprintf(stderr, "Unrecognized condition: %s\n",
            state_cond_type_name(cond->type));
        RULE_PERROR()
        return 2;
    }
    }
    #undef RULE_PERROR

    *matched_ptr = matched;
    return 0;
}

static int state_rule_match(state_rule_t *rule,
    hexgame_state_context_t *context,
    bool *matched_ptr
){
    int err;

    bool matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        err = state_cond_match(cond, context, &matched);
        if(err){
            if(err == 2){
                fprintf(stderr, "...in: state=%s, stateset=%s\n",
                    rule->state->name, rule->state->stateset->filename);
            }
            return err;
        }
        if(!matched)break;
    }

    *matched_ptr = matched;
    return 0;
}

static void effect_apply_boolean(int boolean, bool *b_ptr){
    if(boolean == EFFECT_BOOLEAN_TRUE)*b_ptr = true;
    else if(boolean == EFFECT_BOOLEAN_FALSE)*b_ptr = false;
    else if(boolean == EFFECT_BOOLEAN_TOGGLE)*b_ptr = !*b_ptr;
}

static int _apply_sub_effects(state_effect_t *effect,
    int sub_effects_len, state_effect_t **sub_effects,
    hexgame_state_context_t *context
){
    int err;

    for(int i = 0; i < sub_effects_len; i++){
        state_effect_t *sub_effect = sub_effects[i];

        state_effect_goto_t *gotto = NULL;
        err = state_effect_apply(sub_effect, context, &gotto,
            NULL);
        if(err){
            if(err == 2){
                fprintf(stderr, "...in \"%s\" statement\n",
                    state_effect_type_name(effect->type));
            }
            return err;
        }

        if(gotto != NULL && context->body != NULL){
            err = state_effect_goto_apply_to_body(gotto, context->body);
            if(err)return err;
            if(gotto->immediate){
                /* If there was an "immediate goto" effect,
                then we immediately handle the new state's rules */
                err = body_handle_rules(context->body, context->your_body);
                if(err)return err;
            }
        }
    }

    return 0;
}

int state_effect_apply(state_effect_t *effect,
    hexgame_state_context_t *context,
    state_effect_goto_t **gotto_ptr, bool *continues_ptr
){
    #define RULE_PERROR() \
        fprintf(stderr, "Error in effect: %s\n", state_effect_type_name(effect->type));

    int err;

    hexgame_t *game = context->game;
    prismelrenderer_t *prend = game->prend;
    actor_t *actor = context->actor;
    body_t *body = context->body;
    body_t *your_body = context->your_body;

    bool anim_debug = hexgame_state_context_debug(context);

    switch(effect->type){
    case STATE_EFFECT_TYPE_NOOP: break;
    case STATE_EFFECT_TYPE_NO_KEY_RESET: {
        CHECK_BODY
        body->no_key_reset = true;
        break;
    }
    case STATE_EFFECT_TYPE_PRINT: {
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        valexpr_result_t result = {0};
        err = valexpr_get(&effect->u.expr, &valexpr_context, &result);
        if(err)return err;
        val_t *val = result.val;
        if(val == NULL){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for expression: ");
            valexpr_fprintf(&effect->u.expr, stderr);
            fputc('\n', stderr);
            return 2;
        }

        if(body != NULL)fprintf(stderr, "body %p", body);
        else fprintf(stderr, "unknown body");
        if(actor != NULL)fprintf(stderr, " (actor %p)", actor);
        fputs(": ", stderr);

        val_fprintf(val, stderr);
        fputc('\n', stderr);
        break;
    }
    case STATE_EFFECT_TYPE_MOVE: {
        CHECK_BODY
        vecspace_t *space = body->map->space;
        vec_t vec;
        vec_cpy(space->dims, vec, effect->u.vec);
        rot_t rot = hexgame_location_get_rot(&body->loc);
        space->vec_flip(vec, body->loc.turn);
        space->vec_rot(vec, rot);
        vec_add(space->dims, body->loc.pos, vec);
        break;
    }
    case STATE_EFFECT_TYPE_ROT: {
        CHECK_BODY
        vecspace_t *space = body->map->space;
        rot_t effect_rot = effect->u.rot;
        body->loc.rot = rot_rot(space->rot_max,
            body->loc.rot, effect_rot);
        break;
    }
    case STATE_EFFECT_TYPE_TURN: {
        CHECK_BODY
        vecspace_t *space = body->map->space;
        body->loc.turn = !body->loc.turn;
        body->loc.rot = rot_flip(space->rot_max, body->loc.rot, true);
        break;
    }
    case STATE_EFFECT_TYPE_RELOCATE: {
        CHECK_BODY
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

#       define GET_VALEXPR_STR(STR, EXPR) \
        const char *STR = NULL; \
        { \
            valexpr_result_t result = {0}; \
            err = valexpr_get(EXPR, &valexpr_context, &result); \
            if(err)return err; \
            if(result.val == NULL){ \
                RULE_PERROR() \
                fprintf(stderr, "Couldn't get value for " #STR " expression: "); \
                valexpr_fprintf(EXPR, stderr); \
                fputc('\n', stderr); \
                return 2; \
            } \
            STR = val_get_str(result.val); \
        }
        GET_VALEXPR_STR(loc_name, &effect->u.relocate.loc_expr)
        GET_VALEXPR_STR(map_filename, &effect->u.relocate.map_filename_expr)
        GET_VALEXPR_STR(stateset_filename,
            &effect->u.relocate.stateset_filename_expr)
        GET_VALEXPR_STR(state_name, &effect->u.relocate.state_name_expr)
#       undef GET_VALEXPR_STR

        hexmap_t *new_map = body->map;
        if(map_filename != NULL){
            err = hexgame_get_or_load_map(game,
                map_filename, &new_map);
            if(err)return err;
        }

        hexgame_location_t *loc = NULL;
        if(loc_name != NULL){
            loc = hexmap_get_location(new_map, loc_name);
            if(loc == NULL){
                RULE_PERROR()
                fprintf(stderr,
                    "Couldn't find location \"%s\" for map \"%s\"\n",
                    loc_name, new_map->filename);
                return 2;
            }
        }

        err = body_relocate(body, map_filename,
            loc, stateset_filename, state_name);
        if(err)return err;
        break;
    }
    case STATE_EFFECT_TYPE_GOTO: {
        if(anim_debug)fprintf(stderr, "Goto: \"%s\"\n", effect->u.gotto.name);
        *gotto_ptr = &effect->u.gotto;
        break;
    }
    case STATE_EFFECT_TYPE_CALL: {
        CHECK_BODY
        state_context_t *state_context = body->state->context;
        const char *name = effect->u.call.name;
        if(anim_debug)fprintf(stderr, "Call: \"%s\"\n", name);
        stateset_proc_t *proc = state_context_get_proc(state_context, name);
        if(!proc){
            RULE_PERROR()
            fprintf(stderr,
                "Couldn't find proc \"%s\" for stateset \"%s\"\n",
                name, state_context->stateset->filename);
            return 2;
        }
        for(int i = 0; i < proc->effects_len; i++){
            state_effect_t *effect = proc->effects[i];
            err = state_effect_apply(effect, context, gotto_ptr,
                continues_ptr);
            if(err){
                if(err == 2){
                    fprintf(stderr, "...while calling proc: %s\n", name);
                }
                return err;
            }
        }
        break;
    }
    case STATE_EFFECT_TYPE_DELAY:
    case STATE_EFFECT_TYPE_ADD_DELAY: {
        int *delay_ptr;
        if(actor){
            delay_ptr = &actor->wait;
        }else{
            CHECK_BODY
            delay_ptr = &body->cooldown;
        }

        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        int delay = valexpr_get_int(&effect->u.expr, &valexpr_context);
        if(effect->type == STATE_EFFECT_TYPE_ADD_DELAY){
            *delay_ptr += delay;
        }else{
            *delay_ptr = delay;
        }
        break;
    }
    case STATE_EFFECT_TYPE_SPAWN: {
        CHECK_BODY
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        state_effect_spawn_t *spawn = &effect->u.spawn;

        palettemapper_t *palmapper = NULL;
        if(spawn->palmapper_name){
            palmapper = prismelrenderer_get_palmapper(prend,
                spawn->palmapper_name);
            if(!palmapper){
                RULE_PERROR()
                fprintf(stderr, "Couldn't find palmapper: %s\n",
                    spawn->palmapper_name);
                return 2;
            }
        }

        const char *stateset_filename = valexpr_get_str(
            &spawn->stateset_filename_expr, &valexpr_context);
        if(!stateset_filename){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for expression: ");
            valexpr_fprintf(&spawn->stateset_filename_expr, stderr);
            fputc('\n', stderr);
            return 2;
        }

        const char *state_name = valexpr_get_str(
            &spawn->state_name_expr, &valexpr_context);
        if(!state_name){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for expression: ");
            valexpr_fprintf(&spawn->state_name_expr, stderr);
            fputc('\n', stderr);
            return 2;
        }

        body_t *new_body;
        err = body_add_body(body, &new_body,
            stateset_filename, state_name, palmapper,
            spawn->loc.pos, spawn->loc.rot, spawn->loc.turn);
        if(err)return err;

        new_body->just_spawned = true; /* don't do "step" stuff this frame */
        new_body->cooldown += _get_rgraph_delay(new_body->state->rgraph);

        hexgame_state_context_t sub_context = *context;
        sub_context.your_body = new_body;

        err = _apply_sub_effects(effect,
            spawn->effects_len,
            spawn->effects,
            &sub_context);
        if(err)return err;

        break;
    }
    case STATE_EFFECT_TYPE_PLAY: {
        CHECK_BODY
        const char *play_filename = effect->u.play_filename;
        err = body_load_recording(body, play_filename, false);
        if(err)return err;
        if(actor)hexgame_location_apply(&body->recording.loc0, &actor->trf);
        err = body_play_recording(body);
        if(err)return err;
        break;
    }
    case STATE_EFFECT_TYPE_SHOW_MINIMAP: {
        game->show_minimap = effect->u.i;
    }
    case STATE_EFFECT_TYPE_DIE: {
        CHECK_BODY
        body->dead = effect->u.dead;
        break;
    }
    case STATE_EFFECT_TYPE_REMOVE: {
        CHECK_BODY
        body->remove = true; /* mark for removal */
        break;
    }
    case STATE_EFFECT_TYPE_CONTINUE: {
        if(!continues_ptr){
            RULE_PERROR()
            fprintf(stderr, "Can't use \"continue\" here\n");
            return 2;
        }
        *continues_ptr = true;
        break;
    }
    case STATE_EFFECT_TYPE_CONFUSED: {
        CHECK_BODY
        effect_apply_boolean(effect->u.boolean, &body->confused);
        break;
    }
    case STATE_EFFECT_TYPE_KEY: {
        CHECK_BODY
        int key_i = body_get_key_i(body, effect->u.key.c);
        bool keydown = effect->u.key.action & 0x1;
        bool keyup = effect->u.key.action & 0x2;
        if(anim_debug){
            fprintf(stderr, "Key: ");
            if(keydown)fprintf(stderr, "down ");
            if(keyup)fprintf(stderr, "up ");
            fprintf(stderr, "%c\n", effect->u.key.c);
        }
        if(keydown){
            body_keydown(body, key_i);
        }
        if(keyup){
            body_keyup(body, key_i);
        }
        break;
    }
    case STATE_EFFECT_TYPE_INC:
    case STATE_EFFECT_TYPE_DEC:
    case STATE_EFFECT_TYPE_SET:
    case STATE_EFFECT_TYPE_UNSET: {
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        valexpr_result_t var_result = {0};
        err = valexpr_set(&effect->u.set.var_expr, &valexpr_context,
            &var_result);
        if(err)return err;
        vars_t *vars = var_result.vars;
        var_t *var = var_result.var;
        if(!vars){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get var for expression: ");
            valexpr_fprintf(&effect->u.set.val_expr, stderr);
            fputc('\n', stderr);
            fprintf(stderr, "...maybe you tried to assign to a literal?..\n");
            return 2;
        }

        if(effect->type == STATE_EFFECT_TYPE_UNSET){
            /* TODO: decide whether we want to "fix" this (although it might
            be tricky, since valexpr_get and friends currently return a val_t,
            whereas I believe for \"unset\", we would want... not even just a
            var_t, but really a var_t's parent vars_t...
            Really, we'd need a whole new valexpr_unset function. */
            fprintf(stderr,
                "WARNING: \"unset\" doesn't actually unset things, "
                "it just sets them to null.\n");
        }

        valexpr_result_t val_result = {0};
        err = valexpr_get(&effect->u.set.val_expr, &valexpr_context,
            &val_result);
        if(err)return err;
        val_t *val_val = val_result.val;
        if(val_val == NULL){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for expression: ");
            valexpr_fprintf(&effect->u.set.val_expr, stderr);
            fputc('\n', stderr);
            return 2;
        }

        if(
            effect->type == STATE_EFFECT_TYPE_INC ||
            effect->type == STATE_EFFECT_TYPE_DEC
        ){
            if(val_val->type != VAL_TYPE_INT){
                RULE_PERROR()
                fprintf(stderr, "Increment expects RHS to be int. Got: ");
                val_fprintf(val_val, stderr);
                fputc('\n', stderr);
                fprintf(stderr, "...expression was: ");
                valexpr_fprintf(&effect->u.set.val_expr, stderr);
                fputc('\n', stderr);
                return 2;
            }
            int inc = val_get_int(val_val);
            if(effect->type == STATE_EFFECT_TYPE_DEC)inc = -inc;

            val_set_int(&var->value, val_get_int(&var->value) + inc);
            err = vars_callback(vars, var);
            if(err)return err;
        }else{
            /* STATE_EFFECT_TYPE_SET */
            err = val_copy(&var->value, val_val);
            if(err)return err;
            err = vars_callback(vars, var);
            if(err)return err;
        }

        break;
    }
    case STATE_EFFECT_TYPE_IF: {
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        state_effect_ite_t *ite = effect->u.ite;

        bool matched = true;
        for(int i = 0; i < ite->conds_len; i++){
            state_cond_t *cond = ite->conds[i];
            err = state_cond_match(cond, context, &matched);
            if(err)return err;
            if(!matched)break;
        }

        state_effect_t **effects = matched?
            ite->then_effects: ite->else_effects;
        int effects_len = matched?
            ite->then_effects_len: ite->else_effects_len;

        for(int i = 0; i < effects_len; i++){
            state_effect_t *effect = effects[i];
            err = state_effect_apply(effect, context, gotto_ptr,
                continues_ptr);
            if(err){
                if(err == 2){
                    fprintf(stderr, "...in \"if\" statement\n");
                }
                return err;
            }
        }
        break;
    }
    case STATE_EFFECT_TYPE_AS: {
        switch(effect->u.as.type){
            case AS_YOU: {
                if(!your_body){
                    fprintf(stderr, "No your_body!\n");
                    return 2;
                }

                /* We apply our sub-effects "as you", that is, as your_body.
                The terminology here is just too silly, eh?
                Anyway, long story short, sub_context is like context but with
                body and your_body swapped. */
                hexgame_state_context_t sub_context = *context;
                sub_context.body = your_body;
                sub_context.your_body = body;

                err = _apply_sub_effects(effect,
                    effect->u.as.sub_effects_len,
                    effect->u.as.sub_effects,
                    &sub_context);
                if(err)return err;
                break;
            }
            default:
                fprintf(stderr, "Unrecognized \"as\" type: %i\n",
                    effect->u.as.type);
                return 2;
        }
        break;
    }
    default: {
        fprintf(stderr, "Unrecognized effect: %s\n",
            state_effect_type_name(effect->type));
        return 2;
    }
    }

    return 0;
    #undef RULE_PERROR
}

static int state_rule_apply(state_rule_t *rule,
    hexgame_state_context_t *context,
    state_effect_goto_t **gotto_ptr, bool *continues_ptr
){
    int err;

    /* NOTE: body and/or actor may be NULL.
    See comment on rule_match. */

    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        err = state_effect_apply(effect, context, gotto_ptr, continues_ptr);
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
    hexgame_state_context_t *context,
    state_effect_goto_t **gotto_ptr
){
    int err;

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];

        bool matched;
        err = state_rule_match(rule, context, &matched);
        if(err)return err;

        if(matched){
            if(hexgame_state_context_debug(context)){
                fprintf(stderr, "Matched rule: ");
                if(rule->name != NULL)fprintf(stderr, "\"%s\"", rule->name);
                else fprintf(stderr, "#%i", i);
                fprintf(stderr, "\n");
            }
            bool continues = false;
            err = state_rule_apply(rule, context, gotto_ptr, &continues);
            if(err)return err;
            if(!continues)break;
        }
    }

    return 0;
}

int collmsg_handler_apply(collmsg_handler_t *handler,
    hexgame_state_context_t *context,
    bool *continues_ptr
){
    int err;

    for(int i = 0; i < handler->effects_len; i++){
        state_effect_t *effect = handler->effects[i];

        state_effect_goto_t *gotto = NULL;
        err = state_effect_apply(effect, context, &gotto, continues_ptr);
        if(err)return err;

        actor_t *actor = context->actor;
        body_t *body = context->body;
        body_t *your_body = context->your_body;

        if(gotto != NULL){
            if(actor != NULL){
                /* ACTOR */
                err = state_effect_goto_apply_to_actor(gotto, actor);
                if(err)return err;
                if(gotto->immediate){
                    /* If there was an "immediate goto" effect,
                    then we immediately handle the new state's rules */
                    err = actor_handle_rules(actor, your_body);
                    if(err)return err;
                }
            }else{
                /* BODY */
                err = state_effect_goto_apply_to_body(gotto, body);
                if(err)return err;
                if(gotto->immediate){
                    /* If there was an "immediate goto" effect,
                    then we immediately handle the new state's rules */
                    err = body_handle_rules(body, your_body);
                    if(err)return err;
                }
            }
        }
    }
    return 0;
}
