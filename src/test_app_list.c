
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

static void test_app_list_data_set_options(test_app_list_data_t *data,
    const char **options, int index
){
    int length = 0;
    for(; options[length]; length++);

    data->options = options;
    data->options_index = _remainder(index, length);
    data->options_length = length;
}

static void test_app_list_data_set_options_stateset(test_app_list_data_t *data,
    const char **options, int index,
    stateset_t *stateset, state_t *cur_state
){
    int length = 0;
    for(; options[length]; length++);
    if(stateset)length += stateset->states_len;

    data->options = options;
    data->options_index = _remainder(index, length);
    data->options_length = length;
}

static void _console_write_bar(console_t *console, int index, int length){
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

static void _console_write_field(console_t *console, const char *name, const char *value){
    console_write_msg(console, name);
    console_write_msg(console, ": ");
    console_write_msg(console, value? value: "(unknown)");
    console_newline(console);
}

static void _console_write_options(console_t *console,
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

static void _console_write_options_stateset(console_t *console,
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
    "List submaps",
    "List recordings (TODO)",
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
        case 1: return test_app_open_list_submaps(data->app, NULL, map);
        default: break;
    }
    return 0;
}


/************************
* TEST_APP_LIST_SUBMAPS *
************************/

const char *test_app_list_submaps_options[] = {
    "List bodies (TODO)",
    "Open map",
    NULL
};

int test_app_open_list_submaps(test_app_t *app, hexmap_submap_t *submap, hexmap_t *map){
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
    data->index = _remainder(list->index_x, data->length);
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
        _console_write_field(console, "Filename", submap->filename);
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


/***********************
* TEST_APP_LIST_BODIES *
***********************/

const char *test_app_list_bodies_options[] = {
    "Set camera target",
    "Set first player",
    "Reset recording",
    "Open submap",
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
        _console_write_field(console, "Recording action",
            recording_action_msg(body->recording.action));
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
        case 1: {
            player_t *player = data->app->hexgame.players[0];
            player_set_body(player, body);
            camera_set_body(data->app->camera, body);
        } break;
        case 2: {
            recording_reset(&body->recording);
        } break;
        case 3: return test_app_open_list_submaps(data->app,
            body->cur_submap, body->map);
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



/***********************
* TEST_APP_LIST_ACTORS *
***********************/

const char *test_app_list_actors_options[] = {
    "Open body",
    "State picker",
    NULL
};

const char *test_app_list_actors_options_statepicker[] = {
    "Back",
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
    data->index = _remainder(list->index_x, data->length);
    data->item = data->length > 0? game->actors[data->index]: NULL;
    actor_t *actor = data->item;
    if(data->mode == TEST_APP_LIST_ACTORS_MODE_STATEPICKER){
        test_app_list_data_set_options_stateset(data,
            test_app_list_actors_options_statepicker, list->index_y,
            actor? &actor->stateset: NULL, actor? actor->state: NULL);
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
        _console_write_field(console, "Stateset", actor->stateset.filename);
        _console_write_field(console, "State", actor->state->name);
        _console_write_field(console, "Body stateset", body? body->stateset.filename: NULL);
        _console_write_field(console, "Body state", body? body->state->name: NULL);
        if(data->mode == TEST_APP_LIST_ACTORS_MODE_STATEPICKER){
            _console_write_options_stateset(console, data->options,
                data->options_index, data->options_length,
                &actor->stateset, actor->state);
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
                data->mode = TEST_APP_LIST_ACTORS_MODE_DEFAULT;
                list->index_y = 0;
            } break;
            default: {
                if(actor->stateset.states_len == 0)return 0;
                int options_length = data->options_length - actor->stateset.states_len;
                int state_index = data->options_index - options_length;
                state_t *state = actor->stateset.states[state_index];
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
                data->mode = TEST_APP_LIST_ACTORS_MODE_STATEPICKER;
                list->index_y = 0;
            } break;
            default: break;
        }
    }
    return 0;
}

