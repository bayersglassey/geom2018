

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
#include "write.h"
#include "var_utils.h"



#define TAB_SPACES 4
static void print_tabs(FILE *file, int depth){
    int n = depth * TAB_SPACES;
    for(int i = 0; i < n; i++)putc(' ', file);
}



/************
 * KEY INFO *
 ************/

void keyinfo_reset(keyinfo_t *info){
    for(int i = 0; i < KEYINFO_KEYS; i++){
        info->isdown[i] = false;
        info->wasdown[i] = false;
        info->wentdown[i] = false;
    }
}

void keyinfo_copy(keyinfo_t *info1, keyinfo_t *info2){
    for(int i = 0; i < KEYINFO_KEYS; i++){
        info1->isdown[i] = info2->isdown[i];
        info1->wasdown[i] = info2->wasdown[i];
        info1->wentdown[i] = info2->wentdown[i];
    }
}

int fus_lexer_get_keyinfo(fus_lexer_t *lexer,
    keyinfo_t *info
){
    /* Load keyinfo from file, e.g. the files written when you touch
    a save point */
    int err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;

        char key_c;
        char *name;
        err = fus_lexer_get_name(lexer, &name);
        if(err)return err;
        if(strlen(name) != 1 || !strchr(ANIM_KEY_CS, name[0])){
            return fus_lexer_unexpected(lexer,
                "one of the characters: " ANIM_KEY_CS);}
        key_c = name[0];
        free(name);

        int key_i = body_get_key_i(NULL, key_c);

        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            bool *keystate;
            if(fus_lexer_got(lexer, "is")){
                keystate = info->isdown;
            }else if(fus_lexer_got(lexer, "was")){
                keystate = info->wasdown;
            }else if(fus_lexer_got(lexer, "went")){
                keystate = info->wentdown;
            }else{
                return fus_lexer_unexpected(lexer,
                    "is or was or went");
            }
            err = fus_lexer_next(lexer);
            if(err)return err;

            keystate[key_i] = true;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;
    return 0;
}



/*********************
 * BODY INIT/CLEANUP *
 *********************/

void body_cleanup(body_t *body){
    vars_cleanup(&body->vars);
    stateset_cleanup(&body->stateset);
    recording_cleanup(&body->recording);
}

int body_init(body_t *body, hexgame_t *game, hexmap_t *map,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper
){
    int err;

    body->game = game;
    body->palmapper = palmapper;

    body->confused = false;

    body->out_of_bounds = false;
    body->map = map;
    body->cur_submap = NULL;

    vars_init(&body->vars);

    body->state = NULL;
    body->frame_i = 0;
    body->cooldown = 0;
    body->dead = BODY_NOT_DEAD;

    memset(&body->stateset, 0, sizeof(body->stateset));

    if(stateset_filename != NULL){
        err = body_set_stateset(body, stateset_filename, state_name);
        if(err)return err;
    }else{
        /* We really really expect you to call body_set_stateset
        right away!
        Why didn't caller provide a stateset_filename?..
        Maybe because they're about to call recording_load, or actor_init,
        etc.
        WARNING: if this body is for an actor, but actor doesn't execute the
        "play" effect right away, then this body will remain in this strange
        state with no stateset loaded!!
        ...I do believe we want to fix this. */
    }

    return 0;
}

int body_get_index(body_t *body){
    hexmap_t *map = body->map;
    for(int i = 0; i < map->bodies_len; i++){
        body_t *_body = map->bodies[i];
        if(body == _body)return i;
    }
    return -1;
}

void hexgame_body_dump(body_t *body, int depth){
    print_tabs(stderr, depth);
    fprintf(stderr, "index: %i\n", body_get_index(body));
    print_tabs(stderr, depth);
    fprintf(stderr, "map: %s\n", body->map->name);
    print_tabs(stderr, depth);
    fprintf(stderr, "submap: %s\n",
        body->cur_submap->filename);
    print_tabs(stderr, depth);
    fprintf(stderr, "map vars:\n");
    vars_write(&body->map->vars, stderr, TAB_SPACES * (depth + 1));
    print_tabs(stderr, depth);
    fprintf(stderr, "stateset: %s\n",
        body->stateset.filename);
    print_tabs(stderr, depth);
    fprintf(stderr, "state: %s\n",
        body->state->name);
    print_tabs(stderr, depth);
    fprintf(stderr, "pos: (%i %i)\n",
        body->loc.pos[0], body->loc.pos[1]);
    print_tabs(stderr, depth);
    fprintf(stderr, "rot: %i\n", body->loc.rot);
    print_tabs(stderr, depth);
    fprintf(stderr, "turn: %s\n", body->loc.turn? "yes": "no");
    print_tabs(stderr, depth);
    fprintf(stderr, "body vars:\n");
    vars_write(&body->vars, stderr, TAB_SPACES * (depth + 1));
}

