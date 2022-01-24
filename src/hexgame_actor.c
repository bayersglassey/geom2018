

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
#include "hexgame_vars_props.h"



void actor_cleanup(actor_t *actor){
    vars_cleanup(&actor->vars);
}

int actor_init(actor_t *actor, hexmap_t *map, body_t *body,
    const char *stateset_filename, const char *state_name
){
    int err;

    memset(actor, 0, sizeof(*actor));

    vars_init_with_props(&actor->vars, hexgame_vars_prop_names);

    /* NOTE: body may be NULL */
    actor->body = body;

    actor->game = map->game;

    err = actor_init_stateset(actor, stateset_filename, state_name, map);
    if(err)return err;

    return 0;
}


int actor_init_stateset(actor_t *actor, const char *stateset_filename,
    const char *state_name, hexmap_t *map
){
    int err;

    err = hexgame_get_or_load_stateset(actor->game, stateset_filename,
        &actor->stateset);
    if(err)return err;

    if(state_name == NULL){
        state_name = actor->stateset->default_state_name;
    }

    err = actor_set_state(actor, state_name);
    if(err)return err;

    return 0;
}

int actor_set_state(actor_t *actor, const char *state_name){
    if(state_name == NULL){
        actor->state = NULL;
    }else{
        actor->state = stateset_get_state(actor->stateset, state_name);
        if(actor->state == NULL){
            fprintf(stderr, "Couldn't init actor stateset: "
                "couldn't find state %s in stateset %s\n",
                state_name, actor->stateset->filename);
            return 2;}
    }
    return 0;
}

int actor_handle_rules(actor_t *actor, body_t *your_body){
    int err;
    handle: {
        hexgame_state_context_t context = {
            .game = actor->game,
            .actor = actor,
            .body = actor->body,
            .your_body = your_body,
        };
        state_effect_goto_t *gotto = NULL;
        err = state_handle_rules(actor->state, &context, &gotto);
        if(err)return err;
        if(gotto != NULL){
            err = state_effect_goto_apply_to_actor(gotto, actor);
            if(err)return err;

            if(gotto->immediate)goto handle;
                /* If there was an "immediate goto" effect,
                then we immediately handle the new state's rules */
        }
    }
    return 0;
}

int actor_step(actor_t *actor, struct hexgame *game){
    int err;

    if(actor->wait > 0){
        actor->wait--;
        return 0;
    }

    body_t *body = actor->body;
    if(body == NULL || body->recording.action == 0){
        /* No body, or recording not playing */

        /* Handle current state's rules */
        err = actor_handle_rules(actor, NULL);
        if(err)return err;
    }
    return 0;
}

int actor_refresh_vars(actor_t *actor){
    int err;
    vars_t *vars = &actor->vars;

    /* At the moment, actor doesn't set any special vars... */

    return 0;
}
