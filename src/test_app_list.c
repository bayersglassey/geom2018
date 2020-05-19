
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
}

static void test_app_list_data_set_options(test_app_list_data_t *data,
    const char **options, int index
){
    int length = 0;
    for(; options[length]; length++);

    data->options = options;
    data->options_index = _remainder(index, length);
    data->options_length = length;
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
    test_app_list_callback_t *step,
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
    list->step = step;
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

int test_app_list_cleanup_data(test_app_list_t *list){
    /* (This is a list->cleanup callback) */
    test_app_list_data_t *data = list->data;
    test_app_list_data_cleanup(data);
    free(data);
    return 0;
}


/*********************
* TEST_APP_LIST_MAPS *
*********************/

const char *test_app_list_maps_options[] = {
    "List bodies",
    "List submaps (TODO)",
    NULL
};

int test_app_open_list_maps(test_app_t *app, body_t *body, hexmap_t *map){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    if(body != NULL)map = body->map;
    new_data->body = body;
    new_data->map = map;
    return test_app_open_list(app, "Maps",
        map? hexgame_get_map_index(&app->hexgame, map): 0, 0,
        new_data,
        &test_app_list_maps_step,
        &test_app_list_maps_render,
        &test_app_list_maps_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_maps_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    data->length = game->maps_len;
    data->index = _remainder(list->index_x, data->length);
    data->item = data->length > 0? game->maps[data->index]: NULL;
    test_app_list_data_set_options(data,
        test_app_list_maps_options, list->index_y);
    return 0;
}

int test_app_list_maps_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_bar(console, data->index, data->length);
    hexmap_t *map = data->item;
    if(map != NULL){
        _console_write_field(console, "Name", map->name);
        _console_write_options(console, data->options,
            data->options_index, data->options_length);
    }
    return 0;
}

int test_app_list_maps_select_item(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexmap_t *map = data->item;
    if(map == NULL)return 0;
    switch(data->options_index){
        case 0: return test_app_open_list_bodies(data->app, NULL, map);
        default: break;
    }
    return 0;
}


/***********************
* TEST_APP_LIST_BODIES *
***********************/

const char *test_app_list_bodies_options[] = {
    "Set camera target",
    "Open stateset (TODO)",
    NULL
};

int test_app_open_list_bodies(test_app_t *app, body_t *body, hexmap_t *map){
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
    test_app_list_data_set_options(data,
        test_app_list_bodies_options, list->index_y);
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
        _console_write_options(console, data->options,
            data->options_index, data->options_length);
    }
    return 0;
}

int test_app_list_bodies_select_item(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    body_t *body = data->item;
    if(body == NULL)return 0;
    switch(data->options_index){
        case 0: {
            camera_set_body(data->app->camera, body);
        } break;
        default: break;
    }
    return 0;
}


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
    data->index = _remainder(list->index_x, data->length);
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
        _console_write_field(console, "Stateset", body? body->stateset.filename: NULL);
        _console_write_field(console, "State", body? body->state->name: NULL);
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



/***********************
* TEST_APP_LIST_ACTORS *
***********************/

const char *test_app_list_actors_options[] = {
    NULL
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
    data->index = _remainder(list->index_x, data->length);
    data->item = data->length > 0? game->actors[data->index]: NULL;
    test_app_list_data_set_options(data,
        test_app_list_actors_options, list->index_y);
    return 0;
}

int test_app_list_actors_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_bar(console, data->index, data->length);
    actor_t *actor = data->item;
    if(actor != NULL){
        body_t *body = actor->body;
        _console_write_field(console, "Stateset", actor->stateset.filename);
        _console_write_field(console, "State", actor->state->name);
        _console_write_field(console, "Body stateset", body? body->stateset.filename: NULL);
        _console_write_field(console, "Body state", body? body->state->name: NULL);
        _console_write_options(console, data->options,
            data->options_index, data->options_length);
    }
    return 0;
}

int test_app_list_actors_select_item(test_app_list_t *list){
    return 0;
}

