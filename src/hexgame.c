

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexmap.h"
#include "hexspace.h"
#include "prismelrenderer.h"
#include "array.h"
#include "util.h"
#include "location.h"
#include "lexer.h"
#include "lexer_macros.h"



/**********
 * CAMERA *
 **********/

void camera_cleanup(camera_t *camera){
    /* Nuthin */
}

int camera_init(camera_t *camera, hexgame_t *game, hexmap_t *map,
    body_t *body
){
    camera->game = game;
    camera->map = map;
    camera->cur_submap = NULL;
    camera->body = body;

    camera->follow = false;
    camera->smooth_scroll = true;
    camera->mapper = NULL;

    memset(camera->colors, 0, sizeof(camera->colors));
    camera->colors_fade = 0;

    camera_set(camera, (vec_t){0}, 0);
    return 0;
}

void camera_set(camera_t *camera, vec_t pos,
    rot_t rot
){
    vecspace_t *space = camera->map->space;
    vec_cpy(space->dims, camera->pos, pos);
    vec_cpy(space->dims, camera->scrollpos, pos);
    camera->rot = rot;
    camera->should_reset = true;
}

void camera_set_body(camera_t *camera, body_t *body){
    camera->body = body;
    camera->mapper = NULL;
}

void camera_colors_flash(camera_t *camera, Uint8 r, Uint8 g, Uint8 b,
    int percent
){
    for(int i = 0; i < 256; i++){
        SDL_Color *c = &camera->colors[i];
        interpolate_color(c, r, g, b, percent, 100);
    }
    camera->colors_fade = 0;
}

void camera_colors_flash_white(camera_t *camera, int percent){
    camera_colors_flash(camera, 255, 255, 255, percent);
}

static int camera_colors_step(camera_t *camera, palette_t *palette){
    int err;
    if(camera->cur_submap != NULL){
        err = palette_update_colors(palette, camera->colors,
            camera->colors_fade, HEXGAME_MAX_COLORS_FADE);
        if(err)return err;
        if(camera->colors_fade < HEXGAME_MAX_COLORS_FADE)camera->colors_fade++;
    }
    return 0;
}

int camera_step(camera_t *camera){
    int err;

    hexgame_t *game = camera->game;
    body_t *body = camera->body;

    /* Figure out camera's current submap */
    if(body != NULL){
        camera->map = body->map;
        if(camera->cur_submap != body->cur_submap){
            camera->cur_submap = body->cur_submap;

#ifndef DONT_ANIMATE_PALETTE
            if(camera->smooth_scroll && !camera->should_reset){
                /* Smoothly transition between old & new palettes */
                camera->colors_fade = 0;
            }
#endif

            /* camera->cur_submap could be NULL if it's following a weird
            body like a coin... */
            if(camera->cur_submap != NULL){
                err = palette_reset(&camera->cur_submap->palette);
                if(err)return err;
            }
        }
    }

    hexmap_t *map = camera->map;
    vecspace_t *space = map->space;

#ifndef DONT_ANIMATE_PALETTE
    /* Animate palette */
    /* NOTE: compiling with -DDONT_ANIMATE_PALETTE is useful for producing
    animated GIF gameplay clips, since our palette changes are too subtle
    for my GIF capturing software (byzanz), leading to periodic flashes of
    different colours. */

    /* NOTE: The following call to palette_step shouldn't be in here.
    This means palettes are updated once for every camera which is looking
    at them, which makes no sense, but will work ok so long as we only
    have 1 camera. */
    if(camera->cur_submap != NULL){
        err = palette_step(&camera->cur_submap->palette);
        if(err)return err;
    }
#endif

    if(camera->cur_submap != NULL){
        err = camera_colors_step(camera, &camera->cur_submap->palette);
        if(err)return err;
    }

    /* Set camera */
    int camera_type = -1;
    if(
        camera->follow ||
        (body && body->state && body->state->flying)
    ){
        camera_type = 1;
    }else if(camera->cur_submap != NULL){
        camera_type = camera->cur_submap->camera_type;
    }
    if(camera_type == 0){
        /* camera determined by submap */
        if(camera->cur_submap != NULL){
            vec_cpy(space->dims, camera->pos,
                camera->cur_submap->camera_pos);
        }
        camera->rot = 0;
    }else if(camera_type == 1){
        /* body-following camera */
        if(body != NULL){
            vec_cpy(space->dims, camera->pos,
                body->pos);
            camera->rot = body->rot;
        }
    }

    /* Scroll renderpos */
    if(camera->smooth_scroll && !camera->should_reset){
        vec_ptr_t scrollpos = camera->scrollpos;

        vec_t target_scrollpos;
        vec_cpy(space->dims, target_scrollpos, camera->pos);

        vec_t diff;
        vec_cpy(space->dims, diff, target_scrollpos);
        vec_sub(space->dims, diff, scrollpos);

        rot_t rot;
        int dist, angle;
        hexspace_angle(diff, &rot, &dist, &angle);

        if(dist > 0){
            vec_t add;
            if(angle > 0){
                /* X + R X */
                hexspace_set(add, 2, 1);
            }else{
                /* X */
                hexspace_set(add, 1, 0);
            }
            hexspace_rot(add, rot);
            vec_add(space->dims, scrollpos, add);
        }
    }else{
        if(camera->should_reset){
            vec_cpy(space->dims, camera->scrollpos, camera->pos);
            camera->should_reset = false;
        }
    }

    return 0;
}