int body_respawn(body_t *body, vec_t pos, rot_t rot, bool turn,
    hexmap_t *map
){
    /* Respawns body at given map and location.
    Only moves body, does NOT change stateset or state.
    Caller is free to use body_set_stateset/body_set_state to achieve
    that. */

    int err;

    hexgame_t *game = body->game;
    vecspace_t *space = game->space;

    /* Set body pos, rot, turn */
    vec_cpy(space->dims, body->loc.pos, pos);
    body->loc.rot = rot;
    body->loc.turn = turn;

    /* Set body state */
    err = body_set_state(body, body->stateset.default_state_name, true);
    if(err)return err;

    /* Set body map */
    hexmap_t *old_map = body->map;
    err = body_move_to_map(body, map);
    if(err)return err;
    if(old_map != map)body_reset_cameras(body);

    /* Reset body keyinfo */
    keyinfo_reset(&body->keyinfo);

    return 0;
}

int body_add_body(body_t *body, body_t **new_body_ptr,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper,
    vec_t addpos, rot_t addrot, bool turn
){
    /* Adds a new body at same location as another body
    (used for e.g. emitting projectiles) */
    int err;
    ARRAY_PUSH_NEW(body_t*, body->map->bodies, new_body)
    err = body_init(new_body, body->game, body->map,
        stateset_filename, state_name, palmapper);
    if(err)return err;
    vecspace_t *space = body->map->space;

    rot_t rot = hexgame_location_get_rot(&body->loc);

    vec_t addpos_cpy;
    vec_cpy(space->dims, addpos_cpy, addpos);
    space->vec_flip(addpos_cpy, body->loc.turn);
    space->vec_rot(addpos_cpy, rot);

    vec_cpy(space->dims, new_body->loc.pos, body->loc.pos);
    vec_add(space->dims, new_body->loc.pos, addpos_cpy);

    new_body->loc.rot =
        rot_rot(space->rot_max, body->loc.rot, addrot);

    new_body->loc.turn = turn? !body->loc.turn: body->loc.turn;

    if(new_body_ptr)*new_body_ptr = new_body;
    return 0;
}



/*************
 * BODY MISC *
 *************/

player_t *body_get_player(body_t *body){
    hexgame_t *game = body->game;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->body == body)return player;
    }
    return NULL;
}

void body_flash_cameras(body_t *body, Uint8 r, Uint8 g, Uint8 b,
    int percent
){
    /* Flash all cameras targeting given body */
    FOREACH_BODY_CAMERA(body, camera, {
        camera_colors_flash(camera, r, g, b, percent);
    })
}

void body_reset_cameras(body_t *body){
    /* Reset all cameras targeting given body */
    FOREACH_BODY_CAMERA(body, camera, {
        camera->should_reset = true;
    })
}

int body_remove(body_t *body){
    /* WARNING: this function modifies map->bodies for body->map.
    So if caller is trying to loop over map->bodies in the usual way,
    the behaviour of that loop is probably gonna be super wrong. */
    int err;
    hexgame_t *game = body->game;
    hexmap_t *map = body->map;

    ARRAY_UNHOOK(map->bodies, body)

    /* Update any cameras following this body */
    FOREACH_BODY_CAMERA(body, camera, {
        camera->body = NULL;
    })
    return 0;
}

int body_move_to_map(body_t *body, hexmap_t *map){
    /* WARNING: this function modifies map->bodies for two maps.
    So if caller is trying to loop over map->bodies in the usual way,
    the behaviour of that loop is probably gonna be super wrong. */
    int err;
    hexgame_t *game = body->game;
    hexmap_t *old_map = body->map;

    /* Don't do nuthin rash if you don't gotta */
    if(body->map == map)return 0;

    /* Do the thing we all came here for */
    body->map = map;

    /* Move body from old to new map's array of bodies */
    ARRAY_UNHOOK(old_map->bodies, body)
    ARRAY_PUSH(body_t*, map->bodies, body)

    /* Update body->cur_submap */
    body_update_cur_submap(body);

    /* Update any cameras following this body */
    FOREACH_BODY_CAMERA(body, camera, {
        camera->map = map;
        camera->cur_submap = body->cur_submap;
    })
    return 0;
}


