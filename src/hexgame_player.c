

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
        if(strlen(name) != 1 || !strchr(ANIM_KEY_CS, name[0])){
            return fus_lexer_unexpected(lexer,
                "one of the characters: " ANIM_KEY_CS);}
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
    const char *stateset_filename, const char *state_name, int keymap,
    vec_t respawn_pos, char *respawn_filename
){
    int err;

    player->keymap = keymap;
    for(int i = 0; i < PLAYER_KEYS; i++)player->key_code[i] = 0;
    if(keymap == 0){
        player->key_code[PLAYER_KEY_ACTION] = SDLK_SPACE;
        player->key_code[PLAYER_KEY_U] = SDLK_UP;
        player->key_code[PLAYER_KEY_D] = SDLK_DOWN;
        player->key_code[PLAYER_KEY_L] = SDLK_LEFT;
        player->key_code[PLAYER_KEY_R] = SDLK_RIGHT;
    }else if(keymap == 1){
        player->key_code[PLAYER_KEY_ACTION] = SDLK_f;
        player->key_code[PLAYER_KEY_U] = SDLK_w;
        player->key_code[PLAYER_KEY_D] = SDLK_s;
        player->key_code[PLAYER_KEY_L] = SDLK_a;
        player->key_code[PLAYER_KEY_R] = SDLK_d;
    }

    player->palmapper = NULL;

    if(stateset_filename != NULL){
        err = player_init_stateset(player, stateset_filename, state_name,
            map);
        if(err)return err;
    }else{
        /* We really really expect you to call player_init_stateset
        right away! */
        player_set_state(player, NULL);
    }

    if(respawn_pos == NULL)respawn_pos = map->spawn;
    vec_cpy(map->space->dims, player->respawn_pos, respawn_pos);
    vec_cpy(map->space->dims, player->pos, respawn_pos);

    player->out_of_bounds = false;
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

    if(state_name == NULL){
        state_name = player->stateset.states[0]->name;
    }

    err = player_set_state(player, state_name);
    if(err)return err;

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
    player->dead = false;
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
    int key_i =
        c == 'x'? PLAYER_KEY_ACTION:
        c == 'u'? PLAYER_KEY_U:
        c == 'd'? PLAYER_KEY_D:
        c == 'l'? PLAYER_KEY_L:
        c == 'r'? PLAYER_KEY_R:
        c == 'f'? (!absolute && player->turn? PLAYER_KEY_L: PLAYER_KEY_R):
        c == 'b'? (!absolute && player->turn? PLAYER_KEY_R: PLAYER_KEY_L):
        -1;
    return key_i;
}

char player_get_key_c(player_t *player, int key_i, bool absolute){
    return
        key_i == PLAYER_KEY_ACTION? 'x':
        key_i == PLAYER_KEY_U? 'u':
        key_i == PLAYER_KEY_D? 'd':
        key_i == PLAYER_KEY_L? (absolute? 'l': player->turn? 'f': 'b'):
        key_i == PLAYER_KEY_R? (absolute? 'r': player->turn? 'b': 'f'):
        ' ';
}

