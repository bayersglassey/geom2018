
#include <stdio.h>
#include <stdbool.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "test_app_list_utils.h"


/************************
* TEST_APP_LIST_SUBMAPS *
************************/

const char *test_app_list_submaps_options[] = {
    "List bodies (TODO)",
    "Open map",
    NULL
};

int test_app_open_list_submaps(test_app_t *app,
    hexmap_submap_t *submap, hexmap_t *map
){
    test_app_list_data_t *new_data = test_app_list_data_create(app);
    if(new_data == NULL)return 1;

    if(submap != NULL)map = submap->map;
    if(map == NULL)map = app->hexgame.maps[0];
    new_data->submap = submap;
    new_data->map = map;
    return test_app_open_list(app, "Submaps",
        submap? hexmap_get_submap_index(map, submap): 0, 0,
        new_data,
        &test_app_list_submaps_step,
        &test_app_list_submaps_render,
        &test_app_list_submaps_select_item,
        &test_app_list_cleanup_data);
}

int test_app_list_submaps_step(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexmap_t *map = data->map;
    data->length = map->submaps_len;
    data->index = _test_app_list_remainder(list->index_x, data->length);
    data->item = data->length > 0? map->submaps[data->index]: NULL;
    test_app_list_data_set_options(data,
        test_app_list_submaps_options, list->index_y);
    return 0;
}

int test_app_list_submaps_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    console_t *console = &data->app->console;
    _console_write_bar(console, data->index, data->length);
    hexmap_submap_t *submap = data->item;
    if(submap != NULL){
        hexmap_t *map = submap->map;
        vecspace_t *space = map->space;

        WRITE_FIELD(submap, filename)
        WRITE_FIELD_BOOL(submap, solid)
        WRITE_FIELD_VEC(submap, space->dims, pos)
        WRITE_FIELD_VEC(submap, space->dims, camera_pos)
        _console_write_field(console, "camera_type",
            submap_camera_type_msg(submap->camera_type));
        WRITE_FIELD(submap, palette->name)
        WRITE_FIELD(submap, tileset->name)
        WRITE_FIELD(submap, mapper->name)
        for(int i = 0; i < submap->doors_len; i++){
            hexmap_door_t *door = submap->doors[i];
            _console_write_field(console, "door",
                hexmap_door_type_msg(door->type));
        }

        _console_write_options(console, data->options,
            data->options_index, data->options_length);
    }
    return 0;
}

int test_app_list_submaps_select_item(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexmap_submap_t *submap = data->item;
    if(submap == NULL)return 0;
    switch(data->options_index){
        case 1: return test_app_open_list_maps(data->app, NULL, submap->map);
        default: break;
    }
    return 0;
}

