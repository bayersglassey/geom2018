
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



typedef struct test_app_command {
    const char *name;
    int (*action)(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr);
} test_app_command_t;



static int _test_app_command_help(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr);



static int _test_app_command_exit(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    app->loop = false;
    return 0;
}

static int _test_app_command_cls(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    console_clear(&app->console);
    return 0;
}

static int _test_app_command_run(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    console_write_msg(&app->console, "Try F5\n");
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



test_app_command_t _test_app_commands[] = {
    {"exit", &_test_app_command_exit},
    {"help", &_test_app_command_help},
    {"cls", &_test_app_command_cls},
    {"run", &_test_app_command_run},
    {"add_player", &_test_app_command_add_player},
    {"save", &_test_app_command_save},
    {"dump", &_test_app_command_dump},
    {"map", &_test_app_command_map},
    {"renderall", &_test_app_command_renderall},
    {"get_shape", &_test_app_command_get_shape},
    {NULL, NULL},
};

static int _test_app_command_help(test_app_t *app, fus_lexer_t *lexer, bool *lexer_err_ptr){
    console_write_msg(&app->console, "Commands:\n");
    for(test_app_command_t *command = _test_app_commands; command->name; command++){
        console_write_msg(&app->console, " * ");
        console_write_msg(&app->console, command->name);
        console_write_msg(&app->console, "\n");
    }
    return 0;
}



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
    console_write_msg(&app->console, "Couldn't parse that\n");
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

