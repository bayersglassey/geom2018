

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



static void print_tabs(FILE *file, int depth){
    for(int i = 0; i < depth; i++)fprintf(file, "  ");
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

        int key_i = body_get_key_i(NULL, key_c, true);

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

    if(stateset_filename != NULL){
        err = body_init_stateset(body, stateset_filename, state_name);
        if(err)return err;
    }else{
        /* We really really expect you to call body_init_stateset
        right away! */
        body_set_state(body, NULL, true);
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
    fprintf(stderr, "stateset: %s\n",
        body->stateset.filename);
    print_tabs(stderr, depth);
    fprintf(stderr, "state: %s\n",
        body->state->name);
    print_tabs(stderr, depth);
    fprintf(stderr, "pos: %i %i\n",
        body->pos[0], body->pos[1]);
    print_tabs(stderr, depth);
    fprintf(stderr, "rot: %i\n", body->rot);
    print_tabs(stderr, depth);
    fprintf(stderr, "turn: %c\n", body->turn? 'y': 'n');
}

int body_respawn(body_t *body, vec_t pos, rot_t rot, bool turn,
    hexmap_t *map
){
    /* Respawn body at given map and location */
    int err;

    hexgame_t *game = body->game;
    vecspace_t *space = game->space;

    /* Set body pos, rot, turn */
    vec_cpy(space->dims, body->pos, pos);
    body->rot = rot;
    body->turn = turn;

    /* Set body state */
    err = body_set_state(body, body->stateset.states[0]->name, true);
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

    rot_t rot = body_get_rot(body);

    vec_t addpos_cpy;
    vec_cpy(space->dims, addpos_cpy, addpos);
    space->vec_flip(addpos_cpy, body->turn);
    space->vec_rot(addpos_cpy, rot);

    vec_cpy(space->dims, new_body->pos, body->pos);
    vec_add(space->dims, new_body->pos, addpos_cpy);

    new_body->rot =
        rot_rot(space->rot_max, body->rot, addrot);

    new_body->turn = turn? !body->turn: body->turn;

    if(new_body_ptr)*new_body_ptr = new_body;
    return 0;
}



/*************
 * BODY MISC *
 *************/

rot_t body_get_rot(body_t *body){
    /* Coverts body->rot/turn into the rot_t value representing
    the vector parallel to body's bottom (that is, the bottom of
    body's hitbox, where the body rests upon the ground) */
    vecspace_t *space = body->map->space;
    rot_t rot = body->rot;
    if(body->turn){
        rot = rot_contain(space->rot_max,
            space->rot_max/2 - rot);}
    return rot;
}

void body_init_trf(body_t *body, trf_t *trf){
    /* Initializes trf so that it represents the transformation needed to
    bring a body from zero pos/rot/turn to body->pos/rot/turn.
    If you see what I mean.
    In particular, we use this to set up transformations which will move
    the body's hitbox over top of it. */
    vecspace_t *space = body->map->space;
    vec_cpy(space->dims, trf->add, body->pos);
    trf->rot = body_get_rot(body);
    trf->flip = body->turn;
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

int body_init_stateset(body_t *body, const char *stateset_filename,
    const char *state_name
){
    /* body->stateset is expected to have been cleaned up already;
    basically this function is a hack, only ever called (?) by body_init
    or body_set_stateset */
    int err;
    hexmap_t *map = body->map;

    char *stateset_filename_dup = strdup(stateset_filename);
    if(!stateset_filename_dup)return 1;
    err = stateset_load(&body->stateset, stateset_filename_dup,
        NULL, map->prend, map->space);
    if(err)return err;

    if(state_name == NULL){
        state_name = body->stateset.states[0]->name;
    }

    err = body_set_state(body, state_name, true);
    if(err)return err;

    return 0;
}

int body_set_stateset(body_t *body, const char *stateset_filename,
    const char *state_name
){
    stateset_cleanup(&body->stateset);

    /* Make sure freed pointers are zeroed, in case body_init_stateset fails
    before overwriting them */
    memset(&body->stateset, 0, sizeof(body->stateset));

    return body_init_stateset(body, stateset_filename, state_name);
}

int body_set_state(body_t *body, const char *state_name,
    bool reset_cooldown
){
    if(state_name == NULL){
        /* Is this ever used other than by body_init, which expects you
        to set a proper state ASAP?.. */
        body->state = NULL;
    }else{
        body->state = stateset_get_state(&body->stateset, state_name);
        if(body->state == NULL){
            fprintf(stderr, "Couldn't init body stateset: "
                "couldn't find state %s in stateset %s\n",
                state_name, body->stateset.filename);
            return 2;}
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
        /* record */
        body_maybe_record_wait(body);

        char c = body_get_key_c(body, key_i, true);
        fprintf(body->recording.file, "+%c", c);
        if(DEBUG_RECORDINGS)printf("+%c\n", c);

        /*
        char buffer[3];
        buffer[0] = '+';
        buffer[1] = c;
        buffer[2] = '\0';
        int err = body_record(body, buffer);
        if(err){perror("body_record failed");}
        */
    }
}

void body_keyup(body_t *body, int key_i){
    if(key_i < 0 || key_i >= KEYINFO_KEYS)return;
    body->keyinfo.isdown[key_i] = false;

    if(body->recording.action == 2){
        /* record */
        body_maybe_record_wait(body);

        char c = body_get_key_c(body, key_i, true);
        fprintf(body->recording.file, "-%c", c);
        if(DEBUG_RECORDINGS)printf("-%c\n", c);

        /*
        char buffer[3];
        buffer[0] = '-';
        buffer[1] = c;
        buffer[2] = '\0';
        int err = body_record(body, buffer);
        if(err){perror("body_record failed");}
        */
    }
}

int body_get_key_i(body_t *body, char c, bool absolute){
    bool turn = false;
    if(body){
        turn = body->confused? !body->turn: body->turn;
    }
    int key_i =
        c == 'x'? KEYINFO_KEY_ACTION1:
        c == 'y'? KEYINFO_KEY_ACTION2:
        c == 'u'? KEYINFO_KEY_U:
        c == 'd'? KEYINFO_KEY_D:
        c == 'l'? KEYINFO_KEY_L:
        c == 'r'? KEYINFO_KEY_R:
        c == 'f'? (!absolute && turn? KEYINFO_KEY_L: KEYINFO_KEY_R):
        c == 'b'? (!absolute && turn? KEYINFO_KEY_R: KEYINFO_KEY_L):
        -1;
    return key_i;
}

char body_get_key_c(body_t *body, int key_i, bool absolute){
    bool turn = false;
    if(body){
        turn = body->confused? !body->turn: body->turn;
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


/**************
 * BODY RULES *
 **************/

static int body_match_cond(body_t *body,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, state_cond_t *cond,
    bool *rule_matched_ptr
){
    int err;

    if(DEBUG_RULES)printf("  if: %s\n", cond->type);

    #define RULE_PERROR() \
        fprintf(stderr, " (cond=%s, state=%s, stateset=%s)\n", \
            cond->type, rule->state->name, \
            rule->state->stateset->filename);

    bool rule_matched = false;

    if(cond->type == state_cond_type_false){
        rule_matched = false;
    }else if(cond->type == state_cond_type_key){
        if(body == NULL){
            fprintf(stderr, "No body");
            RULE_PERROR()
            return 2;}

        int kstate_i = cond->u.key.kstate;
        bool *kstate =
            kstate_i == 0? body->keyinfo.isdown:
            kstate_i == 1? body->keyinfo.wasdown:
            kstate_i == 2? body->keyinfo.wentdown:
            NULL;
        if(kstate == NULL){
            fprintf(stderr, "kstate out of range: %i", kstate_i);
            RULE_PERROR()
            return 2;}

        char c = cond->u.key.c;
        int key_i = body_get_key_i(body, c, false);
        if(key_i == -1){
            fprintf(stderr, "Unrecognized key char: %c", c);
            RULE_PERROR()
            return 2;}

        rule_matched = kstate[key_i];
        if(!cond->u.key.yes)rule_matched = !rule_matched;
    }else if(cond->type == state_cond_type_coll){
        if(body == NULL){
            fprintf(stderr, "No body");
            RULE_PERROR()
            return 2;}

        if(body->state == NULL){
            rule_matched = false;
        }else{
            hexmap_t *map = body->map;
            vecspace_t *space = map->space;

            trf_t hitbox_trf;
            body_init_trf(body, &hitbox_trf);
            hexcollmap_t *hitbox = cond->u.coll.collmap;

            int flags = cond->u.coll.flags;
            bool all = flags & ANIM_COND_FLAGS_ALL;
            bool yes = flags & ANIM_COND_FLAGS_YES;
            bool water = flags & ANIM_COND_FLAGS_WATER;
            bool against_bodies = flags & ANIM_COND_FLAGS_BODIES;

            if(against_bodies){
                int n_matches = 0;
                for(int j = 0; j < map->bodies_len; j++){
                    body_t *body_other = map->bodies[j];
                    if(body == body_other)continue;
                    if(body_other->state == NULL)continue;
                    hexcollmap_t *hitbox_other = body_other->state->hitbox;
                    if(hitbox_other == NULL)continue;

                    trf_t hitbox_other_trf;
                    body_init_trf(body_other, &hitbox_other_trf);

                    /* The other body has a hitbox! Do the collision... */
                    bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                        hitbox_other, &hitbox_other_trf, space,
                        yes? all: !all);
                    if(yes? collide: !collide)n_matches++;
                }
                rule_matched = n_matches > 0;
            }else if(water){
                hexmap_collision_t collision;
                hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);
                bool collide = collision.water.submap != NULL;
                rule_matched = yes? collide: !collide;
            }else{
                bool collide = hexmap_collide(map,
                    hitbox, &hitbox_trf, yes? all: !all);
                rule_matched = yes? collide: !collide;
            }
        }
    }else if(cond->type == state_cond_type_chance){
        int n = rand() % 100;
        rule_matched = n <= cond->u.percent;
    }else if(
        cond->type == state_cond_type_any ||
        cond->type == state_cond_type_all
    ){
        bool all = cond->type == state_cond_type_all;
        rule_matched = all? true: false;
        for(int i = 0; i < cond->u.subconds.conds_len; i++){
            state_cond_t *subcond = cond->u.subconds.conds[i];
            err = body_match_cond(body, actor, game, rule, subcond,
                &rule_matched);
            if(err)return err;
            if((all && !rule_matched) || (!all && rule_matched))break;
        }
    }else{
        fprintf(stderr, "Unrecognized state rule condition: %s",
            cond->type);
        RULE_PERROR()
        return 2;
    }
    #undef RULE_PERROR

    *rule_matched_ptr = rule_matched;
    return 0;
}

static int body_match_rule(body_t *body,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, bool *rule_matched_ptr
){
    int err;

    /* NOTE: body and/or actor may be NULL.
    We are basically reusing the rule/cond/effect structure for bodies
    and actors; most conds/effects naturally apply to one or the other.
    E.g. keypress stuff is for the body; "play" is for actor.
    However, actor may want to check some stuff about the body, so
    it may make use of "body-oriented" rules. */

    bool rule_matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        err = body_match_cond(body, actor, game, rule, cond, &rule_matched);
        if(err)return err;
        if(!rule_matched)break;
    }

    if(DEBUG_RULES && !rule_matched)printf("    NO MATCH\n");

    *rule_matched_ptr = rule_matched;
    return 0;
}

static void effect_apply_boolean(int boolean, bool *b_ptr){
    if(boolean == EFFECT_BOOLEAN_TRUE)*b_ptr = true;
    else if(boolean == EFFECT_BOOLEAN_FALSE)*b_ptr = false;
    else if(boolean == EFFECT_BOOLEAN_TOGGLE)*b_ptr = !*b_ptr;
}

static int body_apply_rule(body_t *body,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, state_effect_goto_t **gotto_ptr
){
    int err;

    /* NOTE: body and/or actor may be NULL.
    See comment on body_match_rule. */

    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(DEBUG_RULES)printf("  then: %s\n", effect->type);

        #define RULE_PERROR() \
            fprintf(stderr, " (effect=%s, state=%s, stateset=%s)\n", \
                effect->type, rule->state->name, \
                rule->state->stateset->filename);

        if(effect->type == state_effect_type_print){
            if(body != NULL)printf("body %p", body);
            else printf("unknown body");
            if(actor != NULL)printf(" (actor %p)", actor);
            printf(" says: %s\n", effect->u.msg);
        }else if(effect->type == state_effect_type_move){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            vecspace_t *space = body->map->space;
            vec_t vec;
            vec_cpy(space->dims, vec, effect->u.vec);
            rot_t rot = body_get_rot(body);
            space->vec_flip(vec, body->turn);
            space->vec_rot(vec, rot);
            vec_add(space->dims, body->pos, vec);
        }else if(effect->type == state_effect_type_rot){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            vecspace_t *space = body->map->space;
            rot_t effect_rot = effect->u.rot;
            body->rot = rot_rot(space->rot_max,
                body->rot, effect_rot);
        }else if(effect->type == state_effect_type_turn){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            vecspace_t *space = body->map->space;
            body->turn = !body->turn;
            body->rot = rot_flip(space->rot_max, body->rot, true);
        }else if(effect->type == state_effect_type_goto){
            *gotto_ptr = &effect->u.gotto;
        }else if(effect->type == state_effect_type_delay){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}
            body->cooldown = effect->u.delay;
        }else if(effect->type == state_effect_type_spawn){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}

            state_effect_spawn_t *spawn = &effect->u.spawn;

            /* TODO: look up palmapper from spawn->palmapper_name */
            palettemapper_t *palmapper = NULL;

            body_t *new_body;
            err = body_add_body(body, &new_body,
                spawn->stateset_filename,
                spawn->state_name, palmapper,
                spawn->pos, spawn->rot, spawn->turn);
            if(err)return err;
        }else if(effect->type == state_effect_type_play){
            if(actor == NULL){
                fprintf(stderr, "No actor");
                RULE_PERROR()
                return 2;}
            const char *play_filename = effect->u.play_filename;
            err = body_load_recording(body, play_filename, false);
            if(err)return err;
            err = body_play_recording(body);
            if(err)return err;
        }else if(effect->type == state_effect_type_die){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}
            body->dead = effect->u.dead;
        }else if(effect->type == state_effect_type_confused){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                return 2;}
            effect_apply_boolean(effect->u.boolean, &body->confused);
        }else{
            fprintf(stderr, "Unrecognized state rule effect: %s\n",
                effect->type);
            return 2;
        }
        #undef RULE_PERROR
    }
    return 0;
}

