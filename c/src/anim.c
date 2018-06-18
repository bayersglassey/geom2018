

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "array.h"
#include "hexmap.h"
#include "util.h"



/************
 * STATESET *
 ************/

void stateset_cleanup(stateset_t *stateset){
    ARRAY_FREE(state_t, *stateset, states, state_cleanup)
}

int stateset_init(stateset_t *stateset){
    ARRAY_INIT(*stateset, states)
    return 0;
}

void stateset_dump(stateset_t *stateset, FILE *f){
    fprintf(f, "stateset: %p\n", stateset);
    if(stateset == NULL)return;
    for(int i = 0; i < stateset->states_len; i++){
        state_dump(stateset->states[i], f, 2);
    }
}

int stateset_load(stateset_t *stateset, const char *filename,
    vecspace_t *space
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = stateset_init(stateset);
    if(err)return err;

    err = stateset_parse(stateset, &lexer, space);
    if(err)return err;

    free(text);
    return 0;
}

int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer,
    vecspace_t *space
){
    int err;

    while(1){
        err = fus_lexer_next(lexer);
        if(err)return err;

        if(fus_lexer_done(lexer))break;

        char *name;
        err = fus_lexer_get_name(lexer, &name);
        if(err)return err;
        ARRAY_PUSH_NEW(state_t, *stateset, states, state)
        err = state_init(state, name);
        if(err)return err;

        err = fus_lexer_expect(lexer, "(");
        if(err)return err;
        while(1){
            err = fus_lexer_next(lexer);
            if(err)return err;

            if(fus_lexer_got(lexer, ")"))break;

            ARRAY_PUSH_NEW(state_rule_t, *state, rules, rule)

            err = fus_lexer_get(lexer, "if");
            if(err)return err;
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            while(1){
                err = fus_lexer_next(lexer);
                if(err)return err;
                if(fus_lexer_got(lexer, ")")){
                    break;
                }else if(fus_lexer_got(lexer, "kdown")){
                    char *name;

                    err = fus_lexer_expect(lexer, "(");
                    if(err)return err;
                    err = fus_lexer_expect_name(lexer, &name);
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_cond_t, *rule, conds, cond)
                    cond->type = state_cond_type_kdown;
                    cond->u.key = name[0];
                    free(name);

                    err = fus_lexer_expect(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "coll")){
                    err = fus_lexer_expect(lexer, "(");
                    if(err)return err;

                    int flags = 0;

                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    if(fus_lexer_got(lexer, "all"))flags ^= 1;
                    else if(fus_lexer_got(lexer, "any"))/* don't do nuthin */;
                    else return fus_lexer_unexpected(lexer, "all or any");

                    err = fus_lexer_next(lexer);
                    if(err)return err;
                    if(fus_lexer_got(lexer, "yes"))flags ^= 2;
                    else if(fus_lexer_got(lexer, "no"))/* dinnae move a muscle */;
                    else return fus_lexer_unexpected(lexer, "yes or no");

                    hexcollmap_t *collmap = calloc(1, sizeof(*collmap));
                    if(collmap == NULL)return 1;
                    err = hexcollmap_init(collmap);
                    if(err)return err;
                    err = hexcollmap_parse(collmap, lexer);
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_cond_t, *rule, conds, cond)
                    cond->type = state_cond_type_coll;
                    cond->u.coll.collmap = collmap;
                    cond->u.coll.flags = flags;
                }else{
                    return fus_lexer_unexpected(lexer, NULL);
                }
            }

            err = fus_lexer_expect(lexer, "then");
            if(err)return err;
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            while(1){
                err = fus_lexer_next(lexer);
                if(err)return err;
                if(fus_lexer_got(lexer, ")")){
                    break;
                }else if(fus_lexer_got(lexer, "move")){
                    err = fus_lexer_expect(lexer, "(");
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_effect_t, *rule, effects, effect)
                    effect->type = state_effect_type_move;
                    for(int i = 0; i < space->dims; i++){
                        err = fus_lexer_expect_int(lexer, &effect->u.vec[i]);
                        if(err)return err;
                    }
                    err = fus_lexer_expect(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "rot")){
                    err = fus_lexer_expect(lexer, "(");
                    if(err)return err;
                    int rot;
                    err = fus_lexer_expect_int(lexer, &rot);
                    if(err)return err;
                    ARRAY_PUSH_NEW(state_effect_t, *rule, effects, effect)
                    effect->type = state_effect_type_rot;
                    effect->u.rot = rot_contain(space->rot_max, rot);
                    err = fus_lexer_expect(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "turn")){
                    ARRAY_PUSH_NEW(state_effect_t, *rule, effects, effect)
                    effect->type = state_effect_type_turn;
                }else if(fus_lexer_got(lexer, "goto")){
                    err = fus_lexer_expect(lexer, "(");
                    if(err)return err;

                    char *goto_name;
                    err = fus_lexer_expect_name(lexer, &goto_name);
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_effect_t, *rule, effects, effect)
                    effect->type = state_effect_type_goto;
                    effect->u.goto_name = goto_name;

                    err = fus_lexer_expect(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "delay")){
                    err = fus_lexer_expect(lexer, "(");
                    if(err)return err;

                    int delay;
                    err = fus_lexer_expect_int(lexer, &delay);
                    if(err)return err;

                    ARRAY_PUSH_NEW(state_effect_t, *rule, effects, effect)
                    effect->type = state_effect_type_delay;
                    effect->u.delay = delay;

                    err = fus_lexer_expect(lexer, ")");
                    if(err)return err;
                }else if(fus_lexer_got(lexer, "die")){
                    ARRAY_PUSH_NEW(state_effect_t, *rule, effects, effect)
                    effect->type = state_effect_type_die;
                }else{
                    return fus_lexer_unexpected(lexer, NULL);
                }
            }
        }
    }
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


