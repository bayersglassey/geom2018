
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"



/*******************
* STATIC UTILITIES *
*******************/

static int _remainder(int a, int b){

    /* We need this to be safe, so we can call it with a, b the
    index and length of an empty test_app_list */
    if(b == 0)return 0;

    int rem = a % b;
    if(rem < 0){
        rem = (b < 0)? rem - b: rem + b;
    }
    return rem;
}

static void _get_options_index_and_length(const char **options, int index,
    int *index_ptr, int *length_ptr
){
    int length = 0;
    for(; options[length]; length++);

    *index_ptr = _remainder(index, length);
    *length_ptr = length;
}

static void _console_write_bar(console_t *console, int index, int length){
    console_write_char(console, '[');
    for(int i = 0; i < length; i++){
        console_write_char(console, i == index? 'X': '-');
    }
    console_write_char(console, ']');
    console_newline(console);
}

static void _console_write_field(console_t *console, const char *name, const char *value){
    console_write_msg(console, name);
    console_write_msg(console, ": ");
    console_write_msg(console, value? value: "(unknown)");
    console_newline(console);
}

static void _console_write_options(console_t *console, const char **options,
    int index, int length
){
    console_newline(console);
    for(int i = 0; i < length; i++){
        const char *option = options[i];
        console_write_char(console, i == index? 'X': '-');
        console_write_char(console, ' ');
        console_write_msg(console, option);
        console_newline(console);
    }
}


/****************
* TEST_APP_LIST *
****************/

void test_app_list_init(test_app_list_t *list,
    const char *title, test_app_list_t *prev,
    int index_x, int index_y,
    void *data,
    test_app_list_callback_t *render,
    test_app_list_callback_t *select_item,
    test_app_list_callback_t *cleanup
){
    memset(list, 0, sizeof(*list));
    list->title = title;
    list->prev = prev;
    list->index_x = index_x;
    list->index_y = index_y;
    list->data = data;
    list->render = render;
    list->select_item = select_item;
    list->cleanup = cleanup;
}

void test_app_list_cleanup(test_app_list_t *list){
    if(list->prev){
        test_app_list_cleanup(list->prev);
        free(list->prev);
    }
    if(list->cleanup){
        int err = list->cleanup(list);
        /* But do nothing with err, because we return void */
    }
}


/*********************
* TEST_APP_LIST_DATA *
*********************/

test_app_list_data_t *test_app_list_data_create(test_app_t *app){
    test_app_list_data_t *data = calloc(1, sizeof(*data));
    if(data == NULL)return NULL;
    data->app = app;
    return data;
}

void test_app_list_data_cleanup(test_app_list_data_t *data){
    /* Nuthin */
}


/*******************************
* TEST_APP_LIST_DATA CALLBACKS *
*******************************/

int test_app_list_cleanup_data(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    test_app_list_data_cleanup(data);
    free(data);
    return 0;
}

const char *test_app_list_maps_options[] = {
    "List bodies",
    NULL
};

int test_app_list_maps_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    int length = game->maps_len;
    int index = _remainder(list->index_x, length);

    console_t *console = &data->app->console;
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    hexmap_t *map = game->maps[index];

    _console_write_field(console, "Name", map->name);

    const char **options = test_app_list_maps_options;
    int options_index, options_length;
    _get_options_index_and_length(options, list->index_y,
        &options_index, &options_length);
    _console_write_options(console, options,
        options_index, options_length);
    return 0;
}

int test_app_list_maps_select_item(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    int length = game->maps_len;
    int index = _remainder(list->index_x, length);
    if(length == 0)return 0;

    hexmap_t *map = game->maps[index];

    const char **options = test_app_list_maps_options;
    int options_index, options_length;
    _get_options_index_and_length(options, list->index_y,
        &options_index, &options_length);

    switch(options_index){
        case 0: {
            test_app_list_data_t *new_data = test_app_list_data_create(data->app);
            if(new_data == NULL)return 1;

            new_data->map = map;
            return test_app_open_list(data->app, "Bodies",
                0, 0,
                new_data,
                &test_app_list_bodies_render,
                &test_app_list_bodies_select_item,
                &test_app_list_cleanup_data);
        } break;
    }
    return 0;
}

int test_app_list_bodies_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexmap_t *map = data->map? data->map: data->app->hexgame.maps[0];
    int length = map->bodies_len;
    int index = _remainder(list->index_x, length);

    console_t *console = &data->app->console;
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    body_t *body = map->bodies[index];

    _console_write_field(console, "Stateset", body->stateset.filename);
    return 0;
}

int test_app_list_bodies_select_item(test_app_list_t *list){
    return 0;
}

int test_app_list_players_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    int length = game->players_len;
    int index = _remainder(list->index_x, length);

    console_t *console = &data->app->console;
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    player_t *player = game->players[index];
    body_t *body = player->body;

    _console_write_field(console, "Stateset", body? body->stateset.filename: NULL);
    return 0;
}

int test_app_list_players_select_item(test_app_list_t *list){
    return 0;
}

int test_app_list_actors_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    int length = game->actors_len;
    int index = _remainder(list->index_x, length);

    console_t *console = &data->app->console;
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    actor_t *actor = game->actors[index];
    body_t *body = actor->body;

    _console_write_field(console, "Stateset", body? body->stateset.filename: NULL);
    return 0;
}

int test_app_list_actors_select_item(test_app_list_t *list){
    return 0;
}

