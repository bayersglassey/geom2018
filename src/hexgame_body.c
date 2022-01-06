

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
#include "write.h"
#include "var_utils.h"
#include "hexgame_vars_props.h"



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
        err = fus_lexer_get_chr(lexer, &key_c);
        if(err)return err;
        if(!strchr(ANIM_KEY_CS, key_c)){
            return fus_lexer_unexpected(lexer,
                "one of the characters: " ANIM_KEY_CS);}

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

static void body_label_mapping_cleanup(body_label_mapping_t *mapping){
    /* Nothing to do... */
}

void body_cleanup(body_t *body){
    valexpr_cleanup(&body->visible_expr);
    vars_cleanup(&body->vars);
    stateset_cleanup(&body->stateset);
    recording_cleanup(&body->recording);
    ARRAY_FREE_PTR(body_label_mapping_t*, body->label_mappings,
        body_label_mapping_cleanup)
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

    valexpr_set_literal_bool(&body->visible_expr, true);
    body->visible_not = false;

    vars_init_with_props(&body->vars, hexgame_vars_prop_names);

    ARRAY_INIT(body->label_mappings)

    body->recording.action = 0; /* No recording loaded */

    body->state = NULL;
    body->frame_i = 0;
    body->cooldown = 0;
    body->no_key_reset = false;
    body->dead = BODY_NOT_DEAD;
    body->safe = false;
    body->remove = false;

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

bool body_is_done_for(body_t *body){
    return body->dead || (body->out_of_bounds && !body->state->flying);
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
    fprintf(stderr, "map: %s\n", body->map->filename);
    print_tabs(stderr, depth);
    fprintf(stderr, "submap: %s\n",
        body->cur_submap->filename);
    print_tabs(stderr, depth);
    if(body->cur_submap->group)fprintf(stderr, "submap group: %s\n",
        body->cur_submap->group->name);
    print_tabs(stderr, depth);
    fprintf(stderr, "global vars:\n");
    vars_write(&body->game->vars, stderr, TAB_SPACES * (depth + 1));
    print_tabs(stderr, depth);
    fprintf(stderr, "map vars:\n");
    vars_write(&body->map->vars, stderr, TAB_SPACES * (depth + 1));
    print_tabs(stderr, depth);
    fprintf(stderr, "stateset: %s\n",
        body->stateset.filename);
    print_tabs(stderr, depth);
    fprintf(stderr, "state: %s\n",
        body->state->name);
    hexgame_location_write(&body->loc, stderr, TAB_SPACES * depth);
    print_tabs(stderr, depth);
    fprintf(stderr, "body vars:\n");
    vars_write(&body->vars, stderr, TAB_SPACES * (depth + 1));
    print_tabs(stderr, depth);
    fprintf(stderr, "label mappings:\n");
    for(int i = 0; i < body->label_mappings_len; i++){
        body_label_mapping_t *mapping = body->label_mappings[i];
        print_tabs(stderr, depth);
        fprintf(stderr, "    %s -> %s (frame=%i)\n", mapping->label_name,
            mapping->rgraph->name, mapping->frame_i);
    }
}

int body_is_visible(body_t *body, bool *visible_ptr){
    int err;

    /* NOTE: visible by default. */
    bool visible = true;

    val_t *result;
    valexpr_context_t context = {
        .myvars = &body->vars,
        .mapvars = &body->map->vars,
        .globalvars = &body->game->vars
    };
    err = valexpr_get(&body->visible_expr, &context, &result);
    if(err){
        fprintf(stderr,
            "Error while evaluating visibility for body:\n");
        hexgame_body_dump(body, 0);
        return err;
    }else if(!result){
        /* Val not found: use default visible value */
    }else{
        visible = val_get_bool(result);
    }

    if(body->visible_not)visible = !visible;
    *visible_ptr = visible;
    return 0;
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

static body_label_mapping_t *_body_get_label_mapping(body_t *body,
    const char *label_name
){
    for(int i = 0; i < body->label_mappings_len; i++){
        body_label_mapping_t *mapping = body->label_mappings[i];
        if(
            mapping->label_name == label_name ||
            !strcmp(mapping->label_name, label_name)
        )return mapping;
    }
    return NULL;
}

static int body_get_label_mapping(body_t *body, const char *label_name,
    body_label_mapping_t **mapping_ptr
){
    int err;

    body_label_mapping_t *found_mapping = _body_get_label_mapping(body,
        label_name);
    if(found_mapping){
        *mapping_ptr = found_mapping;
        return 0;
    }

    ARRAY_PUSH_NEW(body_label_mapping_t*, body->label_mappings, mapping)
    mapping->label_name = label_name;
    mapping->rgraph = NULL;
    *mapping_ptr = mapping;
    return 0;
}

int body_unset_label_mapping(body_t *body, const char *label_name){
    int err;
    body_label_mapping_t *found_mapping = _body_get_label_mapping(body,
        label_name);
    if(found_mapping){
        ARRAY_REMOVE_PTR(body->label_mappings, found_mapping,
            body_label_mapping_cleanup)
    }
    return 0;
}

int body_set_label_mapping(body_t *body, const char *label_name,
    rendergraph_t *rgraph
){
    int err;

    body_label_mapping_t *mapping;
    err = body_get_label_mapping(body, label_name, &mapping);
    if(err)return err;

    mapping->rgraph = rgraph;
    return 0;
}

player_t *body_get_player(body_t *body){
    hexgame_t *game = body->game;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->body == body)return player;
    }
    return NULL;
}