int state_handle_rules(state_t *state, body_t *body,
    actor_t *actor, hexgame_t *game, state_effect_goto_t **gotto_ptr
){
    int err;

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];

        if(DEBUG_RULES){printf("body %p, actor %p, rule %i:\n",
            body, actor, i);}

        bool rule_matched;
        err = body_match_rule(body, actor, game, rule,
            &rule_matched);
        if(err)return err;

        if(rule_matched){
            err = body_apply_rule(body, actor, game, rule,
                gotto_ptr);
            if(err)return err;
            break;
        }
    }

    return 0;
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
        if(!submap->solid)continue;

        hexcollmap_t *collmap = &submap->collmap;

        /* A HACK! */
        trf_t index = {0};
        hexspace_set(index.add,
             body->pos[0] - submap->pos[0],
            -body->pos[1] + submap->pos[1]);

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
            body_init_trf(body, &hitbox_trf);

            hexmap_collision_t collision;
            hexmap_collide_special(map, hitbox, &hitbox_trf, &collision);

            hexmap_submap_t *water_submap = collision.water.submap;
            if(water_submap)new_submap = water_submap;
        }
    }

    if(new_submap != NULL)body->cur_submap = new_submap;
}

int body_step(body_t *body, hexgame_t *game){
    int err;

    if(body->state == NULL){
        return 0;}

    hexmap_t *map = body->map;
    vecspace_t *space = map->space;

    /* Handle recording & playback */
    int rec_action = body->recording.action;
    if(rec_action == 1){
        /* play */
        err = recording_step(&body->recording);
        if(err)return err;
    }else if(rec_action == 2){
        /* record */
        body->recording.wait++;
    }
    if(rec_action != 0 && DEBUG_RECORDINGS){
        printf("KEYS: ");
        #define DEBUG_PRINT_KEYS(keys) { \
            printf("["); \
            for(int i = 0; i < KEYINFO_KEYS; i++)printf("%i", keys[i]); \
            printf("]"); \
        }
        DEBUG_PRINT_KEYS(body->keyinfo.isdown)
        DEBUG_PRINT_KEYS(body->keyinfo.wasdown)
        DEBUG_PRINT_KEYS(body->keyinfo.wentdown)
        #undef DEBUG_PRINT_KEYS
        printf("\n");
    }

    /* Increment frame */
    body->frame_i++;
    if(body->frame_i == MAX_FRAME_I)body->frame_i = 0;

    /* Handle animation & input */
    if(body->cooldown > 0){
        body->cooldown--;
    }else{
        /* Handle current state's rules */
        handle: {
            state_effect_goto_t *gotto = NULL;
            err = state_handle_rules(body->state, body, NULL, game,
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

static const char *_body_handle_collmsg(body_t *body, const char *msg){
    state_t *state = body->state;
    for(int j = 0; j < state->collmsg_handlers_len; j++){
        collmsg_handler_t *handler = &state->collmsg_handlers[j];
        if(body->stateset.debug_collision){
            fprintf(stderr, "    -> state handler: %s -> %s\n",
                handler->msg, handler->state_name);
        }
        if(!strcmp(msg, handler->msg)){
            return handler->state_name;
        }
    }
    stateset_t *stateset = &body->stateset;
    for(int j = 0; j < stateset->collmsg_handlers_len; j++){
        collmsg_handler_t *handler = &stateset->collmsg_handlers[j];
        if(body->stateset.debug_collision){
            fprintf(stderr, "    -> stateset handler: %s -> %s\n",
                handler->msg, handler->state_name);
        }
        if(!strcmp(msg, handler->msg)){
            return handler->state_name;
        }
    }
    return NULL;
}

static const char *_body_handle_other_bodies_collmsgs(body_t *body, body_t *body_other){
    const char *state_name = NULL;
    state_t *state = body_other->state;
    for(int i = 0; i < state->collmsgs_len; i++){
        const char *msg = state->collmsgs[i];
        if(body->stateset.debug_collision){
            fprintf(stderr, "  -> state collmsg: %s\n", msg);
        }
        state_name = _body_handle_collmsg(body, msg);
        if(state_name)return state_name;
    }
    stateset_t *stateset = &body_other->stateset;
    for(int i = 0; i < stateset->collmsgs_len; i++){
        const char *msg = stateset->collmsgs[i];
        if(body->stateset.debug_collision){
            fprintf(stderr, "  -> stateset collmsg: %s\n", msg);
        }
        state_name = _body_handle_collmsg(body, msg);
        if(state_name)return state_name;
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
    const char *state_name = _body_handle_other_bodies_collmsgs(body, body_other);
    if(!state_name)return 0;

    if(body->stateset.debug_collision){
        fprintf(stderr, "  -> *** handling with state: %s\n", state_name);
    }

    /* Body "handles" the collmsg by changing its state */
    err = body_set_state(body, state_name, true);
    if(err)return err;

    return 0;
}


/***************
 * BODY RENDER *
 ***************/

int body_render(body_t *body,
    SDL_Renderer *renderer, SDL_Surface *surface,
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
    vec4_vec_from_hexspace(pos, body->pos);
    vec_sub(rgraph->space->dims, pos, camera_renderpos);
    vec_mul(rgraph->space, pos, map->unit);

    rot_t body_rot = body_get_rot(body);
    rot_t rot = vec4_rot_from_hexspace(body_rot);
    flip_t flip = body->turn;
    int frame_i = body->frame_i;

    err = rendergraph_render(rgraph, renderer, surface,
        pal, prend,
        x0, y0, zoom,
        pos, rot, flip, frame_i, mapper);
    if(err)return err;

    return 0;
}