/**************
 * BODY STATE *
 **************/

int body_set_stateset(body_t *body, const char *stateset_filename,
    const char *state_name
){
    /* Sets body's stateset and state.
    If stateset_filename is NULL, uses current stateset.
    If state_name is NULL, uses stateset's default state. */

    int err;

    if(stateset_filename == NULL)stateset_filename = body->stateset.filename;
    if(stateset_filename == NULL){
        fprintf(stderr, "body_set_stateset: stateset_filename is NULL\n");
        return 2;
    }

    /* Is new stateset different from old one?..
    Note we check for body->stateset.filename == NULL, the weird case
    where body doesn't have a stateset set up yet (and therefore
    body->stateset.filename == NULL). */
    bool stateset_is_different =
        body->stateset.filename != stateset_filename && (
            body->stateset.filename == NULL ||
            strcmp(body->stateset.filename, stateset_filename)
        );

    /* If new stateset is different from old one, make the change */
    if(stateset_is_different){

        /* Don't attempt to cleanup stateset if it's not initialized
        (that weird possibility we want to remove) */
        if(body->stateset.filename != NULL){
            stateset_cleanup(&body->stateset);
        }

        /* Make sure freed pointers are zeroed, etc */
        memset(&body->stateset, 0, sizeof(body->stateset));

        hexmap_t *map = body->map;

        char *stateset_filename_dup = strdup(stateset_filename);
        if(!stateset_filename_dup)return 1;
        err = stateset_load(&body->stateset, stateset_filename_dup,
            NULL, map->prend, map->space);
        if(err)return err;
    }

    if(state_name == NULL){
        /* If state_name not provided, use stateset's default state */
        state_name = body->stateset.default_state_name;
    }

    return body_set_state(body, state_name, true);
}

int body_set_state(body_t *body, const char *state_name,
    bool reset_cooldown
){
    if(state_name == NULL){
        fprintf(stderr, "body_set_state: state_name is NULL\n");
        return 2;
    }

    body->state = stateset_get_state(&body->stateset, state_name);
    if(body->state == NULL){
        fprintf(stderr, "Couldn't init body stateset: "
            "couldn't find state %s in stateset %s\n",
            state_name, body->stateset.filename);
        return 2;
    }

    body->frame_i = 0;
    if(reset_cooldown)body->cooldown = 0;
    body->dead = BODY_NOT_DEAD;
    return 0;
}


/**************
 * BODY INPUT *
 **************/

void body_keydown(body_t *body, int key_i){
    if(key_i < 0 || key_i >= KEYINFO_KEYS)return;
    body->keyinfo.isdown[key_i] = true;
    body->keyinfo.wasdown[key_i] = true;
    body->keyinfo.wentdown[key_i] = true;

    if(body->recording.action == 2){
        body_record_keydown(body, key_i);
    }
}

void body_keyup(body_t *body, int key_i){
    if(key_i < 0 || key_i >= KEYINFO_KEYS)return;
    body->keyinfo.isdown[key_i] = false;

    if(body->recording.action == 2){
        body_record_keyup(body, key_i);
    }
}

int body_get_key_i(body_t *body, char c){
    bool turn = false;
    if(body){
        turn = body->confused? !body->loc.turn: body->loc.turn;
    }
    int key_i =
        c == 'x'? KEYINFO_KEY_ACTION1:
        c == 'y'? KEYINFO_KEY_ACTION2:
        c == 'u'? KEYINFO_KEY_U:
        c == 'd'? KEYINFO_KEY_D:
        c == 'l'? KEYINFO_KEY_L:
        c == 'r'? KEYINFO_KEY_R:
        c == 'f'? (turn? KEYINFO_KEY_L: KEYINFO_KEY_R):
        c == 'b'? (turn? KEYINFO_KEY_R: KEYINFO_KEY_L):
        -1;
    return key_i;
}

char body_get_key_c(body_t *body, int key_i, bool absolute){
    bool turn = false;
    if(body){
        turn = body->confused? !body->loc.turn: body->loc.turn;
    }
    return
        key_i == KEYINFO_KEY_ACTION1? 'x':
        key_i == KEYINFO_KEY_ACTION2? 'y':
        key_i == KEYINFO_KEY_U? 'u':
        key_i == KEYINFO_KEY_D? 'd':
        key_i == KEYINFO_KEY_L? (absolute? 'l': turn? 'f': 'b'):
        key_i == KEYINFO_KEY_R? (absolute? 'r': turn? 'b': 'f'):
        ' ';
}


