

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

    body->out_of_bounds = false;
    body->map = map;
    body->cur_submap = NULL;

    err = vars_init(&body->vars);
    if(err)return err;

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

int body_respawn(body_t *body, vec_t pos, rot_t rot, bool turn,
    hexmap_t *map
){
    int err;
    hexgame_t *game = body->game;
    vecspace_t *space = game->space;

    vec_cpy(space->dims, body->pos, pos);
    body->rot = rot;
    body->turn = turn;
    err = body_set_state(body, body->stateset.states[0]->name, true);
    if(err)return err;

    err = body_move_to_map(body, map);
    if(err)return err;

    keyinfo_reset(&body->keyinfo);

    return 0;
}

int body_add_body(body_t *body, body_t **new_body_ptr,
    const char *stateset_filename, const char *state_name,
    palettemapper_t *palmapper,
    vec_t addpos, rot_t addrot, bool turn
){
    /* Adds a new body at same location as another body */
    int err;
    ARRAY_PUSH_NEW(body_t*, body->map->bodies, new_body)
    err = body_init(new_body, body->game, body->map,
        stateset_filename, state_name, palmapper);
    if(err)return err;
    vecspace_t *space = body->map->space;

    vec_cpy(space->dims, new_body->pos, body->pos);
    rot_t rot = body_get_rot(body);
    space->vec_flip(addpos, body->turn);
    space->vec_rot(addpos, rot);
    vec_add(space->dims, new_body->pos, addpos);

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
    vecspace_t *space = body->map->space;
    rot_t rot = body->rot;
    if(body->turn){
        rot = rot_contain(space->rot_max,
            space->rot_max/2 - rot);}
    return rot;
}

void body_init_trf(body_t *body, trf_t *trf){
    vecspace_t *space = body->map->space;
    vec_cpy(space->dims, trf->add, body->pos);
    trf->rot = body_get_rot(body);
    trf->flip = body->turn;
}

void body_flash_cameras(body_t *body, Uint8 r, Uint8 g, Uint8 b,
    int percent
){
    hexgame_t *game = body->game;
    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        if(camera->body != body)continue;
        camera_colors_flash_white(camera, 30);
    }
}

void body_reset_cameras(body_t *body){
    hexgame_t *game = body->game;
    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        if(camera->body != body)continue;
        camera->should_reset = true;
    }
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
    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        if(camera->body == body){
            camera->map = map;
            camera->cur_submap = body->cur_submap;
        }
    }
    return 0;
}


/**************
 * BODY STATE *
 **************/

int body_init_stateset(body_t *body, const char *stateset_filename,
    const char *state_name
){
    int err;
    hexmap_t *map = body->map;

    err = stateset_load(&body->stateset, strdup(stateset_filename),
        map->prend, map->space);
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
    return body_init_stateset(body, stateset_filename, state_name);
}

