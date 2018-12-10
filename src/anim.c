

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "array.h"
#include "hexmap.h"
#include "util.h"
#include "prismelrenderer.h"



/************
 * STATESET *
 ************/

void stateset_cleanup(stateset_t *stateset){
    free(stateset->filename);
    ARRAY_FREE_PTR(state_t*, stateset->states, state_cleanup)
}

int stateset_init(stateset_t *stateset, char *filename){
    stateset->filename = filename;
    ARRAY_INIT(stateset->states)
    return 0;
}

int stateset_load(stateset_t *stateset, char *filename,
    prismelrenderer_t *prend, vecspace_t *space
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = stateset_init(stateset, filename);
    if(err)return err;

    err = stateset_parse(stateset, &lexer, prend, space);
    if(err)return err;

    free(text);
    return 0;
}

int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    prismelrenderer_t *prend, vecspace_t *space
){
    int err;

    while(1){
        if(fus_lexer_done(lexer))break;

        char *name;
        err = fus_lexer_get_name(lexer, &name);
        if(err)return err;

        err = fus_lexer_get(lexer, "(");
        if(err)return err;

        char *rgraph_name;
        err = fus_lexer_get(lexer, "rgraph");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &rgraph_name);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
        rendergraph_t *rgraph = prismelrenderer_get_rendergraph(
            prend, rgraph_name);
        if(rgraph == NULL){
            fprintf(stderr, "Couldn't find shape: %s\n", rgraph_name);
            free(rgraph_name); return 2;}
        free(rgraph_name);

        ARRAY_PUSH_NEW(state_t*, stateset->states, state)
        err = state_init(state, stateset, name, rgraph);
        if(err)return err;

        if(fus_lexer_got(lexer, "hitbox")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;

            hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
            if(collmap == NULL)return 1;
            err = hexcollmap_init(collmap, space,
                strdup(lexer->filename));
            if(err)return err;
            err = hexcollmap_parse(collmap, lexer, true);
            if(err)return err;
            state->hitbox = collmap;

            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        if(fus_lexer_got(lexer, "crushbox")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;

            if(fus_lexer_got(lexer, "hitbox")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                state->crushbox = state->hitbox;
            }else{
                hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
                if(collmap == NULL)return 1;
                err = hexcollmap_init(collmap, space,
                    strdup(lexer->filename));
                if(err)return err;
                err = hexcollmap_parse(collmap, lexer, true);
                if(err)return err;
                state->crushbox = collmap;
            }

            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            ARRAY_PUSH_NEW(state_rule_t*, state->rules, rule)
            err = state_rule_init(rule, state);
            if(err)return err;

            err = fus_lexer_get(lexer, "if");
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            while(1){
                if(fus_lexer_got(lexer, ")")){
                    break;
                }else if(fus_lexer_got(lexer, "false")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_cond_t*, rule->conds, cond)
                    cond->type = state_cond_type_false;
                }else if(fus_lexer_got(lexer, "key")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;

                    bool yes = true;
                    if(fus_lexer_got(lexer, "not")){
                        err = fus_lexer_next(lexer);
                        if(err)return err;
                        yes = false;
                    }

                    int kstate;
                    if(fus_lexer_got(lexer, "isdown")){
                        kstate = 0;
                    }else if(fus_lexer_got(lexer, "wasdown")){
                        kstate = 1;
                    }else if(fus_lexer_got(lexer, "wentdown")){
                        kstate = 2;
                    }else{
                        return fus_lexer_unexpected(lexer,
                            "isdown or wasdown or wentdown");
                    }
                    err = fus_lexer_next(lexer);
                    if(err)return err;

                    char *name;
                    err = fus_lexer_get_name(lexer, &name);
                    if(err)return err;

                    char c = name[0];
                    if(strlen(name) != 1 || !strchr(ANIM_KEY_CS, c)){
                        return fus_lexer_unexpected(lexer,
                            "one of the characters: " ANIM_KEY_CS);
                    }

                    ARRAY_PUSH_NEW(state_cond_t*, rule->conds, cond)
                    cond->type = state_cond_type_key;
                    cond->u.key.kstate = kstate;
                    cond->u.key.c = c;
                    cond->u.key.yes = yes;
                    free(name);

                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "coll")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;

                    int flags = 0;

                    if(fus_lexer_got(lexer, "all"))flags ^= 1;
                    else if(fus_lexer_got(lexer, "any"))/* don't do nuthin */;
                    else return fus_lexer_unexpected(lexer, "all or any");
                    err = fus_lexer_next(lexer);
                    if(err)return err;

                    if(fus_lexer_got(lexer, "yes"))flags ^= 2;
                    else if(fus_lexer_got(lexer, "no"))/* dinnae move a muscle */;
                    else return fus_lexer_unexpected(lexer, "yes or no");
                    err = fus_lexer_next(lexer);
                    if(err)return err;

                    hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
                    if(collmap == NULL)return 1;
                    err = hexcollmap_init(collmap, space,
                        strdup(lexer->filename));
                    if(err)return err;
                    err = hexcollmap_parse(collmap, lexer, true);
                    if(err)return err;

                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_cond_t*, rule->conds, cond)
                    cond->type = state_cond_type_coll;
                    cond->u.coll.collmap = collmap;
                    cond->u.coll.flags = flags;
                }else if(fus_lexer_got(lexer, "chance")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;
                    int percent = 0;
                    err = fus_lexer_get_int(lexer, &percent);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "%");
                    if(err)return err;
                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_cond_t*, rule->conds, cond)
                    cond->type = state_cond_type_chance;
                    cond->u.percent = percent;
                }else{
                    return fus_lexer_unexpected(lexer, NULL);
                }
            }
            err = fus_lexer_next(lexer);
            if(err)return err;

            err = fus_lexer_get(lexer, "then");
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            while(1){
                if(fus_lexer_got(lexer, ")")){
                    break;
                }else if(fus_lexer_got(lexer, "print")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;
                    char *msg;
                    err = fus_lexer_get_str(lexer, &msg);
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_print;
                    effect->u.msg = msg;
                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "move")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_move;
                    for(int i = 0; i < space->dims; i++){
                        err = fus_lexer_get_int(lexer, &effect->u.vec[i]);
                        if(err)return err;
                    }
                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "rot")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;
                    int rot;
                    err = fus_lexer_get_int(lexer, &rot);
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_rot;
                    effect->u.rot = rot_contain(space->rot_max, rot);
                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "turn")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_turn;
                }else if(fus_lexer_got(lexer, "goto")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;

                    char *goto_name;
                    err = fus_lexer_get_name(lexer, &goto_name);
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_goto;
                    effect->u.goto_name = goto_name;

                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "delay")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;

                    int delay;
                    err = fus_lexer_get_int(lexer, &delay);
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_delay;
                    effect->u.delay = delay;

                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "action")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    err = fus_lexer_get(lexer, "(");
                    if(err)return err;

                    char *action_name;
                    err = fus_lexer_get_name(lexer, &action_name);
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_action;
                    effect->u.action_name = action_name;

                    err = fus_lexer_get(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "die")){
                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_effect_t*, rule->effects, effect)
                    effect->type = state_effect_type_die;
                }else{
                    return fus_lexer_unexpected(lexer, NULL);
                }
            }
            err = fus_lexer_next(lexer);
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;
    return 0;
}

