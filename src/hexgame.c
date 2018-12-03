

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef GEOM_HEXGAME_DEBUG_MALLOC
    #include <malloc.h>
#endif

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
#include "prismelrenderer.h"
#include "array.h"
#include "util.h"
#include "write.h"


#define DEBUG_RULES false
#define DEBUG_RECORDINGS false

#define MAX_FRAME_I 554400
    /* Largest highly composite number smaller than 2^16 */



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


/********************
 * PLAYER RECORDING *
 ********************/

void player_recording_cleanup(player_recording_t *rec){
    free(rec->data);
    free(rec->name);
    free(rec->stateset_name);
    free(rec->state_name);
    if(rec->file != NULL)fclose(rec->file);
}

void player_recording_reset(player_recording_t *rec){
    player_recording_cleanup(rec);

    rec->action = 0; /* none */
    rec->data = NULL;
    rec->stateset_name = NULL;
    rec->state_name = NULL;

    vec_zero(MAX_VEC_DIMS, rec->pos0);
    rec->rot0 = 0;
    rec->turn0 = false;

    player_keyinfo_reset(&rec->keyinfo);

    rec->i = 0;
    rec->size = 0;
    rec->wait = 0;
    rec->name = NULL;
    rec->file = NULL;
    rec->offset = 0;
}

void player_recording_init(player_recording_t *rec, hexmap_t *map){
    rec->map = map;
}

static int player_recording_parse(player_recording_t *rec,
    fus_lexer_t *lexer, const char *filename
){
    int err;

    vecspace_t *space = rec->map->space;

    err = fus_lexer_get(lexer, "anim");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &rec->stateset_name);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    err = fus_lexer_get(lexer, "state");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &rec->state_name);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    err = fus_lexer_get(lexer, "pos");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_vec(lexer, space, rec->pos0);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    err = fus_lexer_get(lexer, "rot");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_int(lexer, &rec->rot0);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    err = fus_lexer_get(lexer, "turn");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    if(fus_lexer_got(lexer, "yes")){
        rec->turn0 = true;
    }else if(fus_lexer_got(lexer, "no")){
        rec->turn0 = false;
    }else{
        return fus_lexer_unexpected(lexer, "yes or no");
    }
    err = fus_lexer_next(lexer);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    if(fus_lexer_got(lexer, "offset")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_int(lexer, &rec->offset);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "keys")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get_player_keyinfo(lexer, &rec->keyinfo);
        if(err)return err;
    }

    err = fus_lexer_get(lexer, "data");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &rec->data);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    return 0;
}

int player_recording_load(player_recording_t *rec, const char *filename,
    hexmap_t *map
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    player_recording_init(rec, map);
    err = player_recording_parse(rec, &lexer, filename);
    if(err)return err;

    free(text);
    return 0;
}

static const char *get_recording_filename(int n){
    /* NOT REENTRANT, FORGIVE MEEE :( */
    static char recording_filename[200] = "data/rec000.fus";
    static const int zeros_pos = 8;
    static const int n_zeros = 3;
    for(int i = 0; i < n_zeros; i++){
        int rem = n % 10;
        n = n / 10;
        recording_filename[zeros_pos + n_zeros - 1 - i] = '0' + rem;
    }
    return recording_filename;
}

static const char *get_last_or_next_recording_filename(bool next){
    const char *recording_filename;
    int n = 0;
    while(1){
        recording_filename = get_recording_filename(n);
        FILE *f = fopen(recording_filename, "r");
        if(f == NULL)break;
        n++;
    }
    if(!next){
        if(n == 0)return NULL;
        recording_filename = get_recording_filename(n-1);
    }
    return recording_filename;
}

static const char *get_last_recording_filename(){
    return get_last_or_next_recording_filename(false);
}

static const char *get_next_recording_filename(){
    return get_last_or_next_recording_filename(true);
}



/**********
 * PLAYER *
 **********/

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

