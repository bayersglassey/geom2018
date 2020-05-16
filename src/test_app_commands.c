
#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "test_app.h"
#include "prismelrenderer.h"
#include "rendergraph.h"
#include "array.h"
#include "vec4.h"
#include "font.h"
#include "sdlfont.h"
#include "geomfont.h"
#include "console.h"
#include "util.h"
#include "anim.h"
#include "hexmap.h"
#include "hexgame.h"
#include "hexspace.h"
#include "lexer_macros.h"



/*************************************
* GENERAL COMMAND STRUCTURES & UTILS *
*************************************/

typedef struct test_app_command {
    const char *name;
    const char *params;
    int (*action)(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr);
} test_app_command_t;

static void console_write_command(console_t *console, test_app_command_t *command){
    console_write_msg(console, command->name);
    if(command->params){
        console_write_msg(console, " ");
        console_write_msg(console, command->params);
    }
}

test_app_command_t _test_app_commands[];


static bool _startswith(const char *s, const char *prefix){
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

void test_app_write_console_commands(test_app_t *app, const char *prefix){
    for(test_app_command_t *command = _test_app_commands; command->name; command++){
        if(prefix && !_startswith(command->name, prefix))continue;
        console_write_msg(&app->console, " * ");
        console_write_command(&app->console, command);
        console_write_msg(&app->console, "\n");
    }
}



/**************************************
* SPECIFIC COMMAND STRUCTURES & UTILS *
**************************************/

typedef struct test_app_list_data {
    test_app_t *app;

    /* Now a bag of fields which might be useful depending on what you're
    listing. This structure doesn't know; that's determined by the callbacks
    you passed to the app_list_t. */
    hexmap_t *map;
    hexmap_submap_t *submap;
} test_app_list_data_t;

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

static int test_app_list_maps_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    int length = game->maps_len;
    int index = _remainder(list->index, length);

    console_t *console = &data->app->console;
    console_clear(console);
    console_write_line(console, "Maps");
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    hexmap_t *map = game->maps[index];
    _console_write_field(console, "Name", map->name);
    return 0;
}

static int test_app_list_bodies_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexmap_t *map = data->map? data->map: data->app->hexgame.maps[0];
    int length = map->bodies_len;
    int index = _remainder(list->index, length);

    console_t *console = &data->app->console;
    console_clear(console);
    console_write_line(console, "Bodies");
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    body_t *body = map->bodies[index];
    _console_write_field(console, "Stateset", body->stateset.filename);
    return 0;
}

static int test_app_list_players_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    int length = game->players_len;
    int index = _remainder(list->index, length);

    console_t *console = &data->app->console;
    console_clear(console);
    console_write_line(console, "Players");
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    player_t *player = game->players[index];
    body_t *body = player->body;
    _console_write_field(console, "Stateset", body? body->stateset.filename: NULL);
    return 0;
}

static int test_app_list_actors_render(test_app_list_t *list){
    test_app_list_data_t *data = list->data;
    hexgame_t *game = &data->app->hexgame;
    int length = game->actors_len;
    int index = _remainder(list->index, length);

    console_t *console = &data->app->console;
    console_clear(console);
    console_write_line(console, "Actors");
    _console_write_bar(console, index, length);
    if(length == 0)return 0;

    actor_t *actor = game->actors[index];
    body_t *body = actor->body;
    _console_write_field(console, "Stateset", body? body->stateset.filename: NULL);
    return 0;
}

static int test_app_list_data_cleanup(test_app_list_t *list){
    /* Generic implementation, just frees the data */
    test_app_list_data_t *data = list->data;
    free(data);
    return 0;
}


/**************************
* COMMAND IMPLEMENTATIONS *
**************************/

static int _test_app_command_exit(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    app->loop = false;
    return 0;
}

static int _test_app_command_help(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    console_write_msg(&app->console,
        "Press Tab to see a list of commands.\n");
    console_write_msg(&app->console,
        "Type a few letters first to filter the listed commands.\n");
    console_write_msg(&app->console,
        "For example, \"he\"-Tab should list \"help\".\n");
    return 0;
}

static int _test_app_command_cls(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    console_clear(&app->console);
    return 0;
}

static int _test_app_command_list_maps(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    test_app_list_data_t *data = calloc(1, sizeof(*data));
    if(data == NULL)return 1;
    data->app = app;
    return test_app_open_list(app, data,
        &test_app_list_maps_render,
        &test_app_list_data_cleanup);
}

static int _test_app_command_list_bodies(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    test_app_list_data_t *data = calloc(1, sizeof(*data));
    if(data == NULL)return 1;
    data->app = app;
    body_t *body = app->camera->body;
    data->map = body? body->map: NULL;
    return test_app_open_list(app, data,
        &test_app_list_bodies_render,
        &test_app_list_data_cleanup);
    return 0;
}