/*************
 * BODY STEP *
 *************/

void body_update_cur_submap(body_t *body){
    /* Sets body->cur_submap, body->out_of_bounds by colliding body
    against body->map */

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    hexmap_submap_t *new_submap = NULL;

    /* Check if body's pos is touching a vert of any submap */
    bool out_of_bounds = true;
    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        if(!hexmap_submap_is_solid(submap))continue;

        hexcollmap_t *collmap = &submap->collmap;

        /* A HACK! */
        trf_t index = {0};
        hexspace_set(index.add,
             body->loc.pos[0] - submap->pos[0],
            -body->loc.pos[1] + submap->pos[1]);

        hexcollmap_elem_t *vert =
            hexcollmap_get_vert(collmap, &index);
        if(vert != NULL)out_of_bounds = false;
        if(hexcollmap_elem_is_solid(vert)){
            new_submap = submap;
            break;
        }
    }
    body->out_of_bounds = out_of_bounds;

    if(new_submap == NULL){
        /* Check if body's hitbox is touching the water face of
        any submap */

        hexcollmap_t *hitbox = body->state? body->state->hitbox: NULL;
        if(hitbox != NULL){
            trf_t hitbox_trf;
            hexgame_location_init_trf(&body->loc, &hitbox_trf);

            hexmap_collision_t collision;
            hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);

            hexmap_submap_t *water_submap = collision.water.submap;
            if(water_submap)new_submap = water_submap;
        }
    }

    if(new_submap != NULL){
        body->cur_submap = new_submap;

        /* Player-controlled bodies help uncover the minimap */
        player_t *player = body_get_player(body);
        if(player)new_submap->visited = true;
    }
}

int body_handle_rules(body_t *body){
    int err;
    handle: {
        state_effect_goto_t *gotto = NULL;
        err = state_handle_rules(body->state, body->game, body, NULL,
            &gotto);
        if(err)return err;
        if(gotto != NULL){
            err = body_set_state(body, gotto->name, false);
            if(err)return err;

            if(gotto->immediate)goto handle;
                /* If there was an "immediate goto" effect,
                then we immediately handle the new state's rules */
        }
    }
    return 0;
}

int body_step(body_t *body, hexgame_t *game){
    int err;

    if(body->state == NULL){
        return 0;}

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    /* Handle recording & playback */
    err = recording_step(&body->recording);
    if(err)return err;

    /* Increment frame */
    body->frame_i++;
    if(body->frame_i == MAX_FRAME_I)body->frame_i = 0;

    /* Handle animation & input */
    if(body->cooldown > 0){
        body->cooldown--;
    }else{
        /* Handle current state's rules */
        err = body_handle_rules(body);
        if(err)return err;

        /* Start of new frame, no keys have gone down yet.
        Note this only happens after cooldown is finished, which allows
        for simple "buffering" of keypresses. */
        for(int i = 0; i < KEYINFO_KEYS; i++){
            body->keyinfo.wasdown[i] = body->keyinfo.isdown[i];
            body->keyinfo.wentdown[i] = false;}
    }

    /* Figure out current submap */
    body_update_cur_submap(body);

    return 0;
}

bool body_sends_collmsg(body_t *body, const char *msg){
    /* Returns whether body is sending given msg */
    state_t *state = body->state;
    for(int i = 0; i < state->collmsgs_len; i++){
        const char *state_msg = state->collmsgs[i];
        if(!strcmp(state_msg, msg))return true;
    }
    stateset_t *stateset = &body->stateset;
    for(int i = 0; i < stateset->collmsgs_len; i++){
        const char *stateset_msg = stateset->collmsgs[i];
        if(!strcmp(stateset_msg, msg))return true;
    }
    return false;
}

collmsg_handler_t *body_get_collmsg_handler(body_t *body, const char *msg){
    /* Returns the handler body uses to handle given msg (or NULL if not
    found) */
    state_t *state = body->state;
    for(int j = 0; j < state->collmsg_handlers_len; j++){
        collmsg_handler_t *handler = &state->collmsg_handlers[j];
        if(body->stateset.debug_collision){
            fprintf(stderr, "    -> state handler: %s\n", handler->msg);
        }
        if(!strcmp(msg, handler->msg)){
            return handler;
        }
    }
    stateset_t *stateset = &body->stateset;
    for(int j = 0; j < stateset->collmsg_handlers_len; j++){
        collmsg_handler_t *handler = &stateset->collmsg_handlers[j];
        if(body->stateset.debug_collision){
            fprintf(stderr, "    -> stateset handler: %s\n", handler->msg);
        }
        if(!strcmp(msg, handler->msg)){
            return handler;
        }
    }
    return NULL;
}

