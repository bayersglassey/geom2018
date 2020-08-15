

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
#include "prismelrenderer.h"
#include "array.h"
#include "lexer.h"
#include "lexer_macros.h"
#include "util.h"
#include "write.h"


const char *recording_action_msg(int action){
    switch(action){
        case 0: return "None";
        case 1: return "Playing";
        case 2: return "Recording";
        default: return "Unknown";
    }
}


/******************
 * RECORDING_NODE *
 ******************/

void recording_node_init_key_c(recording_node_t *node, bool keydown, int key_c){
    node->type = keydown? RECORDING_NODE_TYPE_KEYDOWN: RECORDING_NODE_TYPE_KEYUP;
    node->u.key_c = key_c;
}

void recording_node_init_wait(recording_node_t *node, int wait){
    node->type = RECORDING_NODE_TYPE_WAIT;
    node->u.wait = wait;
}


/*************
 * RECORDING *
 *************/

void recording_cleanup(recording_t *rec){
    free(rec->name);
    free(rec->stateset_name);
    free(rec->state_name);
    if(rec->file != NULL)fclose(rec->file);
    ARRAY_FREE(struct recording_node, rec->nodes, (void))
}

void recording_reset(recording_t *rec){
    recording_cleanup(rec);

    rec->action = 0; /* none */
    rec->stateset_name = NULL;
    rec->state_name = NULL;
    rec->reacts = false;

    vec_zero(rec->pos0);
    rec->rot0 = 0;
    rec->turn0 = false;

    keyinfo_reset(&rec->keyinfo);

    ARRAY_INIT(rec->nodes)

    rec->frame_i = 0;
    rec->node_i = 0;
    rec->wait = 0;
    rec->name = NULL;
    rec->file = NULL;
    rec->offset = 0;
}

void recording_init(recording_t *rec, body_t *body,
    bool loop
){
    /* This should probably call recording_reset... but it doesn't.
    Should we fix that, or would it break something?.. */
    rec->body = body;
    rec->loop = loop;
    rec->resets_position = true;
}

static int recording_parse_nodes(recording_t *rec, const char *data){
    int err;
    int i = 0;
    while(1){
        while(data[i] == ' ')i++;

        char c = data[i];
        if(c == '+' || c == '-'){
            bool keydown = c == '+'; /* Otherwise, keyup */
            char key_c = data[i+1]; i += 2;
            ARRAY_PUSH_UNINITIALIZED(recording_node_t, rec->nodes)
            recording_node_init_key_c(&rec->nodes[rec->nodes_len - 1], keydown, key_c);
        }else if(c == 'w'){
            i++; int wait = atoi(data + i);
            while(isdigit(data[i]))i++;
            ARRAY_PUSH_UNINITIALIZED(recording_node_t, rec->nodes)
            recording_node_init_wait(&rec->nodes[rec->nodes_len - 1], wait);
        }else if(c == '\0'){
            break;
        }else{
            fprintf(stderr, "Unrecognized action: %c\n", c);
            fprintf(stderr, "  ...in position %i of recording: %s\n",
                i, data);
            return 2;
        }
    }
    return 0;
}

static int recording_parse(recording_t *rec,
    fus_lexer_t *lexer, const char *filename
){
    int err;

    hexmap_t *map = rec->body->map;
    vecspace_t *space = map->space;

    GET_ATTR_YESNO("reacts", rec->reacts, true)
    GET_ATTR_YESNO("loop", rec->loop, true)
    GET_ATTR_YESNO("resets_position", rec->resets_position, true)
    GET_ATTR_STR("anim", rec->stateset_name, false)
    GET_ATTR_STR("state", rec->state_name, false)

    GET("pos")
    GET("(")
    err = fus_lexer_get_vec(lexer, space, rec->pos0);
    if(err)return err;
    GET(")")

    GET_ATTR_INT("rot", rec->rot0, false)
    GET_ATTR_YESNO("turn", rec->turn0, false)
    GET_ATTR_INT("offset", rec->offset, true)

    if(GOT("keys")){
        NEXT
        err = fus_lexer_get_keyinfo(lexer, &rec->keyinfo);
        if(err)return err;
    }

    if(GOT("data")){
        NEXT
        GET("(")
        char *data;
        GET_STR(data)
        err = recording_parse_nodes(rec, data);
        if(err)return err;
        free(data);
        GET(")")
    }else{
        GET("nodata")
    }

    return 0;
}

