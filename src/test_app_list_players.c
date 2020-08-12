
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"



/************************
* TEST_APP_LIST_PLAYERS *
************************/

const char *test_app_list_players_options[] = {
    "Open body",
    NULL
};

int test_app_open_list_players(test_app_t *app){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    return test_app_open_list(app, "Players",
        0, 0,
        new_data,
        &test_app_list_players_step,
        &test_app_list_players_render,
        &test_app_list_players_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_players_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    data->length = game->players_len;
    data->index = _test_app_list_remainder(list->index_x, data->length);
    data->item = data->length > 0? game->players[data->index]: NULL;
    test_app_list_data_set_options(data,
        test_app_list_players_options, list->index_y);
    return 0;
}

int test_app_list_players_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_bar(console, data->index, data->length);
    player_t *player = data->item;
    if(player != NULL){
        body_t *body = player->body;
        _console_write_field(console, "Body stateset", body? body->stateset.filename: NULL);
        _console_write_field(console, "Body state", body? body->state->name: NULL);
        _console_write_options(console, data->options,
            data->options_index, data->options_length);
    }
    return 0;
}

int test_app_list_players_select_item(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    player_t *player = data->item;
    if(player == NULL)return 0;
    switch(data->options_index){
        case 0: if(player->body){
            return test_app_open_list_bodies(data->app, player->body, NULL);
        } break;
        default: break;
    }
    return 0;
}

