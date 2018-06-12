

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "array.h"
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

int stateset_load(stateset_t *stateset, const char *filename){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text);
    if(err)return err;

    err = stateset_init(stateset);
    if(err)return err;

    err = stateset_parse(stateset, &lexer);
    if(err)return err;

    free(text);
    return 0;
}

int stateset_parse(stateset_t *stateset, fus_lexer_t *lexer){
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

                    while(1){
                        err = fus_lexer_next(lexer);
                        if(err)return err;

                        if(fus_lexer_got(lexer, ")"))break;

                        char *line;
                        err = fus_lexer_get_str(lexer, &line);
                        if(err)return err;
                        /* TODO: parse them lines... */
                        free(line);
                    }

                    ARRAY_PUSH_NEW(state_cond_t, *rule, conds, cond)
                    cond->type = state_cond_type_coll;
                    cond->u.coll.flags = flags;
                }else{
                    return fus_lexer_unexpected(lexer, NULL);
                }
            }

            err = fus_lexer_expect(lexer, "then");
            if(err)return err;
            err = fus_lexer_expect(lexer, "(");
            if(err)return err;
            err = fus_lexer_parse_silent(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
    }
    return 0;
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
const char state_effect_type_die[] = "die";
const char *state_effect_types[] = {
    state_effect_type_move,
    state_effect_type_rot,
    state_effect_type_turn,
    state_effect_type_goto,
    state_effect_type_die,
    NULL
};


void state_cleanup(state_t *state){
    free(state->name);

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];
        ARRAY_FREE(state_cond_t, *rule, conds, (void));
        ARRAY_FREE(state_effect_t, *rule, effects, (void));
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
                fprintf(f, ": %c", cond->u.key);
            }
            fprintf(f, "\n");
        }
        fprintf(f, "%s    then:\n", spaces);
        for(int i = 0; i < rule->effects_len; i++){
            state_effect_t *effect = rule->effects[i];
            fprintf(f, "%s      effect: %p\n", spaces, effect);
        }
    }
}