static int player_maybe_record_wait(player_t *player){
    int wait = player->recording.wait;
    if(wait == 0)goto ok;

    fprintf(player->recording.file, " w%i", wait);
    if(DEBUG_RECORDINGS)printf("w%i\n", wait);

    /*
    char buffer[4];
    buffer[0] = ' ';
    buffer[1] = 'w';
    buffer[2] = '1'; // <---- ummmmm
    buffer[3] = '\0';

    int err = player_record(player, buffer);
    if(err){perror("player_record failed");}
    */

    player->recording.wait = 0;
ok:
    return 0;
}

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

rot_t player_get_rot(player_t *player, const vecspace_t *space){
    rot_t rot = player->rot;
    if(player->turn){
        rot = rot_contain(space->rot_max,
            space->rot_max/2 - rot);}
    return rot;
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
            vec_cpy(space->dims, trf.add, player->pos);
            trf.rot = player_get_rot(player, space);
            trf.flip = player->turn;

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



int player_play_recording(player_t *player){
    int err;
    player_recording_t *rec = &player->recording;

    err = player_init_stateset(player, rec->stateset_name, rec->state_name,
        rec->map);
    if(err)return err;

    player->recording.action = 1; /* play */
    return player_restart_recording(player, false);
}

int player_restart_recording(player_t *player, bool hard){
    int err;
    player_recording_t *rec = &player->recording;

    rec->i = 0;
    rec->wait = 0;

    player_keyinfo_copy(&player->keyinfo, &rec->keyinfo);
    player_set_state(player, rec->state_name);

    vec_cpy(MAX_VEC_DIMS, player->pos, rec->pos0);
    player->rot = rec->rot0;
    player->turn = rec->turn0;

    if(!hard){
        for(int i = 0; i < rec->offset; i++){
            player_step(player, rec->map);
        }
    }

    return 0;
}

int player_start_recording(player_t *player, char *name){

    FILE *f = fopen(name, "w");
    if(f == NULL){
        perror("Couldn't start recording");
        return 2;}

    player_recording_reset(&player->recording);
    player->recording.action = 2; /* record */
    player->recording.name = name;
    player->recording.file = f;

    fprintf(f, "anim: ");
    fus_write_str(f, player->stateset.filename);
    fprintf(f, "\n");

    fprintf(f, "state: ");
    fus_write_str(f, player->state->name);
    fprintf(f, "\n");

    fprintf(f, "pos: (");
    for(int i = 0; i < hexspace.dims; i++){
        fprintf(f, " %i", player->pos[i]);
    }
    fprintf(f, ")\n");

    fprintf(f, "rot: %i\n", player->rot);
    fprintf(f, "turn: %s\n", player->turn? "yes": "no");

    fprintf(f, "keys:\n");
    for(int i = 0; i < PLAYER_KEYS; i++){
        char key_c = player_get_key_c(player, i, true);
        fprintf(f, "    %c:", key_c);
        if(player->keyinfo.isdown[i])fprintf(f, " is");
        if(player->keyinfo.wasdown[i])fprintf(f, " was");
        if(player->keyinfo.wentdown[i])fprintf(f, " went");
        fprintf(f, "\n");
    }

    fprintf(f, "data: \"");

    return 0;
}

int player_stop_recording(player_t *player){
    int err;

    FILE *f = player->recording.file;
    if(f == NULL)return 2;

    err = player_maybe_record_wait(player);
    if(err)return err;

    fprintf(f, "\"\n");

    player_recording_reset(&player->recording);
    return 0;
}

int player_record(player_t *player, const char *data){
    /* CURRENTLY UNUSED!.. we write directly to file instead. */

    int data_len = strlen(data);
    int required_size = player->recording.i + data_len + 1;

    if(DEBUG_RECORDINGS){
        printf("RECORD: data=%s, len=%i, req=%i\n",
            data, data_len, required_size);}

    if(required_size >= player->recording.size){
        int new_size = required_size + 200;
        char *new_recording = realloc(player->recording.data,
            sizeof(char) * new_size);
        if(new_recording == NULL)return 1;
        new_recording[new_size - 1] = '\0';
        player->recording.data = new_recording;
        player->recording.size = new_size;
        if(DEBUG_RECORDINGS)printf("  REALLOC: new_size=%i\n", new_size);
    }
    strcpy(player->recording.data + player->recording.i, data);
    player->recording.i += data_len;

    if(DEBUG_RECORDINGS){
        printf("  OK! i=%i, data=%s\n",
            player->recording.i, player->recording.data);}

    return 0;
}

int player_recording_step(player_t *player){
    int err;

    if(player->recording.wait > 0){
        player->recording.wait--;
        if(player->recording.wait > 0)return 0;}

    char *rec = player->recording.data;
    int i = player->recording.i;
    char c;

    bool loop = true;

    while(1){
        while(rec[i] == ' ')i++;

        c = rec[i];
        if(c == '+' || c == '-'){
            bool keydown = c == '+';
            char key_c = rec[i+1]; i += 2;
            if(DEBUG_RECORDINGS)printf("%c%c\n", c, key_c);
            int key_i = player_get_key_i(player, key_c, false);
            if(keydown)player_keydown(player, key_i);
            else player_keyup(player, key_i);
        }else if(c == 'w'){
            i++; int wait = atoi(rec + i);
            if(DEBUG_RECORDINGS)printf("w%i\n", wait);
            while(isdigit(rec[i]))i++;
            player->recording.wait = wait;
            break;
        }else if(c == '\0'){
            if(loop){
                /* loop! */
                err = player_restart_recording(player, true);
                if(err)return err;
                i = 0;
            }else{
                break;
            }
        }else{
            fprintf(stderr, "Unrecognized action: %c\n", c);
            fprintf(stderr, "  ...in position %i of recording: %s\n",
                i, player->recording.data);
            return 2;
        }
    }

    player->recording.i = i;

    if(!loop && rec[i] == '\0'){
        player_recording_reset(&player->recording);
    }

    return 0;
}


/***********
 * HEXGAME *
 ***********/


void hexgame_cleanup(hexgame_t *game){
    ARRAY_FREE_PTR(player_t*, game->players, player_cleanup)
}

int hexgame_init(hexgame_t *game, hexmap_t *map){
    game->frame_i = 0;
    game->zoomout = false;
    game->follow = false;
    game->map = map;
    vec_zero(map->space->dims, game->camera_pos);
    game->camera_rot = 0;
    game->cur_submap = NULL;
    ARRAY_INIT(game->players)
    return 0;
}

int hexgame_reset_player(hexgame_t *game, player_t *player, bool hard){
    vec_cpy(game->map->space->dims, player->pos,
        hard? game->map->spawn: player->respawn_pos);
    player->rot = 0;
    player->turn = false;
    player->state = player->stateset.states[0];
    player->frame_i = 0;
    player->cooldown = 0;

    player_keyinfo_reset(&player->keyinfo);

    return 0;
}

int hexgame_reset_player_by_keymap(hexgame_t *game, int keymap, bool hard){
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->keymap == keymap){
            return hexgame_reset_player(game, player, hard);
        }
    }

    /* Player not being found with given keymap is fine. */
    return 0;
}