actor_t *body_get_actor(body_t *body){
    hexgame_t *game = body->game;
    for(int i = 0; i < game->actors_len; i++){
        actor_t *actor = game->actors[i];
        if(actor->body == body)return actor;
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

void body_remove(body_t *body){
    /* WARNING: this function modifies map->bodies for body->map.
    So if caller is trying to loop over map->bodies in the usual way,
    the behaviour of that loop is probably gonna be super wrong.
    Instead, you can use body->remove = true to mark the body for removal
    (which will result in body_remove being called when it is safe to do
    so). */
    hexgame_t *game = body->game;
    hexmap_t *map = body->map;

    /* Update any cameras following this body */
    FOREACH_BODY_CAMERA(body, camera, {
        camera->body = NULL;
    })

    /* If body has an actor, unhook it from body */
    actor_t *actor = body_get_actor(body);
    if(actor)actor->body = NULL;

    ARRAY_REMOVE_PTR(map->bodies, body, body_cleanup)
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
    err = body_update_cur_submap(body);
    if(err)return err;

    /* Update any cameras following this body */
    FOREACH_BODY_CAMERA(body, camera, {
        camera->map = map;
        camera->cur_submap = body->cur_submap;
    })
    return 0;
}

int body_refresh_vars(body_t *body){
    int err;
    vars_t *vars = &body->vars;

    err = vars_set_bool(vars, ".turn", body->loc.turn);
    if(err)return err;

    return 0;
}

int body_relocate(body_t *body, const char *map_filename,
    hexgame_location_t *loc, const char *stateset_filename,
    const char *state_name
){
    int err;

    hexgame_t *game = body->game;

    hexmap_t *new_map = body->map;
    if(map_filename != NULL){
        /* Switch map */
        err = hexgame_get_or_load_map(game,
            map_filename, &new_map);
        if(err)return err;
    }

    if(loc == NULL)loc = &body->loc;

    /* Respawn body */
    err = body_respawn(body,
        loc->pos, loc->rot, loc->turn, new_map);
    if(err)return err;

    /* Colour to flash screen (default: cyan) */
    int flash_r = 0;
    int flash_g = 255;
    int flash_b = 255;

    if(stateset_filename != NULL){
        if(strcmp(body->stateset.filename, stateset_filename)){
            /* Switch anim (stateset) */
            err = body_set_stateset(body, stateset_filename, state_name);
            if(err)return err;

            /* Pink flash indicates your body was changed, not just teleported */
            flash_r = 255;
            flash_g = 200;
            flash_b = 200;
        }
    }

    /* Flash screen so player knows something happened */
    body_flash_cameras(body, flash_r, flash_g, flash_b, 60);
    body_reset_cameras(body);
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

        err = stateset_load(&body->stateset, stateset_filename,
            NULL, map->prend, map->space);
        if(err)return err;
    }

    /* Copy stateset's vars onto body */
    err = vars_copy(&body->vars, &body->stateset.vars);
    if(err)return err;

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

int body_update_cur_submap(body_t *body){
    /* Sets body->cur_submap, body->out_of_bounds by colliding body
    against body->map */
    int err;

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    hexmap_submap_t *new_submap = NULL;

    /* Check if body's pos is touching a vert of any submap */
    bool out_of_bounds = true;
    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        bool solid;
        err = hexmap_submap_is_solid(submap, &solid);
        if(err)return err;
        if(!solid)continue;

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
            err = hexmap_collide_special(map, hitbox, &hitbox_trf,
                &collision);
            if(err)return err;

            hexmap_submap_t *water_submap = collision.water.submap;
            if(water_submap)new_submap = water_submap;
        }
    }

    if(new_submap != NULL){
        body->cur_submap = new_submap;

        /* Player-controlled bodies help uncover the minimap */
        player_t *player = body_get_player(body);
        if(player)hexmap_submap_visit(new_submap);
    }

    return 0;
}

