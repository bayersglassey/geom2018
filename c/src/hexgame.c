

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


#define DEBUG_RULES false
#define DEBUG_RECORDINGS false

#define MAX_FRAME_I 554400
    /* Largest highly composite number smaller than 2^16 */



static const char *get_record_filename(int n){
    /* NOT REENTRANT, FORGIVE MEEE :( */
    static char record_filename[200] = "data/rec000.fus";
    static const int zeros_pos = 8;
    static const int n_zeros = 3;
    for(int i = 0; i < n_zeros; i++){
        int rem = n % 10;
        n = n / 10;
        record_filename[zeros_pos + n_zeros - 1 - i] = '0' + rem;
    }
    return record_filename;
}

static const char *get_last_or_next_record_filename(bool next){
    const char *record_filename;
    int n = 0;
    while(1){
        record_filename = get_record_filename(n);
        FILE *f = fopen(record_filename, "r");
        if(f == NULL)break;
        n++;
    }
    if(!next){
        if(n == 0)return NULL;
        record_filename = get_record_filename(n-1);
    }
    return record_filename;
}

static const char *get_last_record_filename(){
    return get_last_or_next_record_filename(false);
}

static const char *get_next_record_filename(){
    return get_last_or_next_record_filename(true);
}


/**********
 * PLAYER *
 **********/

static void player_reset_recording(player_t *player){
    free(player->recording);
    free(player->recording_name);
    if(player->recording_file != NULL)fclose(player->recording_file);

    player->recording_action = 0; /* none */
    player->recording = NULL;
    player->recording_i = 0;
    player->recording_size = 0;
    player->recording_wait = 0;
    player->recording_name = NULL;
    player->recording_file = NULL;
}

void player_cleanup(player_t *player){
    stateset_cleanup(&player->stateset);
    player_reset_recording(player);
}

int player_init(player_t *player, prismelrenderer_t *prend,
    char *stateset_filename, const char *state_name, int keymap,
    vec_t respawn_pos
){
    int err;

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

    err = stateset_load(&player->stateset, stateset_filename,
        prend, &hexspace);
    if(err)return err;

    if(state_name != NULL){
        player->state = stateset_get_state(&player->stateset, state_name);
        if(player->state == NULL){
            fprintf(stderr, "Couldn't init player: "
                "couldn't find state %s in stateset %s\n",
                stateset_filename, state_name);
            return 2;}
    }else{
        player->state = player->stateset.states[0];
    }
    player->frame_i = 0;
    player->cooldown = 0;
    vec_cpy(hexspace.dims, player->respawn_pos, respawn_pos);
    vec_cpy(hexspace.dims, player->pos, respawn_pos);

    player->recording_action = 0;

    return 0;
}

static int player_maybe_record_wait(player_t *player){
    int wait = player->recording_wait;
    if(wait == 0)goto ok;

    fprintf(player->recording_file, " w%i", wait);
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

    player->recording_wait = 0;
ok:
    return 0;
}

