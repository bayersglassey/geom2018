

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
        body->cooldown = _get_rgraph_delay(body->state->rgraph);
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
        actor->wait = _get_rgraph_delay(actor->state->rgraph);
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

            const char *collmsg = cond->u.coll.collmsg;

            if(against_bodies){
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

        val_t *val1;
        err = valexpr_get(&cond->u.expr.val1_expr,
            &valexpr_context, &val1);
        if(err)return err;
        if(val1 == NULL){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for LHS: ");
            valexpr_fprintf(&cond->u.expr.val1_expr, stderr);
            fputc('\n', stderr);
            return 2;
        }

        val_t *val2;
        err = valexpr_get(&cond->u.expr.val2_expr,
            &valexpr_context, &val2);
        if(err)return err;
        if(val2 == NULL){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for RHS: ");
            valexpr_fprintf(&cond->u.expr.val2_expr, stderr);
            fputc('\n', stderr);
            return 2;
        }

        if(!val_type_eq(val1, val2)){
            RULE_PERROR()
            fprintf(stderr, "Type mismatch between LHS, RHS: ");
            val_fprintf(val1, stderr);
            fputs(", ", stderr);
            val_fprintf(val2, stderr);
            fputc('\n', stderr);
            fprintf(stderr, "...LHS was: ");
            valexpr_fprintf(&cond->u.expr.val1_expr, stderr);
            fputc('\n', stderr);
            fprintf(stderr, "...RHS was: ");
            valexpr_fprintf(&cond->u.expr.val2_expr, stderr);
            fputc('\n', stderr);
            return 2;
        }

        int op = cond->u.expr.op;
        switch(op){
            case STATE_COND_EXPR_OP_EQ: matched = val_eq(val1, val2); break;
            case STATE_COND_EXPR_OP_NE: matched = val_ne(val1, val2); break;
            case STATE_COND_EXPR_OP_LT: matched = val_lt(val1, val2); break;
            case STATE_COND_EXPR_OP_LE: matched = val_le(val1, val2); break;
            case STATE_COND_EXPR_OP_GT: matched = val_gt(val1, val2); break;
            case STATE_COND_EXPR_OP_GE: matched = val_ge(val1, val2); break;
            default:
                fprintf(stderr, "Bad expr op: %i\n", op);
                RULE_PERROR()
                return 2;
        }
        break;
    }
    case STATE_COND_TYPE_GET_BOOL: {
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

        val_t *val;
        err = valexpr_get(&cond->u.valexpr, &valexpr_context, &val);
        if(err)return err;

        matched = val != NULL;
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
        if(DEBUG_RULES)printf("  if: %s\n", state_cond_type_name(cond->type));
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

    switch(effect->type){
    case STATE_EFFECT_TYPE_NOOP: break;
    case STATE_EFFECT_TYPE_PRINT:
    case STATE_EFFECT_TYPE_PRINT_VAR:
    case STATE_EFFECT_TYPE_PRINT_VARS: {
        if(body != NULL)fprintf(stderr, "body %p", body);
        else fprintf(stderr, "unknown body");
        if(actor != NULL)fprintf(stderr, " (actor %p)", actor);

        vars_t *vars = actor? &actor->vars: &body->vars;
        if(effect->type == STATE_EFFECT_TYPE_PRINT_VARS){
            fprintf(stderr, " vars:\n");
            vars_write(vars, stderr, 1);
        }else if(effect->type == STATE_EFFECT_TYPE_PRINT_VAR){
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
    case STATE_EFFECT_TYPE_GOTO: {
        *gotto_ptr = &effect->u.gotto;
        break;
    }
    case STATE_EFFECT_TYPE_DELAY: {
        if(actor){
            actor->wait = effect->u.delay;
        }else{
            CHECK_BODY
            body->cooldown = effect->u.delay;
        }
        break;
    }
    case STATE_EFFECT_TYPE_SPAWN: {
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
        break;
    }
    case STATE_EFFECT_TYPE_PLAY: {
        CHECK_ACTOR
        const char *play_filename = effect->u.play_filename;
        err = body_load_recording(body, play_filename, false);
        if(err)return err;
        hexgame_location_apply(&body->recording.loc0, &actor->trf);
        err = body_play_recording(body);
        if(err)return err;
        break;
    }
    case STATE_EFFECT_TYPE_DIE: {
        CHECK_BODY
        body->dead = effect->u.dead;
        break;
    }
    case STATE_EFFECT_TYPE_CONTINUE: {
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
        if(keydown){
            body_keydown(body, key_i);
        }
        bool keyup = effect->u.key.action & 0x2;
        if(keyup){
            body_keyup(body, key_i);
        }
        break;
    }
    case STATE_EFFECT_TYPE_INC:
    case STATE_EFFECT_TYPE_DEC:
    case STATE_EFFECT_TYPE_SET: {
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        val_t *var_val;
        err = valexpr_set(&effect->u.set.var_expr, &valexpr_context,
            &var_val);
        if(err)return err;
        /* NOTE: valexpr_set guarantees that we find (or create, if
        necessary) a val, so we don't need to check whether var_val is
        NULL. */

        val_t *val_val;
        err = valexpr_get(&effect->u.set.val_expr, &valexpr_context,
            &val_val);
        if(err)return err;
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
            val_set_int(var_val, val_get_int(var_val) + inc);
        }else{
            /* STATE_EFFECT_TYPE_SET */
            err = val_copy(var_val, val_val);
            if(err)return err;
        }
        break;
    }
    case STATE_EFFECT_TYPE_SET_LABEL: {
        valexpr_context_t valexpr_context = {0};
        err = _get_vars(context, &valexpr_context);
        if(err)return err;

        val_t *label_name_val;
        err = valexpr_get(&effect->u.set.var_expr, &valexpr_context,
            &label_name_val);
        if(err)return err;
        const char *_label_name = label_name_val?
            val_get_str(label_name_val): NULL;
        if(_label_name == NULL){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for label name: ");
            valexpr_fprintf(&effect->u.set.var_expr, stderr);
            fputc('\n', stderr);
            if(label_name_val != NULL){
                fprintf(stderr, "(Found the val, but it wasn't a string: ");
                val_fprintf(label_name_val, stderr);
                fputs(")\n", stderr);
            }
            return 2;
        }

        /* _label_name might belong to a val of type VAL_TYPE_STR, which
        might get freed later, so we convert to a const char* owned by a
        stringstore here. */
        const char *label_name = stringstore_get(&prend->name_store,
            _label_name);
        if(!label_name)return 1;

        val_t *rgraph_name_val;
        err = valexpr_get(&effect->u.set.val_expr, &valexpr_context,
            &rgraph_name_val);
        if(err)return err;

        /* We are ok if either:
            * we got a str val
            * we got null val
        */
        bool ok = true;
        const char *rgraph_name = NULL;
        if(!rgraph_name_val){
            ok = false;
        }else if(rgraph_name_val->type == VAL_TYPE_NULL){
            /* This is ok */
        }else{
            rgraph_name = val_get_str(rgraph_name_val);
            if(!rgraph_name)ok = false;
        }

        if(!ok){
            RULE_PERROR()
            fprintf(stderr, "Couldn't get value for rgraph name: ");
            valexpr_fprintf(&effect->u.set.val_expr, stderr);
            fputc('\n', stderr);
            if(rgraph_name_val != NULL){
                fprintf(stderr, "(Found the val, but it wasn't a str or null: ");
                val_fprintf(rgraph_name_val, stderr);
                fputs(")\n", stderr);
            }
            return 2;
        }

        /* Early exit if we're unsetting a label (by assigning it null) */
        if(rgraph_name_val->type == VAL_TYPE_NULL){
            err = body_unset_label_mapping(body, label_name);
            if(err)return err;
            break;
        }

        rendergraph_t *rgraph = prismelrenderer_get_rgraph(prend, rgraph_name);
        if(!rgraph){
            RULE_PERROR()
            fprintf(stderr, "Couldn't find rgraph: %s\n", rgraph_name);
            return 2;
        }

        err = body_set_label_mapping(body, label_name, rgraph);
        if(err)return err;
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
            if(DEBUG_RULES)printf("  %s: %s\n", matched? "then": "else",
                state_effect_type_name(effect->type));
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
            case EFFECT_AS_YOU: {
                if(!your_body){
                    fprintf(stderr, "No your_body!\n");
                    return 2;
                }

                /* We apply our sub-effects "as you", that is, as your_body.
                The terminology here is just too silly, eh?
                Anyway, long story short, sub_context is like context but with
                body and your_body swapped. */
                hexgame_state_context_t sub_context = *context;
                sub_context.your_body = body;
                sub_context.body = your_body;

                for(int i = 0; i < effect->u.as.sub_effects_len; i++){
                    state_effect_t *sub_effect = effect->u.as.sub_effects[i];
                    if(DEBUG_RULES)printf("  as: %s\n",
                        state_effect_type_name(sub_effect->type));

                    state_effect_goto_t *gotto = NULL;
                    bool continues = false; /* No effect if used */
                    err = state_effect_apply(sub_effect, &sub_context, &gotto,
                        &continues);
                    if(err){
                        if(err == 2){
                            fprintf(stderr, "...in \"as\" statement\n");
                        }
                        return err;
                    }

                    if(gotto != NULL){
                        /* SOMEWHAT HACKY:
                        We apply the goto to your_body.
                        The reason this is hacky is... uhhh... I'm not quite
                        sure, but imagine the following:
                            as you:
                                as you:
                                    goto: whatever
                        ...this behaves subtly differently than a plain
                        "goto: whatever", because the goto is applied
                        immediately, instead of bubbling up the C callstack
                        via gotto_ptr. */
                        err = state_effect_goto_apply_to_body(gotto, your_body);
                        if(err)return err;
                        if(gotto->immediate){
                            /* If there was an "immediate goto" effect,
                            then we immediately handle the new state's rules */
                            err = body_handle_rules(your_body, body);
                            if(err)return err;
                        }
                    }
                }
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
        if(DEBUG_RULES)printf("  then: %s\n", state_effect_type_name(effect->type));
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

        if(DEBUG_RULES){printf("body %p, actor %p, rule %i:\n",
            context->body, context->actor, i);}

        bool matched;
        err = state_rule_match(rule, context, &matched);
        if(err)return err;

        if(matched){
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
