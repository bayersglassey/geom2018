

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

void body_cleanup(body_t *body){
    valexpr_cleanup(&body->visible_expr);
    valexpr_cleanup(&body->target_expr);
    vars_cleanup(&body->vars);
    recording_cleanup(&body->recording);
    ARRAY_FREE_PTR(label_mapping_t*, body->label_mappings,
        label_mapping_cleanup)
}

static vars_callback_t body_vars_callback;
static int body_vars_callback(vars_t *vars, var_t *var){
    int err;
    body_t *body = (body_t*)vars->callback_data;
    if(var->props & (1 << HEXGAME_VARS_PROP_LABEL)){
        const char *label_name = var->key;
        const char *rgraph_name = val_get_str(&var->value);
        if(!rgraph_name){
            err = body_unset_label_mapping(body, label_name);
            if(err)return err;
        }else{
            rendergraph_t *rgraph = prismelrenderer_get_rgraph(
                body->game->prend, rgraph_name);
            if(!rgraph){
                fprintf(stderr, "Couldn't find rgraph: %s\n", rgraph_name);
                fprintf(stderr, "...while updating label var: %s\n", var->key);
                fprintf(stderr, "...dumping body:\n");
                body_dump(body, 1);
                return 2;
            }
            err = body_set_label_mapping(body, label_name, rgraph);
            if(err)return err;
        }
    }
    return 0;
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
    body->touching_mappoint = false;
    body->map = map;
    body->cur_submap = NULL;

    valexpr_set_literal_bool(&body->visible_expr, true);
    valexpr_set_literal_bool(&body->target_expr, false);

    vars_init_with_props(&body->vars, hexgame_vars_prop_names);
    body->vars.callback = &body_vars_callback;
    body->vars.callback_data = (void*)body;

    ARRAY_INIT(body->label_mappings)

    /* No recording loaded */
    recording_init(&body->recording);

    body->state = NULL;
    body->frame_i = 0;
    body->cooldown = 0;
    body->no_key_reset = false;
    body->dead = BODY_NOT_DEAD;
    body->remove = false;
    body->just_spawned = false;

    body->stateset = NULL;
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

void body_dump(body_t *body, int depth){
    print_tabs(stderr, depth);
    fprintf(stderr, "index: %i\n", body_get_index(body));
    print_tabs(stderr, depth);
    fprintf(stderr, "map: %s\n", body->map->filename);
    print_tabs(stderr, depth);

    hexmap_submap_t *submap = body->cur_submap;
    if(submap){
        fprintf(stderr, "submap: %s\n", submap->filename);
        print_tabs(stderr, depth);
        if(submap->group)fprintf(stderr, "submap group: %s\n",
            submap->group->name);
        print_tabs(stderr, depth);
    }

    fprintf(stderr, "global vars:\n");
    vars_write(&body->game->vars, stderr, TAB_SPACES * (depth + 1));
    print_tabs(stderr, depth);
    fprintf(stderr, "map vars:\n");
    vars_write(&body->map->vars, stderr, TAB_SPACES * (depth + 1));
    print_tabs(stderr, depth);
    fprintf(stderr, "stateset: %s\n",
        body->stateset->filename);
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
        label_mapping_t *mapping = body->label_mappings[i];
        print_tabs(stderr, depth);
        fprintf(stderr, "    %s -> %s (frame=%i)\n", mapping->label_name,
            mapping->rgraph->name, mapping->frame_i);
    }
}

int body_is_visible(body_t *body, bool *visible_ptr){
    int err;

    /* NOTE: visible by default. */
    bool visible = true;

    valexpr_result_t result = {0};
    valexpr_context_t context = {
        .myvars = &body->vars,
        .mapvars = &body->map->vars,
        .globalvars = &body->game->vars
    };
    err = valexpr_get(&body->visible_expr, &context, &result);
    if(err){
        fprintf(stderr,
            "Error while evaluating visibility for body:\n");
        body_dump(body, 0);
        return err;
    }else if(!result.val){
        /* Val not found: use default visible value */
    }else{
        visible = val_get_bool(result.val);
    }

    *visible_ptr = visible;
    return 0;
}