void player_keydown(player_t *player, int key_i){
    if(key_i < 0 || key_i >= PLAYER_KEYS)return;
    player->key_isdown[key_i] = true;
    player->key_wasdown[key_i] = true;
    player->key_wentdown[key_i] = true;

    if(player->recording_action == 2){
        /* record */
        player_maybe_record_wait(player);

        char c = player_get_key_c(player, key_i, true);
        fprintf(player->recording_file, "+%c", c);
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
    player->key_isdown[key_i] = false;

    if(player->recording_action == 2){
        /* record */
        player_maybe_record_wait(player);

        char c = player_get_key_c(player, key_i, true);
        fprintf(player->recording_file, "-%c", c);
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

int player_get_key_i(player_t *player, char c){
    return
        c == 'u'? PLAYER_KEY_U:
        c == 'd'? PLAYER_KEY_D:
        c == 'l'? PLAYER_KEY_L:
        c == 'r'? PLAYER_KEY_R:
        c == 'f'? (player->turn? PLAYER_KEY_L: PLAYER_KEY_R):
        c == 'b'? (player->turn? PLAYER_KEY_R: PLAYER_KEY_L):
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
    const static vecspace_t *space = &hexspace;

    bool rule_matched = true;
    for(int i = 0; i < rule->conds_len; i++){
        state_cond_t *cond = rule->conds[i];
        if(DEBUG_RULES)printf("  if: %s\n", cond->type);
        if(cond->type == state_cond_type_false){
            rule_matched = false;
        }else if(cond->type == state_cond_type_key){

            int kstate_i = cond->u.key.kstate;
            bool *kstate =
                kstate_i == 0? player->key_isdown:
                kstate_i == 1? player->key_wasdown:
                kstate_i == 2? player->key_wentdown:
                NULL;
            if(kstate == NULL){
                fprintf(stderr, "kstate out of range: %i", kstate_i);
                return 2;}

            char c = cond->u.key.c;
            int key_i = player_get_key_i(player, c);
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
            fprintf(stderr, "Unrecognized state rule condition: %s\n",
                cond->type);
            return 2;
        }
        if(!rule_matched)break;
    }

    if(DEBUG_RULES && !rule_matched)printf("    NO MATCH\n");

    *rule_matched_ptr = rule_matched;
    return 0;
}

static int player_apply_rule(player_t *player, state_rule_t *rule){
    const static vecspace_t *space = &hexspace;
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
            state_t *state = stateset_get_state(&player->stateset,
                effect->u.goto_name);
            if(state == NULL){
                fprintf(stderr, "Unrecognized player state: %s\n",
                    effect->u.goto_name);
                return 2;
            }
            player->state = state;
            player->frame_i = 0;
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

    int rec_action = player->recording_action;
    if(rec_action == 1){
        /* play */
        if(player->recording != NULL){
            err = player_recording_step(player);
            if(err)return err;
        }
    }else if(rec_action == 2){
        /* record */
        player->recording_wait++;
    }

    if(rec_action != 0 && DEBUG_RECORDINGS){
        printf("KEYS: ");
        #define DEBUG_PRINT_KEYS(keys) { \
            printf("["); \
            for(int i = 0; i < PLAYER_KEYS; i++)printf("%i", keys[i]); \
            printf("]"); \
        }
        DEBUG_PRINT_KEYS(player->key_isdown)
        DEBUG_PRINT_KEYS(player->key_wasdown)
        DEBUG_PRINT_KEYS(player->key_wentdown)
        #undef DEBUG_PRINT_KEYS
        printf("\n");
    }

    player->frame_i++;
    if(player->frame_i == MAX_FRAME_I)player->frame_i = 0;

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
                err = player_apply_rule(player, rule);
                if(err)return err;
                break;
            }
        }

        /* start of new frame, no keys have gone down yet. */
        for(int i = 0; i < PLAYER_KEYS; i++){
            player->key_wasdown[i] = player->key_isdown[i];
            player->key_wentdown[i] = false;}
    }
    return 0;
}

int player_play_recording(player_t *player, char *data, char *filename){
    player_reset_recording(player);
    player->recording_action = 1; /* play */
    player->recording = data;
    player->recording_name = filename;
    return 0;
}

int player_start_recording(player_t *player, char *name){
    player_reset_recording(player);

    FILE *f = fopen(name, "w");
    if(f == NULL){
        perror("Couldn't start recording");
        return 2;}

    player_reset_recording(player);
    player->recording_action = 2; /* record */
    player->recording_name = name;
    player->recording_file = f;

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
    fprintf(f, "data: \"");

    return 0;
}

int player_stop_recording(player_t *player){
    FILE *f = player->recording_file;
    if(f == NULL)return 2;

    fprintf(f, "\"\n");

    player_reset_recording(player);

    return 0;
}

