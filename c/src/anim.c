

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "array.h"


void state_cleanup(state_t *state){
    free(state->name);
    ARRAY_FREE(state_rule_t, *state, rules, state_rule_cleanup)
}

int state_init(state_t *state, char *name){
    state->name = name;
    ARRAY_INIT(*state, rules)
    return 0;
}

void state_dump(state_t *state, FILE *f){
    fprintf(f, "state: %p\n", state);
    if(state == NULL)return;
    fprintf(f, "  name: %s\n", state->name);
    for(int i = 0; i < state->rules_len; i++){
    }
}

int state_parse(fus_lexer_t *lexer, state_t **state_ptr){
    return 0;
}




void state_rule_cleanup(state_rule_t *rule){
}

int state_rule_init(state_rule_t *rule){
    return 0;
}

void state_rule_dump(state_rule_t *rule, FILE *f){
}