int body_is_target(body_t *body, bool *target_ptr){
    int err;

    /* NOTE: not a minimap target by default. */
    bool target = false;

    valexpr_result_t result = {0};
    valexpr_context_t context = {
        .myvars = &body->vars,
        .mapvars = &body->map->vars,
        .globalvars = &body->game->vars
    };
    err = valexpr_get(&body->target_expr, &context, &result);
    if(err){
        fprintf(stderr,
            "Error while evaluating visibility for body:\n");
        body_dump(body, 0);
        return err;
    }else if(!result.val){
        /* Val not found: use default target value */
    }else{
        target = val_get_bool(result.val);
    }

    *target_ptr = target;
    return 0;
}

int body_respawn(body_t *body, vec_t pos, rot_t rot, bool turn,
    hexmap_t *map
){
    /* Respawns body at given map and location.
    Only moves body, does NOT change stateset.
    Caller is free to use body_set_stateset to achieve that. */

    int err;

    hexgame_t *game = body->game;
    vecspace_t *space = game->space;

    /* Set body pos, rot, turn */
    vec_cpy(space->dims, body->loc.pos, pos);
    body->loc.rot = rot;
    body->loc.turn = turn;

    /* Set body state */
    err = body_set_state(body, body->stateset->default_state_name, true);
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

static label_mapping_t *_body_get_label_mapping(body_t *body,
    const char *label_name
){
    for(int i = 0; i < body->label_mappings_len; i++){
        label_mapping_t *mapping = body->label_mappings[i];
        if(
            mapping->label_name == label_name ||
            !strcmp(mapping->label_name, label_name)
        )return mapping;
    }
    return NULL;
}

static int body_get_label_mapping(body_t *body, const char *label_name,
    label_mapping_t **mapping_ptr
){
    int err;

    label_mapping_t *found_mapping = _body_get_label_mapping(body,
        label_name);
    if(found_mapping){
        *mapping_ptr = found_mapping;
        return 0;
    }

    ARRAY_PUSH_NEW(label_mapping_t*, body->label_mappings, mapping)
    mapping->label_name = label_name;
    mapping->rgraph = NULL;
    *mapping_ptr = mapping;
    return 0;
}

int body_unset_label_mapping(body_t *body, const char *label_name){
    int err;
    label_mapping_t *found_mapping = _body_get_label_mapping(body,
        label_name);
    if(found_mapping){
        ARRAY_REMOVE_PTR(body->label_mappings, found_mapping,
            label_mapping_cleanup)
    }
    return 0;
}

int body_set_label_mapping(body_t *body, const char *label_name,
    rendergraph_t *rgraph
){
    int err;

    label_mapping_t *mapping;
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

    /* Run any procs which need to e.g. clean up mapvars in the old map */
    err = body_execute_procs(body, STATESET_PROC_TYPE_ONMAPCHANGE);
    if(err)return err;

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
    })
    return 0;
}

