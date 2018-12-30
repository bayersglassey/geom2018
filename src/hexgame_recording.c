

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

void recording_cleanup(recording_t *rec){
    free(rec->data);
    free(rec->name);
    free(rec->stateset_name);
    free(rec->state_name);
    if(rec->file != NULL)fclose(rec->file);
}

void recording_reset(recording_t *rec){
    recording_cleanup(rec);

    rec->action = 0; /* none */
    rec->data = NULL;
    rec->stateset_name = NULL;
    rec->state_name = NULL;

    vec_zero(MAX_VEC_DIMS, rec->pos0);
    rec->rot0 = 0;
    rec->turn0 = false;

    keyinfo_reset(&rec->keyinfo);

    rec->i = 0;
    rec->size = 0;
    rec->wait = 0;
    rec->name = NULL;
    rec->file = NULL;
    rec->offset = 0;
}

void recording_init(recording_t *rec, body_t *body,
    bool loop
){
    rec->body = body;
    rec->loop = loop;
}

static int recording_parse(recording_t *rec,
    fus_lexer_t *lexer, const char *filename
){
    int err;

    hexmap_t *map = rec->body->map;
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
        err = fus_lexer_get_keyinfo(lexer, &rec->keyinfo);
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

int recording_load(recording_t *rec, const char *filename,
    body_t *body, bool loop
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    recording_init(rec, body, loop);
    err = recording_parse(rec, &lexer, filename);
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



/******************
 * BODY RECORDING *
 ******************/

int body_load_recording(body_t *body, const char *filename, bool loop){
    int err;
    recording_t *rec = &body->recording;

    recording_reset(rec);
    err = recording_load(rec, filename, body, loop);
    if(err)return err;
    return 0;
}

int body_play_recording(body_t *body){
    int err;
    recording_t *rec = &body->recording;

    err = body_init_stateset(body, rec->stateset_name, rec->state_name);
    if(err)return err;

    body->recording.action = 1; /* play */
    return body_restart_recording(body, false);
}

int body_restart_recording(body_t *body, bool hard){
    int err;
    recording_t *rec = &body->recording;

    rec->i = 0;
    rec->wait = 0;

    keyinfo_copy(&body->keyinfo, &rec->keyinfo);
    body_set_state(body, rec->state_name, true);

    vec_cpy(MAX_VEC_DIMS, body->pos, rec->pos0);
    body->rot = rec->rot0;
    body->turn = rec->turn0;

    if(!hard){
        for(int i = 0; i < rec->offset; i++){
            body_step(body, body->game);
        }
    }

    return 0;
}

int body_start_recording(body_t *body, char *name){

    FILE *f = fopen(name, "w");
    if(f == NULL){
        perror("Couldn't start recording");
        return 2;}

    recording_reset(&body->recording);
    body->recording.action = 2; /* record */
    body->recording.name = name;
    body->recording.file = f;

    fprintf(f, "anim: ");
    fus_write_str(f, body->stateset.filename);
    fprintf(f, "\n");

    fprintf(f, "state: ");
    fus_write_str(f, body->state->name);
    fprintf(f, "\n");

    fprintf(f, "pos: (");
    for(int i = 0; i < hexspace.dims; i++){
        fprintf(f, " %i", body->pos[i]);
    }
    fprintf(f, ")\n");

    fprintf(f, "rot: %i\n", body->rot);
    fprintf(f, "turn: %s\n", body->turn? "yes": "no");

    fprintf(f, "keys:\n");
    for(int i = 0; i < KEYINFO_KEYS; i++){
        char key_c = body_get_key_c(body, i, true);
        fprintf(f, "    %c:", key_c);
        if(body->keyinfo.isdown[i])fprintf(f, " is");
        if(body->keyinfo.wasdown[i])fprintf(f, " was");
        if(body->keyinfo.wentdown[i])fprintf(f, " went");
        fprintf(f, "\n");
    }

    fprintf(f, "data: \"");

    return 0;
}

int body_stop_recording(body_t *body){
    int err;

    FILE *f = body->recording.file;
    if(f == NULL)return 2;

    err = body_maybe_record_wait(body);
    if(err)return err;

    fprintf(f, "\"\n");

    recording_reset(&body->recording);
    return 0;
}

int body_record(body_t *body, const char *data){
    /* CURRENTLY UNUSED!.. we write directly to file instead. */

    int data_len = strlen(data);
    int required_size = body->recording.i + data_len + 1;

    if(DEBUG_RECORDINGS){
        printf("RECORD: data=%s, len=%i, req=%i\n",
            data, data_len, required_size);}

    if(required_size >= body->recording.size){
        int new_size = required_size + 200;
        char *new_recording = realloc(body->recording.data,
            sizeof(char) * new_size);
        if(new_recording == NULL)return 1;
        new_recording[new_size - 1] = '\0';
        body->recording.data = new_recording;
        body->recording.size = new_size;
        if(DEBUG_RECORDINGS)printf("  REALLOC: new_size=%i\n", new_size);
    }
    strcpy(body->recording.data + body->recording.i, data);
    body->recording.i += data_len;

    if(DEBUG_RECORDINGS){
        printf("  OK! i=%i, data=%s\n",
            body->recording.i, body->recording.data);}

    return 0;
}

int body_maybe_record_wait(body_t *body){
    int wait = body->recording.wait;
    if(wait == 0)goto ok;

    fprintf(body->recording.file, " w%i", wait);
    if(DEBUG_RECORDINGS)printf("w%i\n", wait);

    /*
    char buffer[4];
    buffer[0] = ' ';
    buffer[1] = 'w';
    buffer[2] = '1'; // <---- ummmmm
    buffer[3] = '\0';

    int err = body_record(body, buffer);
    if(err){perror("body_record failed");}
    */

    body->recording.wait = 0;
ok:
    return 0;
}

int recording_step(recording_t *rec){
    int err;

    body_t *body = rec->body;

    if(rec->wait > 0){
        rec->wait--;
        if(rec->wait > 0)return 0;}

    char *data = rec->data;
    int i = rec->i;
    char c;

    bool loop = rec->loop;

    while(1){
        while(data[i] == ' ')i++;

        c = data[i];
        if(c == '+' || c == '-'){
            bool keydown = c == '+';
            char key_c = data[i+1]; i += 2;
            if(DEBUG_RECORDINGS)printf("%c%c\n", c, key_c);
            int key_i = body_get_key_i(body, key_c, false);
            if(keydown)body_keydown(body, key_i);
            else body_keyup(body, key_i);
        }else if(c == 'w'){
            i++; int wait = atoi(data + i);
            if(DEBUG_RECORDINGS)printf("w%i\n", wait);
            while(isdigit(data[i]))i++;
            rec->wait = wait;
            break;
        }else if(c == '\0'){
            if(loop){
                /* loop! */
                err = body_restart_recording(body, true);
                if(err)return err;
                i = 0;
            }else{
                break;
            }
        }else{
            fprintf(stderr, "Unrecognized action: %c\n", c);
            fprintf(stderr, "  ...in position %i of recording: %s\n",
                i, rec->data);
            return 2;
        }
    }

    rec->i = i;

    if(!loop && data[i] == '\0'){
        recording_reset(rec);
    }

    return 0;
}

