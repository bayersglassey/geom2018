

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



/*************
 * RECORDING *
 *************/

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

void player_recording_init(player_recording_t *rec, hexgame_t *game,
    bool loop
){
    rec->game = game;
    rec->loop = loop;
}

static int player_recording_parse(player_recording_t *rec,
    fus_lexer_t *lexer, const char *filename
){
    int err;

    hexmap_t *map = rec->game->map;
    vecspace_t *space = map->space;

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
    hexgame_t *game, bool loop
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    player_recording_init(rec, game, loop);
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

const char *get_last_recording_filename(){
    return get_last_or_next_recording_filename(false);
}

const char *get_next_recording_filename(){
    return get_last_or_next_recording_filename(true);
}



/********************
 * PLAYER RECORDING *
 ********************/

int player_play_recording(player_t *player){
    int err;
    player_recording_t *rec = &player->recording;

    err = player_init_stateset(player, rec->stateset_name, rec->state_name,
        rec->game->map);
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
            player_step(player, rec->game);
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

int player_maybe_record_wait(player_t *player){
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

int player_recording_step(player_t *player){
    int err;

    if(player->recording.wait > 0){
        player->recording.wait--;
        if(player->recording.wait > 0)return 0;}

    char *rec = player->recording.data;
    int i = player->recording.i;
    char c;

    bool loop = player->recording.loop;

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

