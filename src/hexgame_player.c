

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




/*******************
 * PLAYER KEY INFO *
 *******************/

void player_keyinfo_reset(player_keyinfo_t *info){
    for(int i = 0; i < PLAYER_KEYS; i++){
        info->isdown[i] = false;
        info->wasdown[i] = false;
        info->wentdown[i] = false;
    }
}

void player_keyinfo_copy(player_keyinfo_t *info1, player_keyinfo_t *info2){
    for(int i = 0; i < PLAYER_KEYS; i++){
        info1->isdown[i] = info2->isdown[i];
        info1->wasdown[i] = info2->wasdown[i];
        info1->wentdown[i] = info2->wentdown[i];
    }
}

int fus_lexer_get_player_keyinfo(fus_lexer_t *lexer,
    player_keyinfo_t *info
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
        if(strlen(name) != 1 || !strchr("udlrfb", name[0])){
            return fus_lexer_unexpected(lexer,
                "u or d or l or r or f or b");}
        key_c = name[0];
        free(name);

        int key_i = player_get_key_i(NULL, key_c, true);

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



/***********************
 * PLAYER INIT/CLEANUP *
 ***********************/

void player_cleanup(player_t *player){
    stateset_cleanup(&player->stateset);
    player_recording_cleanup(&player->recording);
}

int player_init(player_t *player, hexmap_t *map,
    char *stateset_filename, const char *state_name, int keymap,
    vec_t respawn_pos, char *respawn_filename
){
    int err;

    player->keymap = keymap;
    for(int i = 0; i < PLAYER_KEYS; i++)player->key_code[i] = 0;
    if(keymap == 0){
        player->key_code[PLAYER_KEY_U] = SDLK_UP;
        player->key_code[PLAYER_KEY_D] = SDLK_DOWN;
        player->key_code[PLAYER_KEY_L] = SDLK_LEFT;
        player->key_code[PLAYER_KEY_R] = SDLK_RIGHT;
    }else if(keymap == 1){
        player->key_code[PLAYER_KEY_U] = SDLK_w;
        player->key_code[PLAYER_KEY_D] = SDLK_s;
        player->key_code[PLAYER_KEY_L] = SDLK_a;
        player->key_code[PLAYER_KEY_R] = SDLK_d;
    }

    if(stateset_filename != NULL){
        err = player_init_stateset(player, stateset_filename, state_name,
            map);
        if(err)return err;
    }else{
        /* We really really expect you to call player_init_stateset
        right away! */
        player_set_state(player, NULL);
    }

    vec_cpy(map->space->dims, player->respawn_pos, respawn_pos);
    vec_cpy(map->space->dims, player->pos, respawn_pos);

    player->cur_submap = NULL;
    player->respawn_filename = respawn_filename;

    return 0;
}


/***************
 * PLAYER MISC *
 ***************/

rot_t player_get_rot(player_t *player, const vecspace_t *space){
    rot_t rot = player->rot;
    if(player->turn){
        rot = rot_contain(space->rot_max,
            space->rot_max/2 - rot);}
    return rot;
}

void player_init_trf(player_t *player, trf_t *trf, vecspace_t *space){
    vec_cpy(space->dims, trf->add, player->pos);
    trf->rot = player_get_rot(player, space);
    trf->flip = player->turn;
}


/****************
 * PLAYER STATE *
 ****************/

int player_init_stateset(player_t *player, const char *stateset_filename,
    const char *state_name, hexmap_t *map
){
    int err;

    err = stateset_load(&player->stateset, strdup(stateset_filename),
        map->prend, map->space);
    if(err)return err;

    if(state_name != NULL){
        err = player_set_state(player, state_name);
        if(err)return err;
    }else{
        player->state = player->stateset.states[0];
    }

    return 0;
}

int player_set_state(player_t *player, const char *state_name){
    if(state_name == NULL){
        player->state = NULL;
    }else{
        player->state = stateset_get_state(&player->stateset, state_name);
        if(player->state == NULL){
            fprintf(stderr, "Couldn't init player stateset: "
                "couldn't find state %s in stateset %s\n",
                state_name, player->stateset.filename);
            return 2;}
    }
    player->frame_i = 0;
    player->cooldown = 0;
    return 0;
}


/****************
 * PLAYER INPUT *
 ****************/

void player_keydown(player_t *player, int key_i){
    if(key_i < 0 || key_i >= PLAYER_KEYS)return;
    player->keyinfo.isdown[key_i] = true;
    player->keyinfo.wasdown[key_i] = true;
    player->keyinfo.wentdown[key_i] = true;

    if(player->recording.action == 2){
        /* record */
        player_maybe_record_wait(player);

        char c = player_get_key_c(player, key_i, true);
        fprintf(player->recording.file, "+%c", c);
        if(DEBUG_RECORDINGS)printf("+%c\n", c);

        /*
        char buffer[3];
        buffer[0] = '+';
        buffer[1] = c;
        buffer[2] = '\0';
        int err = player_record(player, buffer);
        if(err){perror("player_record failed");}
        */
    }
}

void player_keyup(player_t *player, int key_i){
    if(key_i < 0 || key_i >= PLAYER_KEYS)return;
    player->keyinfo.isdown[key_i] = false;

    if(player->recording.action == 2){
        /* record */
        player_maybe_record_wait(player);

        char c = player_get_key_c(player, key_i, true);
        fprintf(player->recording.file, "-%c", c);
        if(DEBUG_RECORDINGS)printf("-%c\n", c);

        /*
        char buffer[3];
        buffer[0] = '-';
        buffer[1] = c;
        buffer[2] = '\0';
        int err = player_record(player, buffer);
        if(err){perror("player_record failed");}
        */
    }
}

int player_get_key_i(player_t *player, char c, bool absolute){
    return
        c == 'u'? PLAYER_KEY_U:
        c == 'd'? PLAYER_KEY_D:
        c == 'l'? PLAYER_KEY_L:
        c == 'r'? PLAYER_KEY_R:
        c == 'f'? (!absolute && player->turn? PLAYER_KEY_L: PLAYER_KEY_R):
        c == 'b'? (!absolute && player->turn? PLAYER_KEY_R: PLAYER_KEY_L):
        -1;
}

char player_get_key_c(player_t *player, int key_i, bool absolute){
    return
        key_i == PLAYER_KEY_U? 'u':
        key_i == PLAYER_KEY_D? 'd':
        key_i == PLAYER_KEY_L? (absolute? 'l': player->turn? 'f': 'b'):
        key_i == PLAYER_KEY_R? (absolute? 'r': player->turn? 'b': 'f'):
        ' ';
}

int player_process_event(player_t *player, SDL_Event *event){

    if(player->state == NULL){
        fprintf(stderr, "%s: Skipping player with NULL state!\n",
            __FILE__);
        return 0;}

    if(event->type == SDL_KEYDOWN || event->type == SDL_KEYUP){
        if(!event->key.repeat){
            for(int i = 0; i < PLAYER_KEYS; i++){
                if(event->key.keysym.sym == player->key_code[i]){
                    if(event->type == SDL_KEYDOWN){
                        player_keydown(player, i);
                    }else{
                        player_keyup(player, i);
                    }
                }
            }
        }
    }
    return 0;
}


/****************
 * PLAYER RULES *
 ****************/

static int player_match_rule(player_t *player, hexmap_t *map,
    state_rule_t *rule, bool *rule_matched_ptr
){
    vecspace_t *space = map->space;

    bool rule_matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        if(DEBUG_RULES)printf("  if: %s\n", cond->type);
        if(cond->type == state_cond_type_false){
            rule_matched = false;
        }else if(cond->type == state_cond_type_key){

            int kstate_i = cond->u.key.kstate;
            bool *kstate =
                kstate_i == 0? player->keyinfo.isdown:
                kstate_i == 1? player->keyinfo.wasdown:
                kstate_i == 2? player->keyinfo.wentdown:
                NULL;
            if(kstate == NULL){
                fprintf(stderr, "kstate out of range: %i", kstate_i);
                return 2;}

            char c = cond->u.key.c;
            int key_i = player_get_key_i(player, c, false);
            if(key_i == -1){
                fprintf(stderr, "Unrecognized key char: %c", c);
                return 2;}

            rule_matched = kstate[key_i];
            if(!cond->u.key.yes)rule_matched = !rule_matched;
        }else if(cond->type == state_cond_type_coll){
            trf_t trf;
            player_init_trf(player, &trf, space);

            int flags = cond->u.coll.flags;
            bool all = flags & 1;
            bool yes = flags & 2;

            bool collide = hexmap_collide(map,
                cond->u.coll.collmap, &trf, yes? all: !all);
            rule_matched = yes? collide: !collide;
        }else{
            fprintf(stderr, "Unrecognized state rule condition: %s "
                "(state=%s, stateset=%s)\n",
                cond->type, rule->state->name,
                rule->state->stateset->filename);
            return 2;
        }
        if(!rule_matched)break;
    }

    if(DEBUG_RULES && !rule_matched)printf("    NO MATCH\n");

    *rule_matched_ptr = rule_matched;
    return 0;
}