static int _test_app_command_list_players(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    test_app_list_data_t *data = calloc(1, sizeof(*data));
    if(data == NULL)return 1;
    data->app = app;
    return test_app_open_list(app, data,
        &test_app_list_players_render,
        &test_app_list_data_cleanup);
    return 0;
}

static int _test_app_command_list_actors(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    test_app_list_data_t *data = calloc(1, sizeof(*data));
    if(data == NULL)return 1;
    data->app = app;
    return test_app_open_list(app, data,
        &test_app_list_actors_render,
        &test_app_list_data_cleanup);
    return 0;
}

static int _test_app_command_add_player(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    char *stateset_filename;

    if(!fus_lexer_done(lexer)){
        err = fus_lexer_get_str(lexer, &stateset_filename);
        if(err)goto lexer_err;
    }else{
        stateset_filename = strdup(app->stateset_filename);
        if(!stateset_filename)return 1;
    }

    hexgame_t *game = &app->hexgame;
    /* HACK: we just grab maps[0] */
    hexmap_t *map = game->maps[0];
    vec_ptr_t respawn_pos = map->spawn;
    rot_t respawn_rot = 0;
    bool respawn_turn = false;
    char *respawn_map_filename = NULL;
    if(game->players_len > 0){
        /* HACK: we just grab players[0] */
        player_t *player = game->players[0];
        respawn_pos = player->respawn_location.pos;
        respawn_rot = player->respawn_location.rot;
        respawn_turn = player->respawn_location.turn;
        respawn_map_filename = strdup(player->respawn_location.map_filename);
        if(!respawn_map_filename)return 1;
    }else{
        respawn_map_filename = strdup(map->name);
        if(!respawn_map_filename)return 1;
    }

    hexmap_t *respawn_map;
    err = hexgame_get_or_load_map(game, respawn_map_filename,
        &respawn_map);
    if(err)return err;

    int keymap = -1;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->keymap > keymap)keymap = player->keymap;
    }
    keymap++;

    ARRAY_PUSH_NEW(body_t*, respawn_map->bodies, body)
    err = body_init(body, game, respawn_map, stateset_filename,
        NULL, NULL);
    if(err)return err;

    ARRAY_PUSH_NEW(player_t*, game->players, player)
    err = player_init(player, game, keymap,
        respawn_pos, respawn_rot, respawn_turn, respawn_map_filename,
        NULL);
    if(err)return err;

    /* Attach body to player */
    player->body = body;

    /* Move body to the respawn location */
    err = body_respawn(body, respawn_pos, respawn_rot, respawn_turn, map);
    if(err)return err;

    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_edit_player(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    int player_i = 0;
    char *stateset_filename;

    if(!fus_lexer_done(lexer)){
        err = fus_lexer_get_int(lexer, &player_i);
        if(err)goto lexer_err;
    }

    if(!fus_lexer_done(lexer)){
        err = fus_lexer_get_str(lexer, &stateset_filename);
        if(err)goto lexer_err;
    }else{
        stateset_filename = strdup(app->stateset_filename);
        if(!stateset_filename)return 1;
    }

    hexgame_t *game = &app->hexgame;
    if(player_i < 0 || player_i >= game->players_len){
        console_write_msg(&app->console, "Player # invalid\n");
        return 0;
    }

    player_t *player = game->players[player_i];

    err = body_set_stateset(player->body, stateset_filename, NULL);
    if(err)return err;

    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_save(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    char *filename = NULL;

    if(!fus_lexer_done(lexer)){
        err = fus_lexer_get_str(lexer, &filename);
        if(err)goto lexer_err;
    }

    if(filename == NULL){
        err = prismelrenderer_write(&app->prend, stdout);
        if(err)return err;
    }else{
        err = prismelrenderer_save(&app->prend, filename);
        if(err)return err;
    }
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_dump(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    int dump_bitmaps = 1;
    int dump_what = 0; /* rgraph, prend */
    while(1){
        if(fus_lexer_done(lexer))break;
        else if(fus_lexer_got(lexer, "rgraph"))dump_what = 0;
        else if(fus_lexer_got(lexer, "prend"))dump_what = 1;
        else if(fus_lexer_got(lexer, "nobitmaps"))dump_bitmaps = 0;
        else if(fus_lexer_got(lexer, "surfaces")){
            /* WARNING: doing this with "prend" after "renderall" causes
            my laptop to hang... */
            dump_bitmaps = 2;}
        else goto lexer_err;
        err = fus_lexer_next(lexer);
        if(err)return err;
    }
    if(dump_what == 0){
        rendergraph_t *rgraph =
            app->prend.rendergraphs[app->cur_rgraph_i];
        rendergraph_dump(rgraph, stdout, 0, dump_bitmaps);
    }else if(dump_what == 1){
        prismelrenderer_dump(&app->prend, stdout, dump_bitmaps);
    }
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_map(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    char *mapper_name;
    char *mapped_rgraph_name;
    char *resulting_rgraph_name = NULL;

    err = fus_lexer_get_str(lexer, &mapper_name);
    if(err)goto lexer_err;
    err = fus_lexer_get_str(lexer, &mapped_rgraph_name);
    if(err)goto lexer_err;
    if(!fus_lexer_done(lexer)){
        err = fus_lexer_get_str(lexer, &resulting_rgraph_name);
        if(err)goto lexer_err;
    }

    prismelmapper_t *mapper = prismelrenderer_get_mapper(
        &app->prend, mapper_name);
    if(mapper == NULL){
        fprintf(stderr, "Couldn't find mapper: %s\n", mapper_name);
        return 2;}
    rendergraph_t *mapped_rgraph = prismelrenderer_get_rendergraph(
        &app->prend, mapped_rgraph_name);
    if(mapped_rgraph == NULL){
        fprintf(stderr, "Couldn't find shape: %s\n",
            mapped_rgraph_name);
        return 2;}

    free(mapper_name);
    free(mapped_rgraph_name);

    rendergraph_t *rgraph;
    err = prismelmapper_apply_to_rendergraph(mapper, &app->prend,
        mapped_rgraph, resulting_rgraph_name, app->prend.space,
        NULL, &rgraph);
    if(err)return err;
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_renderall(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    SDL_Renderer *renderer = NULL;
    err = prismelrenderer_render_all_bitmaps(
        &app->prend, app->sdl_palette);
    if(err)return err;
    return 0;
}

static int _test_app_command_get_shape(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    char *name;
    err = fus_lexer_get_str(lexer, &name);
    if(err)goto lexer_err;
    bool found = false;
    for(int i = 0; i < app->prend.rendergraphs_len; i++){
        rendergraph_t *rgraph = app->prend.rendergraphs[i];
        if(!strcmp(rgraph->name, name)){
            app->cur_rgraph_i = i;
            found = true;
            break;
        }
    }
    if(!found){
        fprintf(stderr, "Couldn't find shape: %s\n", name);
        return 2;}
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_mode(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    if(GOT("game")){
        app->mode = TEST_APP_MODE_GAME;
    }else if(GOT("editor")){
        app->mode = TEST_APP_MODE_EDITOR;
    }else{
        UNEXPECTED("game or editor");
        goto lexer_err;
    }
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}


/*********************************
* END OF COMMAND IMPLEMENTATIONS *
*********************************/


#define COMMAND(NAME, PARAMS) (test_app_command_t){#NAME, PARAMS, &_test_app_command_##NAME}
#define NULLCOMMAND (test_app_command_t){NULL, NULL, NULL}
test_app_command_t _test_app_commands[] = {
    COMMAND(exit, NULL),
    COMMAND(help, NULL),
    COMMAND(cls, NULL),
    COMMAND(list_maps, NULL),
    COMMAND(list_bodies, "[map_index]"),
    COMMAND(list_players, NULL),
    COMMAND(list_actors, NULL),
    COMMAND(add_player, "[stateset]"),
    COMMAND(edit_player, "player_index [stateset]"),
    COMMAND(save, "[filename]"),
    COMMAND(dump, "[rgraph|prend|nobitmaps|surfaces ...]"),
    COMMAND(map, "mapper rgraph [resulting_rgraph]"),
    COMMAND(renderall, NULL),
    COMMAND(get_shape, "shape"),
    COMMAND(mode, "game|editor"),
    NULLCOMMAND
};



static int _test_app_process_console_input(test_app_t *app, fus_lexer_t *lexer){
    int err;

    test_app_command_t *command;
    for(command = _test_app_commands; command->name; command++){
        if(fus_lexer_got(lexer, command->name)){
            err = fus_lexer_next(lexer);
            if(err)goto lexer_err;
            break;
        }
    }

    if(!command->name){
        fus_lexer_unexpected(lexer, NULL);
        console_write_msg(&app->console,
            "Didn't recognize that command. Try \"help\"\n");
        return 0;
    }

    bool lexer_err = false;
    err = (*command->action)(app, lexer, &lexer_err);
    if(err)return err;
    if(lexer_err)goto lexer_err;

    console_write_msg(&app->console, "OK\n");
    return 0;
lexer_err:
    console_write_msg(&app->console, "Couldn't parse that.\nUsage: ");
    console_write_command(&app->console, command);
    console_write_msg(&app->console, "\n");
    return 0;
}


int test_app_process_console_input(test_app_t *app){
    int err;
    fus_lexer_t lexer;

    err = fus_lexer_init(&lexer, app->console.input, "<console input>");
    if(err)return err;

    err = _test_app_process_console_input(app, &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);
    return 0;
}

