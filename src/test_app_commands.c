
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "lexer_macros.h"



/*******************
* STATIC UTILITIES *
*******************/

static bool _startswith(const char *s, const char *prefix){
    return strncmp(s, prefix, strlen(prefix)) == 0;
}


/*******************
* TEST_APP_COMMAND *
*******************/

typedef struct test_app_command {
    const char *name;
    const char *alt_name;
    const char *params;
    int (*action)(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr);
} test_app_command_t;

static void test_app_command_write_to_console(
    test_app_command_t *command, console_t *console
){
    if(command->alt_name){
        console_write_char(console, '(');
        console_write_msg(console, command->name);
        console_write_msg(console, " | ");
        console_write_msg(console, command->alt_name);
        console_write_char(console, ')');
    }else{
        console_write_msg(console, command->name);
    }
    if(command->params){
        console_write_msg(console, " ");
        console_write_msg(console, command->params);
    }
}



/********************
* TEST_APP_COMMANDS *
********************/

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

static int _test_app_command_list_worldmaps_select_item(test_app_list_t *list){
    int err;
    test_app_list_data_t *data = list->data;
    test_app_t *app = data->app;

    int worldmap_index = data->options_index;
    hexgame_t *game = &app->hexgame;
    const char *worldmap = game->worldmaps[worldmap_index];

    hexmap_t *map;
    err = hexgame_get_or_load_map(game, worldmap, &map);
    if(err)return err;

    err = test_app_close_list(app);
    if(err)return err;

    return test_app_open_list_maps(app, NULL, map);
}

static int _test_app_command_list_worldmaps(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    hexgame_t *game = &app->hexgame;
    hexmap_t *map = app->camera->map;

    int worldmap_index = 0;
    for(int i = 0; i < game->worldmaps_len; i++){
        const char *worldmap = game->worldmaps[i];
        if(!strcmp(worldmap, map->name)){
            worldmap_index = i;
            break;
        }
    }

    return test_app_open_list_choices(app, "Worldmaps",
        (const char **)game->worldmaps, game->worldmaps_len, worldmap_index,
        &_test_app_command_list_worldmaps_select_item);
}

static int _test_app_command_list_maps(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    body_t *body = app->camera->body;
    return test_app_open_list_maps(app, body, NULL);
}

static int _test_app_command_list_submaps(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    body_t *body = app->camera->body;
    return test_app_open_list_submaps(app, body? body->cur_submap: NULL, body? body->map: NULL);
}

static int _test_app_command_list_bodies(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    body_t *body = app->camera->body;
    return test_app_open_list_bodies(app, body, NULL);
}

static int _test_app_command_list_players(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    return test_app_open_list_players(app);
}

static int _test_app_command_list_actors(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    return test_app_open_list_actors(app);
}

