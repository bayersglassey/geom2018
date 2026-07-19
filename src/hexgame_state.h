#ifndef _HEXGAME_STATE_H_
#define _HEXGAME_STATE_H_

/* This file contains prototypes for functions dealing with structs from
both anim.h and hexgame.h */

#include <stdbool.h>

#include "anim.h"
#include "hexgame.h"


typedef struct hexgame_state_controlflow {
    bool can_break; /* Allow STATE_EFFECT_TYPE_BREAK */
    bool can_continue; /* Allow STATE_EFFECT_TYPE_CONTINUE */
    bool should_break; /* E.g. via STATE_EFFECT_TYPE_BREAK, or "goto immediate", etc */
    bool should_continue; /* E.g. via STATE_EFFECT_TYPE_CONTINUE */
} hexgame_state_controlflow_t;

void hexgame_state_controlflow_init(hexgame_state_controlflow_t *controlflow, bool can_continue);
bool hexgame_state_controlflow_is_unrolling(hexgame_state_controlflow_t *controlflow);


typedef struct hexgame_state_context {
    hexgame_t *game;
    hexmap_t *map; /* NOTE: not necessarily body->map, see AS_MAP! */
    body_t *body;
    actor_t *actor;
    body_t *your_body;
    vars_t *procvars;
} hexgame_state_context_t;

bool hexgame_state_context_debug(hexgame_state_context_t *context);


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
    state_effect_goto_t **gotto_ptr,
    hexgame_state_controlflow_t *controlflow);

int collmsg_handler_apply(collmsg_handler_t *handler,
    hexgame_state_context_t *context);


#endif