int camera_render(camera_t *camera,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom
){
    int err;

    err = update_sdl_palette(pal, camera->colors);
    if(err)return err;

    hexgame_t *game = camera->game;
    hexmap_t *map = camera->map;
    vecspace_t *space = map->space;

    vec_t camera_renderpos;
    vec4_vec_from_hexspace(camera_renderpos, camera->scrollpos);

    /* Figure out which prismelmapper to use when rendering */
    prismelmapper_t *mapper = NULL;
    if(camera->mapper != NULL){
        mapper = camera->mapper;
    }else if(camera->cur_submap != NULL){
        mapper = camera->cur_submap->mapper;
    }

    /* Render map's submaps */
    for(int i = 0; i < map->submaps_len; i++){
        hexmap_submap_t *submap = map->submaps[i];
        rendergraph_t *rgraph = submap->rgraph_map;

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(submap != camera->cur_submap)continue;
#endif

        vec_t pos;
        vec4_vec_from_hexspace(pos, submap->pos);
        vec_sub(rgraph->space->dims, pos, camera_renderpos);
        vec_mul(rgraph->space, pos, camera->map->unit);

        rot_t rot = vec4_rot_from_hexspace(0);
        //rot_t rot = vec4_rot_from_hexspace(
        //    rot_inv(space->rot_max, camera->rot));
        flip_t flip = false;
        int frame_i = game->frame_i;

        err = rendergraph_render(rgraph, renderer, surface,
            pal, camera->map->prend,
            x0, y0, zoom,
            pos, rot, flip, frame_i, mapper);
        if(err)return err;
    }

    /* Render map's bodies */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];

#ifdef GEOM_ONLY_RENDER_CUR_SUBMAP
        if(body->cur_submap != camera->cur_submap)continue;
#endif

        err = body_render(body,
            renderer, surface,
            pal, x0, y0, zoom,
            map, camera_renderpos, mapper);
        if(err)return err;
    }

    return 0;
}


/***********
 * HEXGAME *
 ***********/

void hexgame_cleanup(hexgame_t *game){
    ARRAY_FREE_PTR(char*, game->worldmaps, (void))
    ARRAY_FREE_PTR(hexmap_t*, game->maps, hexmap_cleanup)
    ARRAY_FREE_PTR(camera_t*, game->cameras, camera_cleanup)
    ARRAY_FREE_PTR(player_t*, game->players, player_cleanup)
    ARRAY_FREE_PTR(actor_t*, game->actors, actor_cleanup)
}

static int hexgame_load_worldmaps(hexgame_t *game, const char *worldmaps_filename){
    int err;
    fus_lexer_t _lexer, *lexer = &_lexer;

    char *text = load_file(worldmaps_filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(lexer, text, worldmaps_filename);
    if(err)return err;

    GET("worldmaps")
    GET("(")
    while(1){
        if(GOT(")"))break;
        GET("(")
        char *worldmap;
        GET_STR(worldmap)
        ARRAY_PUSH(char*, game->worldmaps, worldmap)
        GET(")")
    }
    NEXT

    fus_lexer_cleanup(lexer);

    free(text);
    return 0;
}

int hexgame_init(hexgame_t *game, prismelrenderer_t *prend,
    const char *worldmaps_filename,
    const char *map_filename, void *app,
    new_game_callback_t *new_game_callback,
    continue_callback_t *continue_callback,
    set_players_callback_t *set_players_callback,
    exit_callback_t *exit_callback
){
    int err;

    game->frame_i = 0;
    game->prend = prend;
    game->space = &hexspace; /* NOT the same as prend->space! */
    game->app = app;
    game->new_game_callback = new_game_callback;
    game->continue_callback = continue_callback;
    game->set_players_callback = set_players_callback;
    game->exit_callback = exit_callback;

    ARRAY_INIT(game->worldmaps)
    ARRAY_INIT(game->maps)
    ARRAY_INIT(game->cameras)
    ARRAY_INIT(game->players)
    ARRAY_INIT(game->actors)

    err = hexgame_load_worldmaps(game, worldmaps_filename);
    if(err)return err;

    if(game->worldmaps_len == 0){
        fprintf(stderr, "No worldmaps specified!\n");
        return 2;
    }

    /* Default map to load is first worldmap */
    if(map_filename == NULL)map_filename = game->worldmaps[0];

    hexmap_t *map;
    err = hexgame_load_map(game, map_filename, &map);
    if(err)return err;

    return 0;
}

static const char *hexgame_get_worldmap(hexgame_t *game, const char *filename){
    for(int i = 0; i < game->worldmaps_len; i++){
        const char *worldmap = game->worldmaps[i];
        if(!strcmp(filename, worldmap))return worldmap;
    }
    return NULL;
}

int hexgame_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr
){
    int err;
    const char *worldmap = hexgame_get_worldmap(game, map_filename);
    if(worldmap == NULL){
        fprintf(stderr, "Tried to load a map which wasn't declared as a worldmap: %s\n", map_filename);
        return 2;
    }

    ARRAY_PUSH_NEW(hexmap_t*, game->maps, map)
    err = hexmap_load(map, game, map_filename, NULL);
    if(err)return err;
    *map_ptr = map;
    return 0;
}

