
/* Utilities for use with test_app_list_*.c */

#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"


int _test_app_list_remainder(int a, int b){

    /* We need this to be safe, so we can call it with a, b the
    index and length of an empty test_app_list */
    if(b == 0)return 0;

    int rem = a % b;
    if(rem < 0){
        rem = (b < 0)? rem - b: rem + b;
    }
    return rem;
}



void test_app_list_data_set_options(test_app_list_data_t *data,
    const char **options, int index
){
    int length = 0;
    for(; options[length]; length++);

    data->options = options;
    data->options_index = _test_app_list_remainder(index, length);
    data->options_length = length;
}

void test_app_list_data_set_mode(test_app_list_data_t *data, int mode){
    data->mode = mode;
    data->app->list->index_y = 0;
}



void _console_write_bar(console_t *console, int index, int length){
#ifdef CONSOLE_WRITE_FANCY_BAR
    console_write_char(console, '[');
    for(int i = 0; i < length; i++){
        console_write_char(console, i == index? 'X': '-');
    }
    console_write_char(console, ']');
    console_newline(console);
#else
    console_printf(console, "[%i/%i]\n", index, length);
#endif
}

void _console_write_section(console_t *console, const char *name){
    console_printf(console, "--- %s: ---\n", name);
}

void _console_write_field_header(console_t *console, const char *name){
    console_write_msg(console, name);
    console_write_msg(console, ": ");
}

void _console_write_field(console_t *console, const char *name, const char *value){
    _console_write_field_header(console, name);
    console_write_msg(console, value? value: "(unknown)");
    console_newline(console);
}

void _console_write_field_bool(console_t *console, const char *name, bool value){
    _console_write_field(console, name, value? "Yes": "No");
}

void _console_write_field_int(console_t *console, const char *name, int value){
    _console_write_field_header(console, name);
    console_printf(console, "%i", value);
    console_newline(console);
}

void _console_write_field_vec(console_t *console, const char *name, int dims, vec_t value){
    _console_write_field_header(console, name);
    for(int i = 0; i < dims; i++){
        if(i > 0)console_write_char(console, ' ');
        console_printf(console, "%i", value[i]);
    }
    console_newline(console);
}


void _console_write_keyinfo(console_t *console, body_t *body, keyinfo_t *keyinfo){
    for(int i = 0; i < KEYINFO_KEYS; i++){
        char key_c = body_get_key_c(body, i, true);
        console_printf(console, "Key %c:", key_c);
        if(keyinfo->isdown[i])console_write_msg(console, " is");
        if(keyinfo->wasdown[i])console_write_msg(console, " was");
        if(keyinfo->wentdown[i])console_write_msg(console, " went");
        console_newline(console);
    }
}

void _console_write_recording(console_t *console, recording_t *rec, bool show_data){
    _console_write_field(console, "Action",
        recording_action_msg(rec->action));
    console_printf(console, "reacts=%c, loop=%c, resets_position=%c\n",
        rec->reacts? 'y': 'n',
        rec->loop? 'y': 'n',
        rec->resets_position? 'y': 'n');
    if(show_data){
        _console_write_keyinfo(console, rec->body, &rec->keyinfo);
    }else{
        WRITE_FIELD_VEC(rec, 4, pos0)
        WRITE_FIELD_INT(rec, rot0)
        WRITE_FIELD_BOOL(rec, turn0)
    }
    console_printf(console, "Node %i/%i (offset=%i, wait=%i)\n",
        rec->node_i, rec->nodes_len, rec->offset, rec->wait);
}

void _console_write_options(console_t *console,
    const char **options, int index, int length
){
    console_newline(console);

    /* NOTE: we must correctly handle the case where passed length is greater
    than number of options (so e.g. caller can render more options dynamically
    under these ones) */
    int i = 0;
    for(const char *option; option = options[i]; i++){
        console_write_char(console, i == index? '>': '-');
        console_write_char(console, ' ');
        console_write_msg(console, option);
        console_newline(console);
    }
}

void _console_write_options_stateset(console_t *console,
    const char **options, int index, int length,
    stateset_t *stateset, state_t *cur_state
){
    _console_write_options(console, options, index, length);

    console_write_line(console, "  *** States: ***");
    int options_length = length - stateset->states_len;
    for(int i = 0; i < stateset->states_len; i++){
        state_t *state = stateset->states[i];
        console_write_char(console, options_length + i == index? '>': '-');
        console_write_char(console, state == cur_state? '>': ' ');
        console_write_msg(console, state->name);
        console_newline(console);
    }
}