int body_handle_rules(body_t *body, body_t *your_body){
    int err;
    handle: {
        hexgame_state_context_t context = {
            .game = body->game,
            .body = body,
            .your_body = your_body,
        };
        state_effect_goto_t *gotto = NULL;
        err = state_handle_rules(body->state, &context, &gotto);
        if(err)return err;
        if(gotto != NULL){
            err = state_effect_goto_apply_to_body(gotto, body);
            if(err)return err;

            if(gotto->immediate)goto handle;
                /* If there was an "immediate goto" effect,
                then we immediately handle the new state's rules */
        }
    }
    return 0;
}

static int _increment_frame_i(int frame_i) {
    if(frame_i == MAX_FRAME_I - 1)return 0;
    return frame_i + 1;
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
    body->frame_i = _increment_frame_i(body->frame_i);
    for(int i = 0; i < body->label_mappings_len; i++){
        body_label_mapping_t *mapping = body->label_mappings[i];
        mapping->frame_i = _increment_frame_i(mapping->frame_i);
    }

    /* Handle animation & input */
    if(body->cooldown > 0){
        body->cooldown--;
    }else{
        /* Handle current state's rules */
        err = body_handle_rules(body, NULL);
        if(err)return err;

        if(body->no_key_reset) {
            /* States can choose to prevent keys from being reset at the
            end of cooldown, so that "buffering" of keypresses lasts through
            those states to the ones they transition to */
            body->no_key_reset = false;
        }else{
            /* Start of new frame, no keys have gone down yet.
            Note this only happens after cooldown is finished, which allows
            for simple "buffering" of keypresses. */
            for(int i = 0; i < KEYINFO_KEYS; i++){
                body->keyinfo.wasdown[i] = body->keyinfo.isdown[i];
                body->keyinfo.wentdown[i] = false;
            }
        }
    }

    /* Figure out current submap */
    err = body_update_cur_submap(body);
    if(err)return err;

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

        hexgame_state_context_t context = {
            .game = body->game,
            .body = body,
            .your_body = body_other,
        };
        err = collmsg_handler_apply(handler, &context, &continues);
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
    vecspace_t *map_space = map->space; /* &hexspace */
    vecspace_t *rgraph_space = rgraph->space; /* &vec4 */

    if(body->palmapper){
        err = palettemapper_apply_to_rendergraph(body->palmapper,
            prend, rgraph, NULL, map_space, &rgraph);
        if(err)return err;
    }

    int frame_i = body->frame_i;

    vec_t rendered_pos;
    rot_t rendered_rot;
    flip_t rendered_flip;
    vec4_coords_from_hexspace(
        body->loc.pos,
        hexgame_location_get_rot(&body->loc),
        body->loc.turn,
        rendered_pos, &rendered_rot, &rendered_flip);
    vec_sub(rgraph_space->dims, rendered_pos, camera_renderpos);
    vec_mul(rgraph_space, rendered_pos, map->unit);

    /* Render body's rgraph */
    err = rendergraph_render(rgraph, surface,
        pal, prend,
        x0, y0, zoom,
        rendered_pos, rendered_rot, rendered_flip,
        frame_i, mapper);
    if(err)return err;

    if(body->label_mappings_len){

        /* Make sure body->labels is populated */
        err = rendergraph_calculate_labels(rgraph);
        if(err)return err;

        trf_t rendered_trf = {
            .rot = rendered_rot,
            .flip = rendered_flip,
        };
        vec_cpy(rgraph_space->dims, rendered_trf.add, rendered_pos);

        int animated_frame_i = get_animated_frame_i(
            rgraph->animation_type, rgraph->n_frames, frame_i);
        rendergraph_frame_t *frame = &rgraph->frames[animated_frame_i];

        /* Render all mapped labels */
        for(int i = 0; i < body->label_mappings_len; i++){
            body_label_mapping_t *mapping = body->label_mappings[i];
            rendergraph_t *label_rgraph = mapping->rgraph;
            for(int j = 0; j < frame->labels_len; j++){
                rendergraph_label_t *label = frame->labels[j];
                if(!(
                    label->name == mapping->label_name ||
                    !strcmp(label->name, mapping->label_name)
                ))continue;

                trf_t label_trf = label->trf;
                trf_apply(rgraph_space, &label_trf, &rendered_trf);

                int label_frame_i = get_animated_frame_i(
                    label_rgraph->animation_type,
                    label_rgraph->n_frames, mapping->frame_i);

                /* Render label's rgraph */
                err = rendergraph_render(label_rgraph, surface,
                    pal, prend,
                    x0, y0, zoom,
                    label_trf.add, label_trf.rot, label_trf.flip,
                    label_frame_i, mapper);
                if(err)return err;
            }
        }
    }

    return 0;
}

