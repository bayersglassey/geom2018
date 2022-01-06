
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"



/*********************
* TEST_APP_LIST_MAPS *
*********************/

const char *test_app_list_maps_options[] = {
    "List bodies",
    "List submaps",
    "Reveal entire minimap",
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
    data->index = _test_app_list_remainder(list->index_x, data->length);
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
        _console_write_field(console, "Filename", map->filename);
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
        case 1: return test_app_open_list_submaps(data->app, NULL, map);
        case 2: {
            for(int i = 0; i < map->submaps_len; i++){
                hexmap_submap_t *submap = map->submaps[i];
                hexmap_submap_visit(submap);
            }
            break;
        }
        default: break;
    }
    return 0;
}

