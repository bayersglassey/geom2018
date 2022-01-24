
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"



static void test_app_list_data_set_options_stateset(test_app_list_data_t *data,
    const char **options, int index,
    stateset_t *stateset, state_t *cur_state
){
    int length = 0;
    for(; options[length]; length++);
    if(stateset)length += stateset->states_len;

    data->options = options;
    data->options_index = _test_app_list_remainder(index, length);
    data->options_length = length;
}



/***********************
* TEST_APP_LIST_ACTORS *
***********************/

const char *test_app_list_actors_options[] = {
    "Open body",
    "State picker",
    NULL
};

const char *test_app_list_actors_options_statepicker[] = {
    "<- Back",
    NULL
};

enum {
    TEST_APP_LIST_ACTORS_MODE_DEFAULT,
    TEST_APP_LIST_ACTORS_MODE_STATEPICKER,
    TEST_APP_LIST_ACTORS_MODES
};

int test_app_open_list_actors(test_app_t *app){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    return test_app_open_list(app, "Actors",
        0, 0,
        new_data,
        &test_app_list_actors_step,
        &test_app_list_actors_render,
        &test_app_list_actors_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_actors_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    data->length = game->actors_len;
    data->index = _test_app_list_remainder(list->index_x, data->length);
    data->item = data->length > 0? game->actors[data->index]: NULL;
    actor_t *actor = data->item;
    if(data->mode == TEST_APP_LIST_ACTORS_MODE_STATEPICKER){
        test_app_list_data_set_options_stateset(data,
            test_app_list_actors_options_statepicker, list->index_y,
            actor? actor->stateset: NULL, actor? actor->state: NULL);
    }else{
        test_app_list_data_set_options(data,
            test_app_list_actors_options, list->index_y);
    }
    return 0;
}

int test_app_list_actors_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_bar(console, data->index, data->length);
    actor_t *actor = data->item;
    if(actor != NULL){
        body_t *body = actor->body;
        _console_write_field(console, "Stateset", actor->stateset->filename);
        _console_write_field(console, "State", actor->state->name);
        _console_write_field(console, "Body stateset", body? body->stateset->filename: NULL);
        _console_write_field(console, "Body state", body? body->state->name: NULL);
        if(data->mode == TEST_APP_LIST_ACTORS_MODE_STATEPICKER){
            _console_write_options_stateset(console, data->options,
                data->options_index, data->options_length,
                actor->stateset, actor->state);
        }else{
            _console_write_options(console, data->options,
                data->options_index, data->options_length);
        }
    }
    return 0;
}

int test_app_list_actors_select_item(test_app_list_t *list){
    int err;
    test_app_list_data_t *data = list->data;
    actor_t *actor = data->item;
    if(actor == NULL)return 0;
    body_t *body = actor->body;
    if(body == NULL)return 0;
    if(data->mode == TEST_APP_LIST_ACTORS_MODE_STATEPICKER){
        switch(data->options_index){
            case 0: {
                test_app_list_data_set_mode(data,
                    TEST_APP_LIST_ACTORS_MODE_DEFAULT);
            } break;
            default: {
                if(actor->stateset->states_len == 0)return 0;
                int options_length = data->options_length - actor->stateset->states_len;
                int state_index = data->options_index - options_length;
                state_t *state = actor->stateset->states[state_index];
                err = actor_set_state(actor, state->name);
                if(err)return err;
                if(body != NULL)body->recording.action = 0;
            } break;
        }
    }else{
        switch(data->options_index){
            case 0: {
                return test_app_open_list_bodies(data->app, body, NULL);
            } break;
            case 1: {
                test_app_list_data_set_mode(data,
                    TEST_APP_LIST_ACTORS_MODE_STATEPICKER);
            } break;
            default: break;
        }
    }
    return 0;
}

