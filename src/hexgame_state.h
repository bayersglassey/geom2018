#ifndef _HEXGAME_STATE_H_
#define _HEXGAME_STATE_H_

/* This file contains prototypes for functions dealing with structs from
both anim.h and hexgame.h */

#include <stdbool.h>

#include "anim.h"
#include "hexgame.h"

typedef struct hexgame_state_context {
    hexgame_t *game;
    body_t *body;
    actor_t *actor;
    body_t *your_body;
} hexgame_state_context_t;


int state_handle_rules(state_t *state,
    hexgame_state_context_t *context,
    state_effect_goto_t **gotto_ptr);

int state_effect_goto_apply_to_body(state_effect_goto_t *gotto,
    body_t *body);
int state_effect_goto_apply_to_actor(state_effect_goto_t *gotto,
    actor_t *actor);

int state_cond_match(state_cond_t *cond,
    hexgame_state_context_t *context,
    bool *matched_ptr);

int state_effect_apply(state_effect_t *effect,
    hexgame_state_context_t *context,
    state_effect_goto_t **gotto_ptr, bool *continues_ptr);

int collmsg_handler_apply(collmsg_handler_t *handler,
    hexgame_state_context_t *context,
    bool *continues_ptr);


#endif