const char state_cond_type_kdown[] = "kdown";
const char state_cond_type_coll[] = "coll";
const char *state_cond_types[] = {
    state_cond_type_kdown,
    state_cond_type_coll,
    NULL
};


const char state_effect_type_move[] = "move";
const char state_effect_type_rot[] = "rot";
const char state_effect_type_turn[] = "turn";
const char state_effect_type_goto[] = "goto";
const char state_effect_type_delay[] = "delay";
const char state_effect_type_die[] = "die";
const char *state_effect_types[] = {
    state_effect_type_move,
    state_effect_type_rot,
    state_effect_type_turn,
    state_effect_type_goto,
    state_effect_type_delay,
    state_effect_type_die,
    NULL
};


void state_cleanup(state_t *state){
    free(state->name);

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];

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
            if(effect->type == state_effect_type_goto){
                free(effect->u.goto_name);
            }
        }
        free(rule->effects);
    }
    free(state->rules);
}

int state_init(state_t *state, char *name){
    state->name = name;
    ARRAY_INIT(*state, rules)
    return 0;
}

void state_dump(state_t *state, FILE *f, int n_spaces){
    char spaces[20];
    get_spaces(spaces, 20, n_spaces);

    fprintf(f, "%sstate: %p\n", spaces, state);
    if(state == NULL)return;
    fprintf(f, "%s  name: %s\n", spaces, state->name);
    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];
        fprintf(f, "%s    if:\n", spaces);
        for(int i = 0; i < rule->conds_len; i++){
            state_cond_t *cond = rule->conds[i];
            fprintf(f, "%s      %s", spaces, cond->type);
            if(cond->type == state_cond_type_kdown){
                fprintf(f, ": %c\n", cond->u.key);
            }else if(cond->type == state_cond_type_coll){
                fprintf(f, ": %s %s\n",
                    (cond->u.coll.flags & 1)? "all": "any",
                    (cond->u.coll.flags & 2)? "yes": "no");
                hexcollmap_dump(cond->u.coll.collmap,
                    f, n_spaces + 8);
            }else{
                fprintf(f, "\n");
            }
        }
        fprintf(f, "%s    then:\n", spaces);
        for(int i = 0; i < rule->effects_len; i++){
            state_effect_t *effect = rule->effects[i];
            fprintf(f, "%s      %s", spaces, effect->type);
            if(effect->type == state_effect_type_move){
                int *vec = effect->u.vec;
                fprintf(f, ":");
                for(int i = 0; i < 4; i++)fprintf(f, " %i", vec[i]);
                fprintf(f, "\n");
            }else if(effect->type == state_effect_type_rot){
                fprintf(f, ": %i\n", effect->u.rot);
            }else if(effect->type == state_effect_type_turn){
                fprintf(f, "\n");
            }else if(effect->type == state_effect_type_goto){
                fprintf(f, ": %s\n", effect->u.goto_name);
            }else if(effect->type == state_effect_type_die){
                fprintf(f, "\n");
            }else{
                fprintf(f, "\n");
            }
        }
    }
}