int hexgame_load_player_recording(hexgame_t *game, const char *filename,
    int keymap
){
    int err;

    ARRAY_PUSH_NEW(player_t*, game->players, player)
    err = player_init(player, game->map, NULL, NULL,
        -1, game->map->spawn, NULL);
    if(err)return err;

    err = player_recording_load(&player->recording, filename,
        game->map);
    if(err)return err;

    err = player_play_recording(player);
    if(err)return err;

    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    int err;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = true;
        }else if(event->key.keysym.sym == SDLK_F7){
            game->follow = !game->follow;
        }else if(event->key.keysym.sym == SDLK_F9){
            /* save recording */
            if(game->players_len >= 1){
                player_t *player = game->players[0];
                if(player->recording.action != 2){
                    fprintf(stderr,
                        "Can't stop recording without starting first! "
                        "(Try pressing 'R' before 'F9')\n");
                }else{
                    err = player_stop_recording(player);
                    if(err)return err;
                }
            }
        }else if(event->key.keysym.sym == SDLK_F10){
            /* load recording */
            const char *recording_filename = get_last_recording_filename();
            if(recording_filename == NULL){
                fprintf(stderr, "Couldn't find file of last recording. "
                    "Maybe you need to record your first one with "
                    "'R' then 'F9'?\n");
            }else{
                err = hexgame_load_player_recording(game, recording_filename, -1);
                if(err)return err;
            }
#ifdef GEOM_HEXGAME_DEBUG_MALLOC
        }else if(event->key.keysym.sym == SDLK_F11){
            malloc_stats();
#endif
        }else if(event->key.keysym.sym == SDLK_r){
            /* start recording */
            if(game->players_len >= 1){
                const char *recording_filename = get_next_recording_filename();
                err = player_start_recording(game->players[0],
                    strdup(recording_filename));
                if(err)return err;
            }
        }else if(!event->key.repeat){
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            if(event->key.keysym.sym == SDLK_1){
                hexgame_reset_player_by_keymap(game, 0, shift);}
            if(event->key.keysym.sym == SDLK_2){
                hexgame_reset_player_by_keymap(game, 1, shift);}
        }
    }else if(event->type == SDL_KEYUP){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = false;
        }
    }

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_process_event(player, event);
        if(err)return err;
    }

    return 0;
}