int body_set_state(body_t *body, const char *state_name,
    bool reset_cooldown
){
    if(state_name == NULL){
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
    int key_i =
        c == 'x'? KEYINFO_KEY_ACTION1:
        c == 'y'? KEYINFO_KEY_ACTION2:
        c == 'u'? KEYINFO_KEY_U:
        c == 'd'? KEYINFO_KEY_D:
        c == 'l'? KEYINFO_KEY_L:
        c == 'r'? KEYINFO_KEY_R:
        c == 'f'? (!absolute && body->turn? KEYINFO_KEY_L: KEYINFO_KEY_R):
        c == 'b'? (!absolute && body->turn? KEYINFO_KEY_R: KEYINFO_KEY_L):
        -1;
    return key_i;
}

char body_get_key_c(body_t *body, int key_i, bool absolute){
    return
        key_i == KEYINFO_KEY_ACTION1? 'x':
        key_i == KEYINFO_KEY_ACTION2? 'y':
        key_i == KEYINFO_KEY_U? 'u':
        key_i == KEYINFO_KEY_D? 'd':
        key_i == KEYINFO_KEY_L? (absolute? 'l': body->turn? 'f': 'b'):
        key_i == KEYINFO_KEY_R? (absolute? 'r': body->turn? 'b': 'f'):
        ' ';
}


/**************
 * BODY RULES *
 **************/

static int body_match_rule(body_t *body,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, bool *rule_matched_ptr
){

    /* NOTE: body and/or actor may be NULL.
    We are basically reusing the rule/cond/effect structure for bodys
    and actors; most conds/effects naturally apply to one or the other.
    E.g. keypress stuff is for the body; "play" is for actor.
    However, actor may want to check some stuff about the body, so
    it may make use of "body-oriented" rules. */

    bool rule_matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        if(DEBUG_RULES)printf("  if: %s\n", cond->type);

        #define RULE_PERROR() \
            fprintf(stderr, " (cond=%s, state=%s, stateset=%s)\n", \
                cond->type, rule->state->name, \
                rule->state->stateset->filename);

        if(cond->type == state_cond_type_false){
            rule_matched = false;
        }else if(cond->type == state_cond_type_key){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                break;}

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
                break;}

            if(body->state == NULL){
                rule_matched = false;
                break;}

            hexmap_t *map = body->map;
            vecspace_t *space = map->space;

            trf_t hitbox_trf;
            body_init_trf(body, &hitbox_trf);
            hexcollmap_t *hitbox = cond->u.coll.collmap;

            int flags = cond->u.coll.flags;
            bool all = flags & 1;
            bool yes = flags & 2;
            bool water = flags & 4;
            bool against_bodies = flags & 8;

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
                hexmap_submap_t *collide_savepoint = NULL;
                hexmap_submap_t *collide_door = NULL;
                hexmap_submap_t *collide_water = NULL;
                hexmap_collide_special(map, hitbox, &hitbox_trf,
                    &collide_savepoint, &collide_door, &collide_water);
                bool collide = collide_water != NULL;
                rule_matched = yes? collide: !collide;
            }else{
                bool collide = hexmap_collide(map,
                    hitbox, &hitbox_trf, yes? all: !all);
                rule_matched = yes? collide: !collide;
            }
        }else{
            fprintf(stderr, "Unrecognized state rule condition: %s",
                cond->type);
            RULE_PERROR()
            return 2;
        }
        if(!rule_matched)break;
        #undef RULE_PERROR
    }

    if(DEBUG_RULES && !rule_matched)printf("    NO MATCH\n");

    *rule_matched_ptr = rule_matched;
    return 0;
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
                break;}

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
                break;}

            vecspace_t *space = body->map->space;
            rot_t effect_rot = effect->u.rot;
            body->rot = rot_rot(space->rot_max,
                body->rot, effect_rot);
        }else if(effect->type == state_effect_type_turn){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                break;}

            vecspace_t *space = body->map->space;
            body->turn = !body->turn;
            body->rot = rot_flip(space->rot_max, body->rot, true);
        }else if(effect->type == state_effect_type_goto){
            *gotto_ptr = &effect->u.gotto;
        }else if(effect->type == state_effect_type_delay){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                break;}
            body->cooldown = effect->u.delay;
        }else if(effect->type == state_effect_type_action){
            const char *action_name = effect->u.action_name;
            if(!strcmp(action_name, "ping")){
                fprintf(stderr, "pong\n");
            }else if(
                !strcmp(action_name, "spit") ||
                !strcmp(action_name, "spit_crouch") ||
                !strcmp(action_name, "spit_up")
            ){
                if(body == NULL){
                    fprintf(stderr, "No body");
                    RULE_PERROR()
                    break;}
                bool crouch = action_name[4] == '_' && action_name[5] == 'c';
                bool up = action_name[4] == '_' && action_name[5] == 'u';

                vec_t addpos = {0, 0};
                if(up)addpos[0] = 1;
                rot_t addrot = up? 1: 0;
                bool turn = false;

                body_t *new_body;
                err = body_add_body(body, &new_body, "anim/spit.fus",
                    crouch? "crouch_fly": "fly", NULL,
                    addpos, addrot, turn);
                if(err)return err;
            }else{
                fprintf(stderr, "Unrecognized action: %s\n",
                    action_name);
                return 2;
            }
        }else if(effect->type == state_effect_type_play){
            if(actor == NULL){
                fprintf(stderr, "No actor");
                RULE_PERROR()
                break;}
            const char *play_filename = effect->u.play_filename;
            err = body_load_recording(body, play_filename, false);
            if(err)return err;
            err = body_play_recording(body);
            if(err)return err;
        }else if(effect->type == state_effect_type_die){
            if(body == NULL){
                fprintf(stderr, "No body");
                RULE_PERROR()
                break;}
            body->dead = effect->u.dead;
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

            hexmap_submap_t *collide_savepoint = NULL;
            hexmap_submap_t *collide_door = NULL;
            hexmap_submap_t *collide_water = NULL;
            hexmap_collide_special(map, hitbox, &hitbox_trf,
                &collide_savepoint, &collide_door, &collide_water);

            if(collide_water)new_submap = collide_water;
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
        if(body->recording.data != NULL){
            err = recording_step(&body->recording);
            if(err)return err;
        }
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

int body_collide_against_body(body_t *body, body_t *body_other){
    int err;
    if(body_other->state->crushes){
        /* Bodies whose recording is playing cannot die */
        /* MAYBE TODO: These bodies should die too, but then their
        recording should restart after a brief pause?
        Maybe we can reuse body->cooldown for the pause. */
        if(body->recording.action != 1){
            /* Hardcoded "dead" state name... I suppose we could
            have a char* body->dead_anim_name or something, but whatever.
            We more or less expect this "dead" state to, for instance,
            cause the body to die. Maybe after exploding or whatever. */
            err = body_set_state(body, "dead", true);
            if(err)return err;
        }
    }
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