int player_record(player_t *player, const char *data){
    /* CURRENTLY UNUSED!.. we write directly to file instead. */

    int data_len = strlen(data);
    int required_size = player->recording_i + data_len + 1;

    if(DEBUG_RECORDINGS){
        printf("RECORD: data=%s, len=%i, req=%i\n",
            data, data_len, required_size);}

    if(required_size >= player->recording_size){
        int new_size = required_size + 200;
        char *new_recording = realloc(player->recording,
            sizeof(char) * new_size);
        if(new_recording == NULL)return 1;
        new_recording[new_size - 1] = '\0';
        player->recording = new_recording;
        player->recording_size = new_size;
        if(DEBUG_RECORDINGS)printf("  REALLOC: new_size=%i\n", new_size);
    }
    strcpy(player->recording + player->recording_i, data);
    player->recording_i += data_len;

    if(DEBUG_RECORDINGS){
        printf("  OK! i=%i, data=%s\n",
            player->recording_i, player->recording);}

    return 0;
}

int player_recording_step(player_t *player){
    if(player->recording_wait > 0){
        player->recording_wait--;
        if(player->recording_wait > 0)return 0;}

    char *rec = player->recording;
    int i = player->recording_i;
    char c;

    while(rec[i] != '\0'){
        while(rec[i] == ' ')i++;

        c = rec[i];
        if(c == '+' || c == '-'){
            bool keydown = c == '+';
            char key_c = rec[i+1]; i += 2;
            if(DEBUG_RECORDINGS)printf("%c%c\n", c, key_c);
            int key_i = player_get_key_i(player, key_c);
            if(keydown)player_keydown(player, key_i);
            else player_keyup(player, key_i);
        }else if(c == 'w'){
            i++; int wait = atoi(rec + i);
            if(DEBUG_RECORDINGS)printf("w%i\n", wait);
            while(isdigit(rec[i]))i++;
            player->recording_wait = wait;
            break;
        }else{
            fprintf(stderr, "Unrecognized action: %c\n", c);
            fprintf(stderr, "  ...in position %i of recording: %s\n",
                i, player->recording);
            return 2;
        }
    }

    if(rec[i] == '\0'){
        player_reset_recording(player);
    }else{
        player->recording_i = i;
    }

    return 0;
}


/***********
 * HEXGAME *
 ***********/


void hexgame_cleanup(hexgame_t *game){
    ARRAY_FREE(player_t, *game, players, player_cleanup)
}

int hexgame_init(hexgame_t *game, hexmap_t *map, char *respawn_filename){
    game->frame_i = 0;
    game->zoomout = false;
    game->follow = false;
    game->map = map;
    game->respawn_filename = respawn_filename;
    vec_zero(map->space->dims, game->camera_pos);
    game->camera_rot = 0;
    game->cur_submap = NULL;
    ARRAY_INIT(*game, players)
    return 0;
}

static int hexgame_parse_player_recording(hexgame_t *game, fus_lexer_t *lexer,
    const char *filename, int keymap
){
    int err;

    char *stateset_filename;
    err = fus_lexer_expect(lexer, "anim");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_str(lexer, &stateset_filename);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;

    char *state_name;
    err = fus_lexer_expect(lexer, "state");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_str(lexer, &state_name);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;

    ARRAY_PUSH_NEW(player_t, *game, players, player)
    err = player_init(player, game->map->prend, stateset_filename,
        state_name, keymap, game->map->spawn);
    if(err)return err;
    free(state_name);

    err = fus_lexer_expect(lexer, "pos");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_vec(lexer, game->map->space, player->pos);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;

    rot_t rot;
    err = fus_lexer_expect(lexer, "rot");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_int(lexer, &rot);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;
    player->rot = rot;

    bool turn;
    err = fus_lexer_expect(lexer, "turn");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_next(lexer);
    if(err)return err;
    if(fus_lexer_got(lexer, "yes")){
        turn = true;
    }else if(fus_lexer_got(lexer, "no")){
        turn = false;
    }else{
        return fus_lexer_unexpected(lexer, "yes or no");
    }
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;
    player->turn = turn;

    char *data;
    err = fus_lexer_expect(lexer, "data");
    if(err)return err;
    err = fus_lexer_expect(lexer, "(");
    if(err)return err;
    err = fus_lexer_expect_str(lexer, &data);
    if(err)return err;
    err = fus_lexer_expect(lexer, ")");
    if(err)return err;

    return player_play_recording(player, data, strdup(filename));
}