state_t *stateset_get_state(stateset_t *stateset, const char *name){
    for(int i = 0; i < stateset->states_len; i++){
        state_t *state = stateset->states[i];
        if(!strcmp(state->name, name))return state;
    }
    return NULL;
}



/*********
 * STATE *
 *********/


const char state_cond_type_false[] = "false";
const char state_cond_type_key[] = "key";
const char state_cond_type_coll[] = "coll";
const char state_cond_type_chance[] = "chance";
const char *state_cond_types[] = {
    state_cond_type_false,
    state_cond_type_key,
    state_cond_type_coll,
    state_cond_type_chance,
    NULL
};


const char state_effect_type_print[] = "print";
const char state_effect_type_move[] = "move";
const char state_effect_type_rot[] = "rot";
const char state_effect_type_turn[] = "turn";
const char state_effect_type_goto[] = "goto";
const char state_effect_type_delay[] = "delay";
const char state_effect_type_action[] = "action";
const char state_effect_type_die[] = "die";
const char *state_effect_types[] = {
    state_effect_type_print,
    state_effect_type_move,
    state_effect_type_rot,
    state_effect_type_turn,
    state_effect_type_goto,
    state_effect_type_delay,
    state_effect_type_action,
    state_effect_type_die,
    NULL
};


void state_cleanup(state_t *state){
    free(state->name);
    if(state->hitbox != NULL){

        /* Don't free the same hexcollmap twice! */
        if(state->crushbox == state->hitbox)state->crushbox = NULL;

        hexcollmap_cleanup(state->hitbox);
        free(state->hitbox);
    }
    if(state->crushbox != NULL){
        hexcollmap_cleanup(state->crushbox);
        free(state->crushbox);
    }
    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];
        state_rule_cleanup(rule);
    }
    free(state->rules);
}

int state_init(state_t *state, stateset_t *stateset, char *name,
    rendergraph_t *rgraph
){
    state->stateset = stateset;
    state->name = name;
    state->rgraph = rgraph;
    state->hitbox = NULL;
    state->crushbox = NULL;
    ARRAY_INIT(state->rules)
    return 0;
}


void state_rule_cleanup(state_rule_t *rule){
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        if(cond->type == state_cond_type_coll){
            hexcollmap_t *collmap = cond->u.coll.collmap;
            if(collmap != NULL){
                hexcollmap_cleanup(collmap);
                free(collmap);
            }
        }
    }
    free(rule->conds);

    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(effect->type == state_effect_type_print){
            free(effect->u.msg);
        }else if(effect->type == state_effect_type_goto){
            free(effect->u.goto_name);
        }else if(effect->type == state_effect_type_action){
            free(effect->u.action_name);
        }
    }
    free(rule->effects);
}

int state_rule_init(state_rule_t *rule, state_t *state){
    rule->state = state;
    ARRAY_INIT(rule->conds)
    ARRAY_INIT(rule->effects)
    return 0;
}

