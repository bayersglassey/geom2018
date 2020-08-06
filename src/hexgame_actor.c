

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



void actor_cleanup(actor_t *actor){
    stateset_cleanup(&actor->stateset);
}

int actor_init(actor_t *actor, hexmap_t *map, body_t *body,
    const char *stateset_filename, const char *state_name
){
    int err;

    actor->body = body;

    err = actor_init_stateset(actor, stateset_filename, state_name, map);
    if(err)return err;

    return 0;
}


int actor_init_stateset(actor_t *actor, const char *stateset_filename,
    const char *state_name, hexmap_t *map
){
    int err;

    err = stateset_load(&actor->stateset, strdup(stateset_filename),
        NULL, map->prend, map->space);
    if(err)return err;

    if(state_name == NULL){
        state_name = actor->stateset.states[0]->name;
    }

    err = actor_set_state(actor, state_name);
    if(err)return err;

    return 0;
}

int actor_set_state(actor_t *actor, const char *state_name){
    if(state_name == NULL){
        actor->state = NULL;
    }else{
        actor->state = stateset_get_state(&actor->stateset, state_name);
        if(actor->state == NULL){
            fprintf(stderr, "Couldn't init actor stateset: "
                "couldn't find state %s in stateset %s\n",
                state_name, actor->stateset.filename);
            return 2;}
    }
    return 0;
}

int actor_step(actor_t *actor, struct hexgame *game){
    int err;
    body_t *body = actor->body;
    if(body == NULL || body->recording.action == 0){
        /* No body, or recording not playing */

        /* Handle current state's rules */
        handle: {
            state_effect_goto_t *gotto = NULL;
            err = state_handle_rules(actor->state, game, body, actor,
                &gotto);
            if(err)return err;
            if(gotto != NULL){
                err = actor_set_state(actor, gotto->name);
                if(err)return err;

                if(gotto->immediate)goto handle;
                    /* If there was an "immediate goto" effect,
                    then we immediately handle the new state's rules */
            }
        }
    }
    return 0;
}