static int _test_app_command_add_player(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;

    const char *stateset_filename = app->stateset_filename;
    if(!DONE){
        GET_STR_CACHED(stateset_filename, &app->prend.filename_store)
    }

    hexgame_t *game = &app->hexgame;
    /* HACK: we just grab maps[0] */
    hexmap_t *map = game->maps[0];
    hexgame_location_t spawn = map->spawn;
    const char *respawn_map_filename = map->name;
    if(game->players_len > 0){
        /* HACK: we just grab players[0] */
        player_t *player = game->players[0];
        spawn = player->respawn_location.loc;
        respawn_map_filename = player->respawn_location.map_filename;
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
        spawn.pos, spawn.rot, spawn.turn, respawn_map_filename,
        NULL);
    if(err)return err;

    /* Attach body to player */
    player->body = body;

    /* Move body to the respawn location */
    err = body_respawn(body, spawn.pos, spawn.rot, spawn.turn, map);
    if(err)return err;

    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_edit_player(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    int player_i = 0;

    if(!fus_lexer_done(lexer) && fus_lexer_got_int(lexer)){
        err = fus_lexer_get_int(lexer, &player_i);
        if(err)goto lexer_err;
    }

    const char *stateset_filename = app->stateset_filename;
    if(!DONE){
        GET_STR_CACHED(stateset_filename, &app->prend.filename_store)
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

static int _test_app_command_save_player(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    int player_i = 0;

    if(!fus_lexer_done(lexer)){
        err = fus_lexer_get_int(lexer, &player_i);
        if(err)goto lexer_err;
    }

    hexgame_t *game = &app->hexgame;
    if(player_i < 0 || player_i >= game->players_len){
        console_write_msg(&app->console, "Player # invalid\n");
        return 0;
    }

    player_t *player = game->players[player_i];

    err = player_use_savepoint(player);
    if(err)return err;

    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_play_recording(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    INIT

    const char *recording_filename = NULL;
    GET_STR_CACHED(recording_filename, &app->prend.filename_store)

    hexgame_t *game = &app->hexgame;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->keymap != 0)continue;

        body_t *body = player->body;
        if(!body){
            fprintf(stderr,
                "Can't play back recording without a body!\n");
            break;
        }

        err = hexmap_load_recording(body->map,
            recording_filename, NULL, true, 0, NULL, NULL);
        if(err)return err;
    }

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
    free(filename);
    return 0;

lexer_err:
    free(filename);
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_dump(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    int dump_bitmaps = 1;
    int dump_what = 0; /* rgraph, prend */
    FILE *file = NULL;
    while(1){
        if(fus_lexer_done(lexer))break;
        else if(fus_lexer_got(lexer, "rgraph"))dump_what = 0;
        else if(fus_lexer_got(lexer, "prend"))dump_what = 1;
        else if(fus_lexer_got(lexer, "nobitmaps"))dump_bitmaps = 0;
        else if(fus_lexer_got(lexer, "surfaces")){
            /* WARNING: doing this with "prend" after "renderall" causes
            my laptop to hang... */
            dump_bitmaps = 2;
        }else if(fus_lexer_got(lexer, "file")){
            err = fus_lexer_next(lexer);
            if(err)goto lexer_err;
            char *filename;
            err = fus_lexer_get_str(lexer, &filename);
            if(err)goto lexer_err;
            file = fopen(filename, "w");
            free(filename);
            if(!file){
                perror("fopen");
                return 1;
            }
        }else goto lexer_err;
        err = fus_lexer_next(lexer);
        if(err)goto lexer_err;
    }
    if(dump_what == 0){
        rendergraph_t *rgraph =
            app->prend.rendergraphs[app->editor.cur_rgraph_i];
        rendergraph_dump(rgraph, file? file: stdout, 0, dump_bitmaps);
    }else if(dump_what == 1){
        prismelrenderer_dump(&app->prend, file? file: stdout, dump_bitmaps);
    }
    if(file != NULL){
        if(fclose(file)){
            perror("fclose");
            return 1;
        }
    }
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_dump_stringstores(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    bool dump_map = true;
    bool dump_minimap = false;
    if(!fus_lexer_done(lexer)){
        if(fus_lexer_got(lexer, "map")){
            err = fus_lexer_next(lexer);
            if(err)goto lexer_err;
            dump_map = true;
            dump_minimap = false;
        }else if(fus_lexer_got(lexer, "minimap")){
            err = fus_lexer_next(lexer);
            if(err)goto lexer_err;
            dump_map = false;
            dump_minimap = true;
        }else if(fus_lexer_got(lexer, "both")){
            err = fus_lexer_next(lexer);
            if(err)goto lexer_err;
            dump_map = true;
            dump_minimap = true;
        }else{
            err = fus_lexer_unexpected(lexer, "map minimap both");
            goto lexer_err;
        }
    }
    if(dump_map){
        fprintf(stderr, "=== MAP:\n");
        prismelrenderer_dump_stringstores(&app->prend, stderr);
    }
    if(dump_minimap){
        fprintf(stderr, "=== MINI MAP:\n");
        prismelrenderer_dump_stringstores(&app->minimap_prend, stderr);
    }
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_map(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    INIT

    const char *mapper_name;
    const char *mapped_rgraph_name;
    const char *resulting_rgraph_name = NULL;
    GET_STR_CACHED(mapper_name, &app->prend.name_store)
    GET_STR_CACHED(mapped_rgraph_name, &app->prend.name_store)
    if(!DONE){
        GET_STR_CACHED(resulting_rgraph_name, &app->prend.name_store)
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
    err = prismelrenderer_render_all_bitmaps(
        &app->prend, app->sdl_palette);
    if(err)return err;
    return 0;
}

static int _test_app_command_get_shape(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    INIT

    const char *name;
    GET_STR_CACHED(name, &app->prend.name_store)
    int rgraph_i = prismelrenderer_get_rgraph_i(
        &app->prend, name);
    if(rgraph_i < 0){
        fprintf(stderr, "Couldn't find shape: %s\n", name);
        return 2;}
    app->editor.cur_rgraph_i = rgraph_i;
    return 0;
lexer_err:
    *lexer_err_ptr = true;
    return 0;
}

static int _test_app_command_mode(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    if(GOT("game") || GOT("g")){
        app->mode = TEST_APP_MODE_GAME;
    }else if(GOT("editor") || GOT("e")){
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

static int _test_app_command_visit_all(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    hexgame_t *game = &app->hexgame;
    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        for(int j = 0; j < map->submaps_len; j++){
            hexmap_submap_t *submap = map->submaps[j];
            submap->visited = true;
        }
    }
    return 0;
}


#define COMMAND(NAME, ALT_NAME, PARAMS) (test_app_command_t){ \
    .name = #NAME, \
    .alt_name = ALT_NAME, \
    .params = PARAMS, \
    .action = &_test_app_command_##NAME \
}
#define NULLCOMMAND (test_app_command_t){0}
test_app_command_t _test_app_commands[] = {
    COMMAND(exit, NULL, NULL),
    COMMAND(help, NULL, NULL),
    COMMAND(cls, NULL, NULL),
    COMMAND(list_worldmaps, "lw", NULL),
    COMMAND(list_maps, "lm", NULL),
    COMMAND(list_submaps, "ls", NULL),
    COMMAND(list_bodies, "lb", NULL),
    COMMAND(list_players, "lp", NULL),
    COMMAND(list_actors, "la", NULL),
    COMMAND(add_player, "ap", "[STATESET]"),
    COMMAND(edit_player, "ep", "[PLAYER_INDEX] [STATESET]"),
    COMMAND(save_player, "sp", "[PLAYER_INDEX]"),
    COMMAND(play_recording, "pc", "[FILENAME]"),
    COMMAND(save, NULL, "[FILENAME]"),
    COMMAND(dump, NULL, "[(rgraph | prend | nobitmaps | surfaces | file FILENAME) ...]"),
    COMMAND(dump_stringstores, "dss", "[map | minimap | both]"),
    COMMAND(map, NULL, "MAPPER RGRAPH [RESULTING_RGRAPH]"),
    COMMAND(renderall, NULL, NULL),
    COMMAND(get_shape, NULL, "SHAPE"),
    COMMAND(mode, "m", "(game | g | editor | e)"),
    COMMAND(visit_all, "va", NULL),
    NULLCOMMAND
};




/****************************
* PUBLIC TEST_APP FUNCTIONS *
****************************/

static int _test_app_process_console_input(test_app_t *app, fus_lexer_t *lexer){
    int err;

    test_app_command_t *command;
    for(command = _test_app_commands; command->name; command++){
        if(
            fus_lexer_got(lexer, command->name) ||
            (command->alt_name != NULL && fus_lexer_got(lexer, command->alt_name))
        ){
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
    test_app_command_write_to_console(command, &app->console);
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

void test_app_write_console_commands(test_app_t *app, const char *prefix){
    for(
        test_app_command_t *command = _test_app_commands;
        command->name;
        command++
    ){
        if(prefix != NULL && !(
            _startswith(command->name, prefix) ||
            (command->alt_name != NULL && _startswith(command->alt_name, prefix))
        )){
            continue;
        }
        console_write_msg(&app->console, " * ");
        test_app_command_write_to_console(command, &app->console);
        console_write_msg(&app->console, "\n");
    }
}