static var_t *_get_nosave_var(vars_t *vars, const char *name){
    var_t *var = vars_get_or_add(vars, name);
    if(var == NULL)return NULL;

    /* Mark var as being nosave */
    var->props |= 1 << HEXGAME_VARS_PROP_NOSAVE;
    return var;
}
int body_refresh_vars(body_t *body){
    int err;
    vars_t *vars = &body->vars;

    {
        var_t *var = _get_nosave_var(vars, ".safe");
        if(var == NULL)return 1;
        val_set_bool(&var->value, body->state? body->state->safe: false);
    }

    {
        var_t *var = _get_nosave_var(vars, ".x");
        if(var == NULL)return 1;
        val_set_int(&var->value, body->loc.pos[0]);
    }

    {
        var_t *var = _get_nosave_var(vars, ".y");
        if(var == NULL)return 1;
        val_set_int(&var->value, body->loc.pos[1]);
    }

    {
        var_t *var = _get_nosave_var(vars, ".rot");
        if(var == NULL)return 1;
        val_set_int(&var->value, body->loc.rot);
    }

    {
        var_t *var = _get_nosave_var(vars, ".turn");
        if(var == NULL)return 1;
        val_set_bool(&var->value, body->loc.turn);
    }

    {
        var_t *var = _get_nosave_var(vars, ".anim");
        if(var == NULL)return 1;
        if(body->stateset != NULL){
            val_set_const_str(&var->value, body->stateset->filename);
        }else{
            val_set_null(&var->value);
        }
    }

    {
        var_t *var = _get_nosave_var(vars, ".state");
        if(var == NULL)return 1;
        if(body->state != NULL){
            val_set_const_str(&var->value, body->state->name);
        }else{
            val_set_null(&var->value);
        }
    }

    {
        var_t *var = _get_nosave_var(vars, ".map");
        if(var == NULL)return 1;
        val_set_const_str(&var->value, body->map->filename);
    }

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

        if(loc == NULL)loc = &new_map->spawn;
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
        if(strcmp(body->stateset->filename, stateset_filename)){
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

    /* Update body's cur_submap */
    err = body_update_cur_submap(body);
    if(err)return err;

    return 0;
}


/**************
 * BODY STATE *
 **************/

int body_execute_procs(body_t *body, int type /* enum stateset_proc_type */){
    /* Execute all procs of the given type */
    int err;

    hexgame_state_context_t context = {
        .game = body->game,
        .body = body,
    };
    state_effect_goto_t *gotto = NULL;
    for(int i = 0; i < body->stateset->contexts_len; i++){
        state_context_t *state_context = body->stateset->contexts[i];
        for(int j = 0; j < state_context->procs_len; j++){
            stateset_proc_t *proc = &state_context->procs[j];
            if(proc->type != type)continue;
            if(hexgame_state_context_debug(&context)){
                fprintf(stderr, "Executing \"%s\" proc: \"%s\"\n",
                    stateset_proc_type_msg(type), proc->name);
            }
            for(int k = 0; k < proc->effects_len; k++){
                state_effect_t *effect = proc->effects[k];
                err = state_effect_apply(effect, &context, &gotto, NULL);
                if(!err && gotto){
                    fprintf(stderr, "Can't use \"goto\" in \"%s\" procs\n",
                        stateset_proc_type_msg(type));
                    err = 2;
                }
                if(err){
                    if(err == 2){
                        fprintf(stderr, "...in proc: \"%s\"\n", proc->name);
                        fprintf(stderr, "...of body:\n");
                        body_dump(body, 1);
                    }
                    return err;
                }
            }
        }
    }
    return 0;
}

int body_set_stateset(body_t *body, const char *stateset_filename,
    const char *state_name
){
    /* Sets body's stateset and state.
    If stateset_filename is NULL, uses current stateset.
    If state_name is NULL, uses stateset's default state. */

    int err;

    if(stateset_filename == NULL && body->stateset != NULL){
        stateset_filename = body->stateset->filename;
    }
    if(stateset_filename == NULL){
        fprintf(stderr, "body_set_stateset: stateset_filename is NULL\n");
        return 2;
    }

    /* Is new stateset different from old one?..
    Note we check for body->stateset == NULL, the weird case
    where body doesn't have a stateset set up yet. */
    bool stateset_is_different = body->stateset == NULL || (
        body->stateset->filename != stateset_filename &&
        strcmp(body->stateset->filename, stateset_filename)
    );

    /* If new stateset is different from old one, make the change */
    if(stateset_is_different){

        /* Get or load stateset */
        err = hexgame_get_or_load_stateset(body->game, stateset_filename,
            &body->stateset);
        if(err)return err;

        /* Copy stateset's vars onto body */
        err = vars_copy(&body->vars, &body->stateset->vars);
        if(err)return err;

        /* Execute any "onstatesetchange" procs */
        err = body_execute_procs(body, STATESET_PROC_TYPE_ONSTATESETCHANGE);
        if(err)return err;
    }

    if(state_name == NULL){
        /* If state_name not provided, use stateset's default state */
        state_name = body->stateset->default_state_name;
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

    body->state = stateset_get_state(body->stateset, state_name);
    if(body->state == NULL){
        fprintf(stderr, "Couldn't init body stateset: "
            "couldn't find state %s in stateset %s\n",
            state_name, body->stateset->filename);
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

    if(body->recording.action == RECORDING_ACTION_RECORD){
        body_record_keydown(body, key_i);
    }
}

void body_keyup(body_t *body, int key_i){
    if(key_i < 0 || key_i >= KEYINFO_KEYS)return;
    body->keyinfo.isdown[key_i] = false;

    if(body->recording.action == RECORDING_ACTION_RECORD){
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
    /* Sets body->cur_submap, body->out_of_bounds, body->touching_mappoint
    by colliding body against body->map */
    int err;

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    hexmap_submap_t *new_submap = NULL;

    /* Check if body's pos is touching a vert of any submap */
    bool out_of_bounds = true;
    /* NOTE: we iterate over submaps in reverse order, to match the order
    used by hexmap_collide_elem */
    for(int i = map->submaps_len - 1; i >= 0; i--){
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

    bool touching_mappoint = false;
    hexcollmap_t *hitbox = body->state? body->state->hitbox: NULL;
    if(hitbox != NULL){
        trf_t hitbox_trf;
        hexgame_location_init_trf(&body->loc, &hitbox_trf);

        hexmap_collision_t collision;
        err = hexmap_collide_special(map, hitbox, &hitbox_trf,
            &collision);
        if(err)return err;

        /* If body isn't touching solid ground, but *is* touching water,
        then body is in the submap which has the water. */
        hexmap_submap_t *water_submap = collision.water.submap;
        if(new_submap == NULL && water_submap)new_submap = water_submap;

        /* Is body touching a mappoint? */
        if(collision.mappoint.submap)touching_mappoint = true;
    }
    body->touching_mappoint = touching_mappoint;

    if(new_submap != NULL){
        body->cur_submap = new_submap;

        /* Player-controlled bodies help uncover the minimap */
        player_t *player = body_get_player(body);
        if(player)hexmap_submap_visit(new_submap);
    }

    /* Update any cameras following this body */
    FOREACH_BODY_CAMERA(body, camera, {
        camera->cur_submap = body->cur_submap;
    })

    return 0;
}

int body_handle_rules(body_t *body, body_t *your_body){
    int err;
    hexgame_state_context_t context = {
        .game = body->game,
        .body = body,
        .your_body = your_body,
    };
    handle: {
        if(hexgame_state_context_debug(&context)){
            fprintf(stderr, "Handling rules for state: \"%s\" -> \"%s\"\n",
                body->state->stateset->filename, body->state->name);
        }
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

    if(body->state == NULL)return 0;

    if(body->just_spawned){
        body->just_spawned = false;
        return 0;}

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    /* Handle recording & playback */
    err = recording_step(&body->recording);
    if(err)return err;

    /* Increment frame */
    body->frame_i = _increment_frame_i(body->frame_i);
    for(int i = 0; i < body->label_mappings_len; i++){
        label_mapping_t *mapping = body->label_mappings[i];
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
    for(
        state_context_t *context = state->context;
        context;
        context = context->parent
    ){
        for(int i = 0; i < context->collmsgs_len; i++){
            const char *state_msg = context->collmsgs[i];
            if(!strcmp(state_msg, msg))return true;
        }
    }
    return false;
}

collmsg_handler_t *body_get_collmsg_handler(body_t *body, const char *msg){
    /* Returns the handler body uses to handle given msg (or NULL if not
    found) */
    state_t *state = body->state;
    for(
        state_context_t *context = state->context;
        context;
        context = context->parent
    ){
        for(int i = 0; i < context->collmsg_handlers_len; i++){
            collmsg_handler_t *handler = &context->collmsg_handlers[i];
            if(body->stateset->debug_collision){
                fprintf(stderr, "    -> state handler: %s\n", handler->msg);
            }
            if(!strcmp(msg, handler->msg)){
                return handler;
            }
        }
    }
    return NULL;
}

collmsg_handler_t *_body_handle_other_bodies_collmsgs(body_t *body, body_t *body_other){
    /* Checks all msgs being sent by body_other, and the handlers body uses
    to handle them, and returns the first handler found */
    collmsg_handler_t *handler = NULL;
    state_t *state = body_other->state;
    for(
        state_context_t *context = state->context;
        context;
        context = context->parent
    ){
        for(int i = 0; i < context->collmsgs_len; i++){
            const char *msg = context->collmsgs[i];
            if(body->stateset->debug_collision){
                fprintf(stderr, "  -> state collmsg: %s\n", msg);
            }
            handler = body_get_collmsg_handler(body, msg);
            if(handler)return handler;
        }
    }
    return NULL;
}

int body_collide_against_body(body_t *body, body_t *body_other){
    /* Do whatever happens when two bodies collide */
    int err;

    if(body->stateset->debug_collision){
        fprintf(stderr, "Colliding bodies: %s (%s) against %s (%s)\n",
            body->stateset->filename, body->state->name,
            body_other->stateset->filename, body_other->state->name);
    }

    if(body->recording.action == RECORDING_ACTION_PLAY && !body->recording.reacts){
        /* Bodies playing a recording don't react to collisions.
        In particular, they cannot be "killed" by other bodies.
        MAYBE TODO: These bodies should die too, but then their
        recording should restart after a brief pause?
        Maybe we can reuse body->cooldown for the pause. */
        if(body->stateset->debug_collision){
            fprintf(stderr, "  -> recording is playing, early exit\n");
        }
        return 0;
    }

    /* Find first (if any) collmsg of body_other which is handled by body */
    collmsg_handler_t *handler = _body_handle_other_bodies_collmsgs(
        body, body_other);
    if(!handler)return 0;

    if(body->stateset->debug_collision){
        fprintf(stderr, "  -> *** handling with:\n");
        for(int i = 0; i < handler->effects_len; i++){
            state_effect_t *effect = handler->effects[i];
            int depth = 2; // indentation: depth * "  "
            state_effect_dump(effect, stderr, depth);
        }
    }

    /* Body "handles" the collmsg by applying handler's effects to itself */
    {
        hexgame_state_context_t context = {
            .game = body->game,
            .body = body,
            .your_body = body_other,
        };
        if(hexgame_state_context_debug(&context)){
            fprintf(stderr, "Executing handler for collmsg: \"%s\"\n",
                handler->msg);
        }
        err = collmsg_handler_apply(handler, &context, NULL);
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
    vec_t camera_renderpos, prismelmapper_t *mapper
){
    /* RENDER THAT BODY */
    int err;

    state_t *state = body->state;
    if(state == NULL)return 0;

    if(state->rgraph){
        err = body_render_rgraph(body, state->rgraph,
            surface, pal, x0, y0, zoom, camera_renderpos, mapper,
            true /* render_labels */);
        if(err)return err;
    }

    for(int i = 0; i < state->extra_rgraphs_len; i++){
        rendergraph_t *rgraph = state->extra_rgraphs[i];
        err = body_render_rgraph(body, rgraph,
            surface, pal, x0, y0, zoom, camera_renderpos, mapper,
            false /* render_labels */);
        if(err)return err;
    }

    return 0;
}

int body_render_rgraph(body_t *body, rendergraph_t *rgraph,
    SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    vec_t camera_renderpos, prismelmapper_t *mapper,
    bool render_labels
){
    int err;

    hexmap_t *map = body->map;
    prismelrenderer_t *prend = map->prend;
    vecspace_t *map_space = map->space; /* &hexspace */
    vecspace_t *rgraph_space = rgraph->space; /* &vec4 */

    hexgame_location_t *loc = &body->loc;
    palettemapper_t *palmapper = body->palmapper;
    int frame_i = body->frame_i;

    vec_t rendered_pos;
    rot_t rendered_rot;
    flip_t rendered_flip;
    vec4_coords_from_hexspace(
        loc->pos,
        hexgame_location_get_rot(loc),
        loc->turn,
        rendered_pos, &rendered_rot, &rendered_flip);
    vec_sub(rgraph_space->dims, rendered_pos, camera_renderpos);
    vec_mul(rgraph_space, rendered_pos, map->unit);

    if(render_labels){
        /* Render rgraph and labels (recursively) */
        return rendergraph_render_with_labels(rgraph, surface,
            pal, prend,
            x0, y0, zoom,
            rendered_pos, rendered_rot, rendered_flip,
            frame_i, mapper, palmapper,
            body->label_mappings_len, body->label_mappings);
    }else{
        /* Render just the rgraph */
        return rendergraph_render(rgraph, surface,
            pal, prend,
            x0, y0, zoom,
            rendered_pos, rendered_rot, rendered_flip,
            frame_i, mapper, palmapper);
    }
}
