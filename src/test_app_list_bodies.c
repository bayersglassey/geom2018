
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
    "Open player",
    "Open actor",
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

enum {
    TEST_APP_LIST_BODIES_MODE_DEFAULT,
    TEST_APP_LIST_BODIES_MODE_RECORDING,
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
    /* Now we frig with the currently-selected index into map->bodies...
    This is pretty weird. Probably instead, we should do something like
    check whether body->remove is true, which I believe tells us that
    body *will* be removed at start of next frame?.. or something?.. */
    data->index = _test_app_list_remainder(list->index_x, data->length);
    data->item = data->length > 0? map->bodies[data->index]: NULL;
    if(data->mode == TEST_APP_LIST_BODIES_MODE_RECORDING){
        test_app_list_data_set_options(data,
            test_app_list_bodies_options_recording, list->index_y);
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
        actor_t *actor = body_get_actor(body);
        if(actor){
            _console_write_field(console, "Actor stateset", actor->stateset->filename);
            _console_write_field(console, "Actor state", actor->state->name);
        }
        _console_write_field(console, "Stateset", body->stateset->filename);
        _console_write_field(console, "State", body->state->name);
        if(body->palmapper)_console_write_field(console, "Palmapper", body->palmapper->name);
        _console_write_keyinfo(console, body, &body->keyinfo);

        recording_t *rec = &body->recording;
        _console_write_section(console, "Recording");
        _console_write_recording(console, rec, false);

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
                recording_t *rec = &body->recording;
                return test_app_open_list_recording(data->app, rec);
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
                player->body = body;
                camera_set_body(data->app->camera, body);
            } break;
            case 2: {
                player_t *player = body_get_player(body);
                int player_i = player? player_get_index(player): -1;
                if(player_i < 0)break;
                return test_app_open_list_players(data->app, player_i);
            } break;
            case 3: {
                actor_t *actor = body_get_actor(body);
                int actor_i = actor? actor_get_index(actor): -1;
                if(actor_i < 0)break;
                return test_app_open_list_actors(data->app, actor_i);
            } break;
            case 4: {
                test_app_list_data_set_mode(data,
                    TEST_APP_LIST_BODIES_MODE_RECORDING);
            } break;
            case 5: {
                return test_app_open_list_submaps(data->app,
                    body->cur_submap, body->map);
            }
            default: break;
        }
    }
    return 0;
}
