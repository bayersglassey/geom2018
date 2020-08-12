
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"



/**************************
* TEST_APP_LIST_RECORDING *
***************************/

int test_app_open_list_recording(test_app_t *app, recording_t *rec){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    new_data->body = rec->body;
    return test_app_open_list(app, "Recording",
        0, 0,
        new_data,
        &test_app_list_recording_step,
        &test_app_list_recording_render,
        &test_app_list_recording_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_recording_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    body_t *body = data->body;
    recording_t *rec = &body->recording;
    data->length = rec->nodes_len;
    data->index = rec->node_i;
    return 0;
}

int test_app_list_recording_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_bar(console, data->index, data->length);

    body_t *body = data->body;
    recording_t *rec = &body->recording;
    recording_node_t *node = rec->nodes_len > 0? &rec->nodes[rec->node_i]: NULL;

    _console_write_field(console, "Stateset", body->stateset.filename);
    _console_write_field(console, "State", body->state->name);
    _console_write_section(console, "Recording");
    _console_write_recording(console, rec, true);

    return 0;
}

int test_app_list_recording_select_item(test_app_list_t *list){
    int err;

    test_app_list_data_t *data = list->data;
    body_t *body = data->body;
    recording_t *rec = &body->recording;
    recording_node_t *node = rec->nodes_len > 0? &rec->nodes[rec->node_i]: NULL;

    return 0;
}
