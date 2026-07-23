
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "console.h"
#include "test_app.h"
#include "test_app_list.h"
#include "lexer_macros.h"
#include "hexgame_state.h"



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
    app->state = TEST_APP_STATE_QUIT;
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
        if(!strcmp(worldmap, map->filename)){
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
    body_t *body = app->camera->body;
    player_t *player = body_get_player(body);
    int player_i = player? player_get_index(player): -1;
    if(player_i < 0)player_i = 0;
    return test_app_open_list_players(app, player_i);
}

static int _test_app_command_list_actors(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    body_t *body = app->camera->body;
    actor_t *actor = body_get_actor(body);
    int actor_i = actor? actor_get_index(actor): -1;
    if(actor_i < 0)actor_i = 0;
    return test_app_open_list_actors(app, actor_i);
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
        GET_STR(stateset_filename)
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

static int _test_app_command_save(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    const char *filename = NULL;

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

static int _test_app_command_map(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    INIT

    const char *mapper_name;
    const char *mapped_rgraph_name;
    const char *resulting_rgraph_name = NULL;
    GET_STR(mapper_name)
    GET_STR(mapped_rgraph_name)
    if(!DONE){
        GET_STR(resulting_rgraph_name)
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

static int _test_app_command_get_shape(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    INIT

    const char *name;
    GET_STR(name)
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
            hexmap_submap_visit(submap);
        }
    }
    return 0;
}

static int _test_app_command_save_recording_filename(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    const char *filename;
    GET_STR(filename)
    app->save_recording_filename = filename;
    return 0;
}

static int _test_app_command_load_recording_filename(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    const char *filename;
    GET_STR(filename)
    app->load_recording_filename = filename;
    return 0;
}

static int _test_app_command_apply_effect(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    int err;
    int player_i = 0;

    if(GOT_INT){
        err = fus_lexer_get_int(lexer, &player_i);
        if(err)goto lexer_err;
    }

    hexgame_t *game = &app->hexgame;
    if(player_i < 0 || player_i >= game->players_len){
        console_write_msg(&app->console, "Player # invalid\n");
        return 0;
    }

    player_t *player = game->players[player_i];
    body_t *body = player->body;
    if(!body){
        console_printf(&app->console, "Player #%i has no body\n", player_i);
        return 0;
    }
    stateset_t *stateset = body->stateset;
    if(!stateset){
        console_printf(&app->console, "Player #%i's body has no stateset\n", player_i);
        return 0;
    }

    state_effect_t effect;
    err = state_effect_parse(&effect, stateset->root_context, lexer,
        game->prend, game->space);
    if(err == 2){
        /* NOTE: if we couldn't fully parse the effect, we don't attempt to
        do cleanup of it, since state_effect_parse makes no guarantees about
        leaving the effect in a valid state if parsing fails!.. */
        goto lexer_err;
    }else if(err)return err;
    if(!fus_lexer_done(lexer)){
        console_write_msg(&app->console, "Extra input\n");
        state_effect_cleanup(&effect);
        goto lexer_err;
    }

    /* NOTE: whenever err == 2, we return 0.
    In some cases, this is useful, like if you try to call a nonexistent
    proc; but in other cases, the error may have actually resulted in
    corrupted game data, and we should really let the error "bubble up".
    Ah well, maybe some day we'll introduce err == 3 for recoverable
    errors, or something. (Not likely!) */
    hexgame_state_context_t _context = {
        .game = body->game,
        .body = body,
    }, *context=&_context;
    hexgame_state_controlflow_t controlflow;
    hexgame_state_controlflow_init(&controlflow, false);
    state_effect_goto_t *gotto = NULL;
    err = state_effect_apply(&effect, context, &gotto, &controlflow);
    if(err == 2){
        console_write_msg(&app->console, "Error applying effect!\n");
        goto cleanup_effect;
    }else if(err)return err;
    if(gotto != NULL){
        err = state_effect_goto_apply_to_body(gotto, body);
        if(err == 2){
            console_printf(&app->console, "Error applying goto: \"%s\"\n", gotto->name);
            goto cleanup_effect;
        }else if(err)return err;
        if(gotto->immediate){
            /* If there was an "immediate goto" effect,
            then we immediately handle the new state's rules */
            err = body_handle_rules(context->body, context->your_body);
            if(err == 2){
                console_printf(&app->console, "Error applying goto immediate: \"%s\"\n", gotto->name);
                goto cleanup_effect;
            }else if(err)return err;
        }
    }

    return 0;
cleanup_effect:
    state_effect_cleanup(&effect);
    return 0;
lexer_err:
    *lexer_err_ptr = true;
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
    COMMAND(exit, "x", NULL),
    COMMAND(help, "h", NULL),
    COMMAND(cls, NULL, NULL),
    COMMAND(list_worldmaps, "lw", NULL),
    COMMAND(list_maps, "lm", NULL),
    COMMAND(list_submaps, "ls", NULL),
    COMMAND(list_bodies, "lb", NULL),
    COMMAND(list_players, "lp", NULL),
    COMMAND(list_actors, "la", NULL),
    COMMAND(edit_player, "ep", "[PLAYER_INDEX] [STATESET]"),
    COMMAND(save_player, "sp", "[PLAYER_INDEX]"),
    COMMAND(save, NULL, "[FILENAME]"),
    COMMAND(map, NULL, "MAPPER RGRAPH [RESULTING_RGRAPH]"),
    COMMAND(get_shape, NULL, "SHAPE"),
    COMMAND(mode, "m", "(game | g | editor | e)"),
    COMMAND(visit_all, "va", NULL),
    COMMAND(save_recording_filename, "srf", NULL),
    COMMAND(load_recording_filename, "lrf", NULL),
    COMMAND(apply_effect, "ae", "[PLAYER_INDEX] EFFECT"),
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

    fprintf(stderr, "Processing console input: %s\n", app->console.input);

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