int player_process_event(player_t *player, SDL_Event *event){

    if(player->state == NULL){
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

static int player_match_rule(player_t *player,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule, bool *rule_matched_ptr
){
    hexmap_t *map = game->map;
    vecspace_t *space = map->space;

    /* NOTE: player and/or actor may be NULL.
    We are basically reusing the rule/cond/effect structure for players
    and actors; most conds/effects naturally apply to one or the other.
    E.g. keypress stuff is for the player; "play" is for actor.
    However, actor may want to check some stuff about the player, so
    it may make use of "player-oriented" rules. */

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
            if(player == NULL){
                fprintf(stderr, "No player");
                RULE_PERROR()
                break;}

            int kstate_i = cond->u.key.kstate;
            bool *kstate =
                kstate_i == 0? player->keyinfo.isdown:
                kstate_i == 1? player->keyinfo.wasdown:
                kstate_i == 2? player->keyinfo.wentdown:
                NULL;
            if(kstate == NULL){
                fprintf(stderr, "kstate out of range: %i", kstate_i);
                RULE_PERROR()
                return 2;}

            char c = cond->u.key.c;
            int key_i = player_get_key_i(player, c, false);
            if(key_i == -1){
                fprintf(stderr, "Unrecognized key char: %c", c);
                RULE_PERROR()
                return 2;}

            rule_matched = kstate[key_i];
            if(!cond->u.key.yes)rule_matched = !rule_matched;
        }else if(cond->type == state_cond_type_coll){
            if(player == NULL){
                fprintf(stderr, "No player");
                RULE_PERROR()
                break;}

            if(player->state == NULL){
                rule_matched = false;
                break;}

            trf_t hitbox_trf;
            player_init_trf(player, &hitbox_trf, space);
            hexcollmap_t *hitbox = cond->u.coll.collmap;

            int flags = cond->u.coll.flags;
            bool all = flags & 1;
            bool yes = flags & 2;

            if(cond->u.coll.against_players){
                int n_matches = 0;
                for(int j = 0; j < game->players_len; j++){
                    player_t *player_other = game->players[j];
                    if(player == player_other)continue;
                    if(player_other->state == NULL)continue;
                    hexcollmap_t *hitbox_other = player_other->state->hitbox;
                    if(hitbox_other == NULL)continue;

                    trf_t hitbox_other_trf;
                    player_init_trf(player_other, &hitbox_other_trf, space);

                    /* The other player has a hitbox! Do the collision... */
                    bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                        hitbox_other, &hitbox_other_trf, space, yes? all: !all);
                    if(yes? collide: !collide)n_matches++;
                }
                rule_matched = n_matches > 0;
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

static int player_apply_rule(player_t *player,
    actor_t *actor, hexgame_t *game,
    state_rule_t *rule
){
    int err;
    hexmap_t *map = game->map;
    vecspace_t *space = map->space;

    /* NOTE: player and/or actor may be NULL.
    See comment on player_match_rule. */

    for(int i = 0; i < rule->effects_len; i++){
        state_effect_t *effect = rule->effects[i];
        if(DEBUG_RULES)printf("  then: %s\n", effect->type);

        #define RULE_PERROR() \
            fprintf(stderr, " (effect=%s, state=%s, stateset=%s)\n", \
                effect->type, rule->state->name, \
                rule->state->stateset->filename);

        if(effect->type == state_effect_type_print){
            if(player != NULL)printf("player %p", player);
            else printf("unknown player");
            if(actor != NULL)printf(" (actor %p)", actor);
            printf(" says: %s\n", effect->u.msg);
        }else if(effect->type == state_effect_type_move){
            if(player == NULL){
                fprintf(stderr, "No player");
                RULE_PERROR()
                break;}
            vec_t vec;
            vec_cpy(space->dims, vec, effect->u.vec);
            rot_t rot = player_get_rot(player, space);
            space->vec_flip(vec, player->turn);
            space->vec_rot(vec, rot);
            vec_add(space->dims, player->pos, vec);
        }else if(effect->type == state_effect_type_rot){
            if(player == NULL){
                fprintf(stderr, "No player");
                RULE_PERROR()
                break;}
            rot_t effect_rot = effect->u.rot;
            player->rot = rot_rot(space->rot_max,
                player->rot, effect_rot);
        }else if(effect->type == state_effect_type_turn){
            if(player == NULL){
                fprintf(stderr, "No player");
                RULE_PERROR()
                break;}
            player->turn = !player->turn;
            player->rot = rot_flip(space->rot_max, player->rot, true);
        }else if(effect->type == state_effect_type_goto){
            if(actor != NULL){
                /* HACK! TODO: Make it more explicit whether "goto" is
                referring to player or actor. */
                err = actor_set_state(actor, effect->u.goto_name);
                if(err)return err;
            }else{
                if(player == NULL){
                    fprintf(stderr, "No player");
                    RULE_PERROR()
                    break;}
                err = player_set_state(player, effect->u.goto_name);
                if(err)return err;
            }
        }else if(effect->type == state_effect_type_delay){
            if(player == NULL){
                fprintf(stderr, "No player");
                RULE_PERROR()
                break;}
            player->cooldown = effect->u.delay;
        }else if(effect->type == state_effect_type_action){
            const char *action_name = effect->u.action_name;
            if(!strcmp(action_name, "ping")){
                fprintf(stderr, "pong\n");
            }else if(
                !strcmp(action_name, "spit") ||
                !strcmp(action_name, "spit_crouch")
            ){
                if(player == NULL){
                    fprintf(stderr, "No player");
                    RULE_PERROR()
                    break;}
                bool crouch = action_name[4] == '_';
                const char *stateset_filename = "anim/spit.fus";
                ARRAY_PUSH_NEW(player_t*, game->players, new_player)
                err = player_init(new_player, map, stateset_filename,
                    crouch? "crouch_fly": "fly", -1,
                    player->respawn_pos, NULL);
                if(err)return err;
                vec_cpy(space->dims, new_player->pos, player->pos);
                new_player->rot = player->rot;
                new_player->turn = player->turn;
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
            player_recording_reset(&player->recording);
            err = player_recording_load(&player->recording,
                play_filename, game, false);
            if(err)return err;

            err = player_play_recording(player);
            if(err)return err;
        }else if(effect->type == state_effect_type_die){
            if(player == NULL){
                fprintf(stderr, "No player");
                RULE_PERROR()
                break;}
            player->dead = true;
        }else{
            fprintf(stderr, "Unrecognized state rule effect: %s\n",
                effect->type);
            return 2;
        }
        #undef RULE_PERROR
    }
    return 0;
}

int state_handle_rules(state_t *state, player_t *player,
    actor_t *actor, hexgame_t *game
){
    int err;

    for(int i = 0; i < state->rules_len; i++){
        state_rule_t *rule = state->rules[i];

        if(DEBUG_RULES){printf("player %p, actor %p, rule %i:\n",
            player, actor, i);}

        bool rule_matched;
        err = player_match_rule(player, actor, game, rule,
            &rule_matched);
        if(err)return err;

        if(rule_matched){
            err = player_apply_rule(player, actor, game, rule);
            if(err)return err;
            break;
        }
    }

    return 0;
}



/***************
 * PLAYER STEP *
 ***************/

int player_step(player_t *player, hexgame_t *game){
    int err;

    if(player->state == NULL){
        return 0;}

    hexmap_t *map = game->map;
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
        /* Handle current state's rules */
        err = state_handle_rules(player->state, player, NULL, game);
        if(err)return err;

        /* start of new frame, no keys have gone down yet. */
        for(int i = 0; i < PLAYER_KEYS; i++){
            player->keyinfo.wasdown[i] = player->keyinfo.isdown[i];
            player->keyinfo.wentdown[i] = false;}
    }

    /* Figure out current submap */
    bool out_of_bounds = true;
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
        if(vert != NULL)out_of_bounds = false;
        if(hexcollmap_elem_is_solid(vert)){
            if(player->cur_submap != submap && player->keymap >= 0){
                /* For debugging */
                fprintf(stderr, "Player %i arrived at submap: %s\n",
                    player->keymap, submap->filename);
            }
            player->cur_submap = submap;
            break;
        }
    }

    if(player->out_of_bounds != out_of_bounds && player->keymap >= 0){
        fprintf(stderr, "Player %i %s the map!\n",
            player->keymap, out_of_bounds? "left": "returned to");
    }
    player->out_of_bounds = out_of_bounds;

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
        return 0;}

    prismelrenderer_t *prend = map->prend;
    vecspace_t *space = map->space;

    rendergraph_t *rgraph = player->state->rgraph;
    if(player->palmapper){
        err = palettemapper_apply_to_rendergraph(player->palmapper,
            prend, rgraph, NULL, space, &rgraph);
        if(err)return err;
    }

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
        pal, prend,
        x0, y0, zoom,
        pos, rot, flip, frame_i, mapper);
    if(err)return err;

    return 0;
}