int hexgame_load_player_recording(hexgame_t *game, const char *filename,
    int keymap
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexgame_parse_player_recording(game, &lexer, filename, keymap);
    if(err)return err;

    free(text);
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

    for(int i = 0; i < PLAYER_KEYS; i++){
        player->key_isdown[i] = false;
        player->key_wasdown[i] = false;
        player->key_wentdown[i] = false;
    }

    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    int err;

    if(event->type == SDL_KEYDOWN){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = true;
        }else if(event->key.keysym.sym == SDLK_F7){
            game->follow = true;
        }else if(event->key.keysym.sym == SDLK_F9){
            /* save recording */
            if(game->players_len >= 1){
                player_t *player = game->players[0];
                if(player->recording_action != 2){
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
            const char *record_filename = get_last_record_filename();
            if(record_filename == NULL){
                fprintf(stderr, "Couldn't find file of last recording. "
                    "Maybe you need to record your first one with "
                    "'R' then 'F9'?\n");
            }else{
                err = hexgame_load_player_recording(game,
                    record_filename, -1);
                if(err)return err;
            }
        }else if(event->key.keysym.sym == SDLK_r){
            /* start recording */
            if(game->players_len >= 1){
                const char *record_filename = get_next_record_filename();
                err = player_start_recording(game->players[0],
                    strdup(record_filename));
                if(err)return err;
            }
        }else if(!event->key.repeat){
            bool shift = event->key.keysym.mod & KMOD_SHIFT;
            if(event->key.keysym.sym == SDLK_1
                && game->players_len >= 1){
                    hexgame_reset_player(game, game->players[0], shift);}
            if(event->key.keysym.sym == SDLK_2
                && game->players_len >= 2){
                    hexgame_reset_player(game, game->players[1], shift);}
        }
    }else if(event->type == SDL_KEYUP){
        if(event->key.keysym.sym == SDLK_F6){
            game->zoomout = false;
        }else if(event->key.keysym.sym == SDLK_F7){
            game->follow = false;
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

    /* Figure out current submap */
    if(game->players_len >= 1){
        player_t *player = game->players[0];

        bool collide = false;
        for(int i = 0; i < map->submaps_len; i++){
            hexmap_submap_t *submap = map->submaps[i];
            hexcollmap_t *collmap = &submap->collmap;

            trf_t index = {0};
            hexspace_set(index.add,
                 player->pos[0] - submap->pos[0],
                -player->pos[1] + submap->pos[1]);

            /* savepoints are currently this HACK */
            hexcollmap_elem_t *face =
                hexcollmap_get_face(collmap, &index);
            if(face != NULL && face->tile_c == 'S'){
                if(!vec_eq(space->dims, player->respawn_pos, player->pos)){
                    vec_cpy(space->dims, player->respawn_pos, player->pos);
                    if(game->respawn_filename != NULL){
                        FILE *f = fopen(game->respawn_filename, "w");
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
                if(submap != game->cur_submap){
                    /* TODO: Smoothly transition between
                    old & new palettes */
                    err = palette_reset(&submap->palette);
                    if(err)return err;
                }
                game->cur_submap = submap;
                break;
            }
        }
    }

    /* Animate palette */
    if(game->cur_submap != NULL){
        err = palette_step(&game->cur_submap->palette);
        if(err)return err;
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

    /* Do 1 gameplay step for each player */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game->map);
        if(err)return err;
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

    vecspace_t *space = &hexspace;

    hexmap_t *map = game->map;

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
            pal, game->map->prend,
            x0, y0, zoom,
            pos, rot, flip, frame_i, mapper);
        if(err)return err;
    }

    return 0;
}