int recording_load(recording_t *rec, const char *filename,
    vars_t *vars, body_t *body, bool loop
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    recording_init(rec, body, loop);
    err = recording_parse(rec, &lexer, filename);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

static int recording_step_play(recording_t *rec){
    int err;

    /* Empty recording: early exit */
    if(rec->nodes_len <= 0)return 0;

    body_t *body = rec->body;

    rec->frame_i++;

    if(rec->wait > 0){
        rec->wait--;
        if(rec->wait > 0)return 0;}

    while(1){
        /* Exit conditions: */
        if(rec->node_i >= rec->nodes_len){
            if(rec->loop){
                err = body_restart_recording(body, true, rec->resets_position);
                if(err)return err;
            }else{
                /* Stop playback, in fact unload recording entirely */
                recording_reset(rec);
                break;
            }
        }

        /* Process next node: */
        {
            recording_node_t *node = &rec->nodes[rec->node_i];
            switch(node->type){
                case RECORDING_NODE_TYPE_WAIT: {
                    rec->wait = node->u.wait;
                    break;
                }
                case RECORDING_NODE_TYPE_KEYDOWN: {
                    int key_i = body_get_key_i(body, node->u.key_c, false);
                    body_keydown(body, key_i);
                    break;
                }
                case RECORDING_NODE_TYPE_KEYUP: {
                    int key_i = body_get_key_i(body, node->u.key_c, false);
                    body_keyup(body, key_i);
                    break;
                }
                default: {
                    fprintf(stderr, "%s: Unrecognized node type: %i\n",
                        __func__, node->type);
                    return 2;
                }
            }
            rec->node_i++;
        }

        /* Exit conditions: */
        if(rec->wait > 0)break;
    }

    return 0;
}

static int recording_step_play_reverse(recording_t *rec){
    int err;

    /* Empty recording: early exit */
    if(rec->nodes_len <= 0)return 0;

    body_t *body = rec->body;

    /* TODO... */

    return 0;
}

int recording_step(recording_t *recording){
    int rec_action = recording->action;

    if(rec_action == 1){
        /* play */
        int err = recording_step_play(recording);
        if(err)return err;
    }else if(rec_action == 2){
        /* record */
        recording->wait++;
    }

    if(rec_action != 0 && DEBUG_RECORDINGS){
        printf("KEYS: ");
        #define DEBUG_PRINT_KEYS(keys) { \
            printf("["); \
            for(int i = 0; i < KEYINFO_KEYS; i++)printf("%i", keys[i]); \
            printf("]"); \
        }
        body_t *body = recording->body;
        DEBUG_PRINT_KEYS(body->keyinfo.isdown)
        DEBUG_PRINT_KEYS(body->keyinfo.wasdown)
        DEBUG_PRINT_KEYS(body->keyinfo.wentdown)
        #undef DEBUG_PRINT_KEYS
        printf("\n");
    }

    return 0;
}

int recording_step_reverse(recording_t *recording){
    int rec_action = recording->action;

    if(rec_action == 1){
        /* play */
        int err = recording_step_play_reverse(recording);
        if(err)return err;
    }else if(rec_action == 2){
        /* record */
        if(recording->wait > 0){
            recording->wait--;
        }else{
            // TODO: pop a node or something...
        }
    }

    return 0;
}



/******************
 * BODY RECORDING *
 ******************/

static int body_maybe_record_wait(body_t *body);

int body_load_recording(body_t *body, const char *filename, bool loop){
    int err;
    recording_t *rec = &body->recording;

    recording_reset(rec);
    err = recording_load(rec, filename, NULL, body, loop);
    if(err)return err;
    return 0;
}

int body_play_recording(body_t *body){
    int err;
    recording_t *rec = &body->recording;

    err = body_init_stateset(body, rec->stateset_name, rec->state_name);
    if(err)return err;

    body->recording.action = 1; /* play */
    return body_restart_recording(body, false, true);
}

int body_restart_recording(body_t *body, bool ignore_offset, bool reset_position){
    int err;
    recording_t *rec = &body->recording;

    rec->frame_i = 0;
    rec->node_i = 0;
    rec->wait = 0;

    keyinfo_copy(&body->keyinfo, &rec->keyinfo);
    body_set_state(body, rec->state_name, true);

    if(reset_position){
        vec_cpy(MAX_VEC_DIMS, body->pos, rec->pos0);
        body->rot = rec->rot0;
        body->turn = rec->turn0;
    }

    if(!ignore_offset){
        /* The following is pretty ganky; we're assuming that body won't
        have any "side effects" on the game during each step.
        Like, for instance, killing a player.
        Hmmm...
        Should we add a bool no_side_effects parameter to body_step??? */
        for(int i = 0; i < rec->offset; i++){
            err = body_step(body, body->game);
            if(err)return err;
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

static int body_maybe_record_wait(body_t *body){
    int wait = body->recording.wait;
    if(wait == 0)return 0;

    fprintf(body->recording.file, " w%i", wait);
    if(DEBUG_RECORDINGS)printf("w%i\n", wait);

    /*
    char buffer[4];
    buffer[0] = ' ';
    buffer[1] = 'w';
    buffer[2] = '1'; // <---- ummmmm
    buffer[3] = '\0';

    int err = recording_write(body, buffer);
    if(err)return err;
    */

    body->recording.wait = 0;
    return 0;
}

int body_record_keydown(body_t *body, int key_i){
    body_maybe_record_wait(body);

    char c = body_get_key_c(body, key_i, true);
    fprintf(body->recording.file, "+%c", c);
    if(DEBUG_RECORDINGS)printf("+%c\n", c);

    /*
    char buffer[3];
    buffer[0] = '+';
    buffer[1] = c;
    buffer[2] = '\0';
    int err = recording_write(body, buffer);
    if(err)return err;
    */

    return 0;
}

int body_record_keyup(body_t *body, int key_i){
    body_maybe_record_wait(body);

    char c = body_get_key_c(body, key_i, true);
    fprintf(body->recording.file, "-%c", c);
    if(DEBUG_RECORDINGS)printf("-%c\n", c);

    /*
    char buffer[3];
    buffer[0] = '-';
    buffer[1] = c;
    buffer[2] = '\0';
    int err = recording_write(body, buffer);
    if(err)return err;
    */

    return 0;
}