int hexgame_get_or_load_map(hexgame_t *game, const char *map_filename,
    hexmap_t **map_ptr
){
    int err;

    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        if(!strcmp(map->name, map_filename)){
            *map_ptr = map;
            return 0;
        }
    }

    return hexgame_load_map(game, map_filename, map_ptr);
}

int hexgame_get_map_index(hexgame_t *game, hexmap_t *map){
    for(int i = 0; i < game->maps_len; i++){
        if(game->maps[i] == map)return i;
    }
    return -1;
}

int hexgame_reset_player(hexgame_t *game, player_t *player,
    int reset_level, hexmap_t *reset_map
){
    /* reset_level:
    RESET_TO_SAFETY is to player->safe_location, RESET_SOFT is to
    player->respawn_location, RESET_HARD is to start of game. */

    /* WARNING: this function calls body_respawn, which calls
    body_move_to_map, which modifies map->bodies for two maps.
    So if caller is trying to loop over map->bodies in the usual way,
    the behaviour of that loop is probably gonna be super wrong. */

    int err;
    body_t *body = player->body;

    /* If no body, do nothing */
    if(!body)return 0;

    vecspace_t *space = game->space;
    hexmap_t *map = NULL;
    vec_ptr_t pos = NULL;
    rot_t rot = 0;
    bool turn = false;

    if(reset_level == RESET_HARD){
        map = reset_map? reset_map: game->maps[0];
        pos = map->spawn;
    }else{
        location_t *location = reset_level == RESET_SOFT?
            &player->respawn_location:
            &player->safe_location;

        /* Don't we have a function which wraps the following these days?
        player_reload -- does it differ from this at all? */
        err = hexgame_get_or_load_map(game, location->map_filename,
            &map);
        if(err)return err;
        pos = location->pos;
        rot = location->rot;
        turn = location->turn;
    }

    err = body_respawn(body, pos, rot, turn, map);
    if(err)return err;

    body_reset_cameras(body);
    body_flash_cameras(body, 255, 255, 255, 30);
    return 0;
}

static player_t *hexgame_get_player_by_keymap(hexgame_t *game, int keymap){
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        if(player->keymap == keymap)return player;
    }
    return NULL;
}

int hexgame_reset_player_by_keymap(hexgame_t *game, int keymap,
    int reset_level, hexmap_t *reset_map
){
    int err;
    player_t *player = hexgame_get_player_by_keymap(game, keymap);
    if(!player)return 2;
    err = hexgame_reset_player(game, player, reset_level, reset_map);
    if(err)return err;
    return 0;
}

int hexgame_reset_players(hexgame_t *game, int reset_level,
    hexmap_t *reset_map
){
    int err;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = hexgame_reset_player(game, player, reset_level, reset_map);
        if(err)return err;
    }
    return 0;
}

int hexgame_process_event(hexgame_t *game, SDL_Event *event){
    int err;
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_process_event(player, event);
        if(err)return err;
    }
    return 0;
}

int hexgame_step(hexgame_t *game){
    int err;

    game->frame_i++;
    if(game->frame_i == MAX_FRAME_I)game->frame_i = 0;

    /* Reset all camera->mapper for this step */
    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        camera->mapper = NULL;
    }

    /* Do 1 gameplay step for each actor */
    for(int i = 0; i < game->actors_len; i++){
        actor_t *actor = game->actors[i];
        err = actor_step(actor, game);
        if(err)return err;
    }

    /* Do 1 gameplay step for each map */
    for(int i = 0; i < game->maps_len; i++){
        hexmap_t *map = game->maps[i];
        err = hexmap_step(map);
        if(err)return err;
    }

    /* Do 1 gameplay step for each player */
    for(int i = 0; i < game->players_len; i++){
        player_t *player = game->players[i];
        err = player_step(player, game);
        if(err)return err;
    }

    return 0;
}

int hexgame_step_cameras(hexgame_t *game){
    int err;

    /* Do 1 step for each camera */
    for(int i = 0; i < game->cameras_len; i++){
        camera_t *camera = game->cameras[i];
        err = camera_step(camera);
        if(err)return err;
    }

    return 0;
}