collmsg_handler_t *_body_handle_other_bodies_collmsgs(body_t *body, body_t *body_other){
    /* Checks all msgs being sent by body_other, and the handlers body uses
    to handle them, and returns the first handler found */
    collmsg_handler_t *handler = NULL;
    state_t *state = body_other->state;
    for(int i = 0; i < state->collmsgs_len; i++){
        const char *msg = state->collmsgs[i];
        if(body->stateset.debug_collision){
            fprintf(stderr, "  -> state collmsg: %s\n", msg);
        }
        handler = body_get_collmsg_handler(body, msg);
        if(handler)return handler;
    }
    stateset_t *stateset = &body_other->stateset;
    for(int i = 0; i < stateset->collmsgs_len; i++){
        const char *msg = stateset->collmsgs[i];
        if(body->stateset.debug_collision){
            fprintf(stderr, "  -> stateset collmsg: %s\n", msg);
        }
        handler = body_get_collmsg_handler(body, msg);
        if(handler)return handler;
    }
    return NULL;
}

int body_collide_against_body(body_t *body, body_t *body_other){
    /* Do whatever happens when two bodies collide */
    int err;

    if(body->stateset.debug_collision){
        fprintf(stderr, "Colliding bodies: %s (%s) against %s (%s)\n",
            body->stateset.filename, body->state->name,
            body_other->stateset.filename, body_other->state->name);
    }

    if(body->recording.action == 1 && !body->recording.reacts){
        /* Bodies playing a recording don't react to collisions.
        In particular, they cannot be "killed" by other bodies.
        MAYBE TODO: These bodies should die too, but then their
        recording should restart after a brief pause?
        Maybe we can reuse body->cooldown for the pause. */
        if(body->stateset.debug_collision){
            fprintf(stderr, "  -> recording is playing, early exit\n");
        }
        return 0;
    }

    /* Find first (if any) collmsg of body_other which is handled by body */
    collmsg_handler_t *handler = _body_handle_other_bodies_collmsgs(
        body, body_other);
    if(!handler)return 0;

    if(body->stateset.debug_collision){
        fprintf(stderr, "  -> *** handling with:\n");
        for(int i = 0; i < handler->effects_len; i++){
            state_effect_t *effect = handler->effects[i];
            int depth = 2; // indentation: depth * "  "
            state_effect_dump(effect, stderr, depth);
        }
    }

    /* Body "handles" the collmsg by applying handler's effects to itself */
    {
        bool continues = false;
            /* Unused. The "continue" effect only makes sense when handling
            rules: it means "continue checking for matching rules after this
            one".
            But here, we are only applying a series of effects. */

        err = collmsg_handler_apply(handler, body->game, body, NULL,
            &continues);
        if(err)return err;
    }

    return 0;
}


/***************
 * BODY RENDER *
 ***************/

int body_render(body_t *body,
    SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    hexmap_t *map, vec_t camera_renderpos, prismelmapper_t *mapper
){
    /* RENDER THAT BODY */
    int err;

    if(body->state == NULL)return 0;

    rendergraph_t *rgraph = body->state->rgraph;
    if(rgraph == NULL)return 0;

    prismelrenderer_t *prend = map->prend;
    vecspace_t *space = map->space;

    if(body->palmapper){
        err = palettemapper_apply_to_rendergraph(body->palmapper,
            prend, rgraph, NULL, space, &rgraph);
        if(err)return err;
    }

    vec_t pos;
    vec4_vec_from_hexspace(pos, body->loc.pos);
    vec_sub(rgraph->space->dims, pos, camera_renderpos);
    vec_mul(rgraph->space, pos, map->unit);

    rot_t body_rot = hexgame_location_get_rot(&body->loc);
    rot_t rot = vec4_rot_from_hexspace(body_rot);
    flip_t flip = body->loc.turn;
    int frame_i = body->frame_i;

    err = rendergraph_render(rgraph, surface,
        pal, prend,
        x0, y0, zoom,
        pos, rot, flip, frame_i, mapper);
    if(err)return err;

    return 0;
}