int hexgame_step(hexgame_t *game){
    int err;

    game->frame_i++;
    if(game->frame_i == MAX_FRAME_I)game->frame_i = 0;

    hexmap_t *map = game->map;
    vecspace_t *space = map->space;

    /* Animate palette */
    if(game->cur_submap != NULL){
        err = palette_step(&game->cur_submap->palette);
        if(err)return err;
    }

    /* Do 1 gameplay step for each player */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game->map);
        if(err)return err;
    }

    /* Figure out game's current submap */
    if(game->players_len >= 1){
        player_t *player = game->players[0];
        if(game->cur_submap != player->cur_submap){
            game->cur_submap = player->cur_submap;

            /* TODO: Smoothly transition between
            old & new palettes */
            err = palette_reset(&game->cur_submap->palette);
            if(err)return err;
        }
    }

    /* Set camera */
    int camera_type = -1;
    if(game->follow)camera_type = 1;
    else if(game->cur_submap != NULL){
        camera_type = game->cur_submap->camera_type;}
    if(camera_type == 0){
        vec_cpy(space->dims, game->camera_pos,
            game->cur_submap->camera_pos);
        game->camera_rot = 0;
    }else if(camera_type == 1){
        if(game->players_len >= 1){
            player_t *player = game->players[0];
            vec_cpy(space->dims, game->camera_pos,
                player->pos);
            game->camera_rot = player->rot;
        }
    }

    return 0;
}

int hexgame_render(hexgame_t *game,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom
){
    int err;

    if(game->cur_submap != NULL){
        err = palette_update_sdl_palette(&game->cur_submap->palette, pal);
        if(err)return err;
    }

    hexmap_t *map = game->map;
    vecspace_t *space = map->space;

    vec_t camera_renderpos;
    vec4_vec_from_hexspace(camera_renderpos, game->camera_pos);

    prismelmapper_t *mapper = NULL;

    hexmap_submap_t *submap = game->cur_submap;
    if(submap != NULL){
        if(!game->zoomout)mapper = submap->mapper;
    }

    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        rendergraph_t *rgraph = submap->rgraph_map;

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(submap != game->cur_submap)continue;
#endif

        vec_t pos;
        vec4_vec_from_hexspace(pos, submap->pos);
        vec_sub(rgraph->space->dims, pos, camera_renderpos);
        vec_mul(rgraph->space, pos, game->map->unit);

        rot_t rot = vec4_rot_from_hexspace(0);
        //rot_t rot = vec4_rot_from_hexspace(
        //    rot_inv(space->rot_max, game->camera_rot));
        flip_t flip = false;
        int frame_i = game->frame_i;

        err = rendergraph_render(rgraph, renderer, surface,
            pal, game->map->prend,
            x0, y0, zoom,
            pos, rot, flip, frame_i, mapper);
        if(err)return err;
    }

    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(player->cur_submap != game->cur_submap)continue;
#endif

        err = player_render(player,
            renderer, surface,
            pal, x0, y0, zoom,
            map, camera_renderpos, mapper);
        if(err)return err;
    }

    return 0;
}