static int player_apply_rule(player_t *player, hexmap_t *map,
    state_rule_t *rule
){
    int err;
    vecspace_t *space = map->space;
    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(DEBUG_RULES)printf("  then: %s\n", effect->type);
        if(effect->type == state_effect_type_print){
            printf("player %p says: %s\n", player, effect->u.msg);
        }else if(effect->type == state_effect_type_move){
            vec_t vec;
            vec_cpy(space->dims, vec, effect->u.vec);
            rot_t rot = player_get_rot(player, space);
            space->vec_flip(vec, player->turn);
            space->vec_rot(vec, rot);
            vec_add(space->dims, player->pos, vec);
        }else if(effect->type == state_effect_type_rot){
            rot_t effect_rot = effect->u.rot;
            player->rot = rot_rot(space->rot_max,
                player->rot, effect_rot);
        }else if(effect->type == state_effect_type_turn){
            player->turn = !player->turn;
            player->rot = rot_flip(space->rot_max, player->rot, true);
        }else if(effect->type == state_effect_type_goto){
            err = player_set_state(player, effect->u.goto_name);
            if(err)return err;
        }else if(effect->type == state_effect_type_delay){
            player->cooldown = effect->u.delay;
        }else{
            fprintf(stderr, "Unrecognized state rule effect: %s\n",
                effect->type);
            return 2;
        }
    }
    return 0;
}


