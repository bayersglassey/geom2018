
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"



/***********************
* TEST_APP_LIST_BODIES *
***********************/

const char *test_app_list_bodies_options[] = {
    "Set camera target",
    "Set first player",
    "Open recording",
    "Open submap",
    "Open stateset (TODO)",
    NULL
};

const char *test_app_list_bodies_options_recording[] = {
    "<- Back",
    "Reset",
    "Open data",
    NULL
};

const char *test_app_list_bodies_options_recording_data[] = {
    "<- Back",
    NULL
};

enum {
    TEST_APP_LIST_BODIES_MODE_DEFAULT,
    TEST_APP_LIST_BODIES_MODE_RECORDING,
    TEST_APP_LIST_BODIES_MODE_RECORDING_DATA,
    TEST_APP_LIST_BODIES_MODES
};

int test_app_open_list_bodies(test_app_t *app,
    body_t *body, hexmap_t *map
){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    if(body != NULL)map = body->map;
    new_data->body = body;
    new_data->map = map;
    return test_app_open_list(app, "Bodies",
        body? body_get_index(body): 0, 0,
        new_data,
        &test_app_list_bodies_step,
        &test_app_list_bodies_render,
        &test_app_list_bodies_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_bodies_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexmap_t *map = data->map? data->map: data->app->hexgame.maps[0];
    data->length = map->bodies_len;
    data->index = _remainder(list->index_x, data->length);
    data->item = data->length > 0? map->bodies[data->index]: NULL;
    if(data->mode == TEST_APP_LIST_BODIES_MODE_RECORDING){
        test_app_list_data_set_options(data,
            test_app_list_bodies_options_recording, list->index_y);
    }else if(data->mode == TEST_APP_LIST_BODIES_MODE_RECORDING_DATA){
        test_app_list_data_set_options(data,
            test_app_list_bodies_options_recording_data, list->index_y);
    }else{
        test_app_list_data_set_options(data,
            test_app_list_bodies_options, list->index_y);
    }
    return 0;
}

int test_app_list_bodies_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_bar(console, data->index, data->length);
    body_t *body = data->item;
    if(body != NULL){
        _console_write_field(console, "Stateset", body->stateset.filename);
        _console_write_field(console, "State", body->state->name);
        if(data->mode != TEST_APP_LIST_BODIES_MODE_RECORDING_DATA){
            _console_write_keyinfo(console, body, &body->keyinfo);
        }

        /* Section for body->recording */
        recording_t *rec = &body->recording;
        _console_write_section(console, "Recording");
        _console_write_field(console, "Action",
            recording_action_msg(rec->action));
        WRITE_FIELD_BOOL(rec, reacts)
        WRITE_FIELD_BOOL(rec, loop)
        WRITE_FIELD(rec, stateset_name)
        WRITE_FIELD(rec, state_name)
        if(data->mode == TEST_APP_LIST_BODIES_MODE_RECORDING_DATA){
            _console_write_keyinfo(console, body, &rec->keyinfo);
        }else{
            WRITE_FIELD_VEC(rec, 4, pos0)
            WRITE_FIELD_INT(rec, rot0)
            WRITE_FIELD_BOOL(rec, turn0)
        }
        WRITE_FIELD_INT(rec, node_i)
        WRITE_FIELD_INT(rec, nodes_len)
        WRITE_FIELD_INT(rec, wait)
        WRITE_FIELD(rec, name)
        WRITE_FIELD_INT(rec, offset)

        _console_write_options(console, data->options,
            data->options_index, data->options_length);
    }
    return 0;
}

int test_app_list_bodies_select_item(test_app_list_t *list){
    int err;
    test_app_list_data_t *data = list->data;
    body_t *body = data->item;
    if(body == NULL)return 0;
    if(data->mode == TEST_APP_LIST_BODIES_MODE_RECORDING){
        recording_t *rec = &body->recording;
        switch(data->options_index){
            case 0: {
                test_app_list_data_set_mode(data,
                    TEST_APP_LIST_BODIES_MODE_DEFAULT);
            } break;
            case 1: {
                err = body_restart_recording(body, true, true);
                if(err)return err;
            } break;
            case 2: {
                test_app_list_data_set_mode(data,
                    TEST_APP_LIST_BODIES_MODE_RECORDING_DATA);
            } break;
            default: break;
        }
    }else if(data->mode == TEST_APP_LIST_BODIES_MODE_RECORDING_DATA){
        recording_t *rec = &body->recording;
        switch(data->options_index){
            case 0: {
                test_app_list_data_set_mode(data,
                    TEST_APP_LIST_BODIES_MODE_RECORDING);
            } break;
            default: break;
        }
    }else{
        switch(data->options_index){
            case 0: {
                camera_set_body(data->app->camera, body);
            } break;
            case 1: {
                player_t *player = data->app->hexgame.players[0];
                player_set_body(player, body);
                camera_set_body(data->app->camera, body);
            } break;
            case 2: {
                test_app_list_data_set_mode(data,
                    TEST_APP_LIST_BODIES_MODE_RECORDING);
            } break;
            case 3: {
                return test_app_open_list_submaps(data->app,
                    body->cur_submap, body->map);
            }
            default: break;
        }
    }
    return 0;
}