/***************
 * PLAYER STEP *
 ***************/

int player_step(player_t *player, hexmap_t *map){
    int err;

    if(player->state == NULL){
        fprintf(stderr, "%s: Skipping player with NULL state!\n",
            __FILE__);
        return 0;}

    vecspace_t *space = map->space;

    /* Handle recording & playback */
    int rec_action = player->recording.action;
    if(rec_action == 1){
        /* play */
        if(player->recording.data != NULL){
            err = player_recording_step(player);
            if(err)return err;
        }
    }else if(rec_action == 2){
        /* record */
        player->recording.wait++;
    }
    if(rec_action != 0 && DEBUG_RECORDINGS){
        printf("KEYS: ");
        #define DEBUG_PRINT_KEYS(keys) { \
            printf("["); \
            for(int i = 0; i < PLAYER_KEYS; i++)printf("%i", keys[i]); \
            printf("]"); \
        }
        DEBUG_PRINT_KEYS(player->keyinfo.isdown)
        DEBUG_PRINT_KEYS(player->keyinfo.wasdown)
        DEBUG_PRINT_KEYS(player->keyinfo.wentdown)
        #undef DEBUG_PRINT_KEYS
        printf("\n");
    }

    /* Increment frame */
    player->frame_i++;
    if(player->frame_i == MAX_FRAME_I)player->frame_i = 0;

    /* Handle animation & input */
    if(player->cooldown > 0){
        player->cooldown--;
    }else{
        state_t *state = player->state;
        for(int i = 0; i < state->rules_len; i++){
            state_rule_t *rule = state->rules[i];

            if(DEBUG_RULES)printf("player %p rule %i:\n", player, i);

            bool rule_matched;
            err = player_match_rule(player, map, rule, &rule_matched);
            if(err)return err;

            if(rule_matched){
                err = player_apply_rule(player, map, rule);
                if(err)return err;
                break;
            }
        }

        /* start of new frame, no keys have gone down yet. */
        for(int i = 0; i < PLAYER_KEYS; i++){
            player->keyinfo.wasdown[i] = player->keyinfo.isdown[i];
            player->keyinfo.wentdown[i] = false;}
    }

    /* Figure out current submap */
    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        if(!submap->solid)continue;

        hexcollmap_t *collmap = &submap->collmap;

        trf_t index = {0};
        hexspace_set(index.add,
             player->pos[0] - submap->pos[0],
            -player->pos[1] + submap->pos[1]);

        /* savepoints are currently this HACK */
        /* TODO: separate respawn file for each player? */
        hexcollmap_elem_t *face =
            hexcollmap_get_face(collmap, &index);
        if(face != NULL && face->tile_c == 'S'){
            if(!vec_eq(space->dims, player->respawn_pos, player->pos)){
                vec_cpy(space->dims, player->respawn_pos, player->pos);
                if(player->respawn_filename != NULL){
                    FILE *f = fopen(player->respawn_filename, "w");
                    if(f != NULL){
                        fprintf(f, "%i %i\n",
                            player->pos[0], player->pos[1]);
                        fclose(f);
                    }
                }
            }
        }

        hexcollmap_elem_t *vert =
            hexcollmap_get_vert(collmap, &index);
        if(hexcollmap_elem_is_solid(vert)){
            player->cur_submap = submap;
            break;
        }
    }

    return 0;
}


/*****************
 * PLAYER RENDER *
 *****************/

int player_render(player_t *player,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom,
    hexmap_t *map, vec_t camera_renderpos, prismelmapper_t *mapper
){
    int err;

    if(player->state == NULL){
        fprintf(stderr, "%s: Skipping player with NULL state!\n",
            __FILE__);
        return 0;}

    vecspace_t *space = map->space;

    rendergraph_t *rgraph = player->state->rgraph;

    vec_t pos;
    vec4_vec_from_hexspace(pos, player->pos);
    vec_sub(rgraph->space->dims, pos, camera_renderpos);
    vec_mul(rgraph->space, pos, map->unit);

    rot_t player_rot = player_get_rot(player, space);
    rot_t rot = vec4_rot_from_hexspace(player_rot);
    //rot_t rot = vec4_rot_from_hexspace(
    //    rot_contain(space->rot_max,
    //        player_rot + rot_inv(space->rot_max, game->camera_rot)));
    flip_t flip = player->turn;
    int frame_i = player->frame_i;

    err = rendergraph_render(rgraph, renderer, surface,
        pal, map->prend,
        x0, y0, zoom,
        pos, rot, flip, frame_i, mapper);
    if(err)return err;

    return 0;
}

