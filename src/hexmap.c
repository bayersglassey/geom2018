

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "hexmap.h"
#include "hexgame.h"
#include "lexer.h"
#include "util.h"
#include "mathutil.h"
#include "geom.h"
#include "vec4.h"
#include "hexspace.h"
#include "location.h"
#include "prismelrenderer.h"


const char *hexmap_door_type_msg(int door_type){
    switch(door_type){
        case HEXMAP_DOOR_TYPE_DUD: return "Dud";
        case HEXMAP_DOOR_TYPE_RESPAWN: return "Respawn";
        case HEXMAP_DOOR_TYPE_NEW_GAME: return "New Game";
        case HEXMAP_DOOR_TYPE_CONTINUE: return "Continue";
        case HEXMAP_DOOR_TYPE_PLAYERS: return "Players";
        case HEXMAP_DOOR_TYPE_EXIT: return "Exit";
        case HEXMAP_DOOR_TYPE_CAMERA_MAPPER: return "Camera Mapper";
        default: return "Unknown";
    }
}


/******************
 * HEXMAP TILESET *
 ******************/

void hexmap_tileset_cleanup(hexmap_tileset_t *tileset){
    free(tileset->name);
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->vert_entries, (void))
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->edge_entries, (void))
    ARRAY_FREE_PTR(hexmap_tileset_entry_t*, tileset->face_entries, (void))
}

int hexmap_tileset_init(hexmap_tileset_t *tileset, char *name){
    tileset->name = name;
    ARRAY_INIT(tileset->vert_entries)
    ARRAY_INIT(tileset->edge_entries)
    ARRAY_INIT(tileset->face_entries)
    return 0;
}

static int hexmap_tileset_parse(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer
){
    int err;

    err = hexmap_tileset_init(tileset, name);
    if(err)return err;

    /* parse vert, edge, face, rgraphs */
    #define GET_RGRAPH(TYPE) { \
        err = fus_lexer_get(lexer, #TYPE"s"); \
        if(err)return err; \
        err = fus_lexer_get(lexer, "("); \
        if(err)return err; \
        while(1){ \
            if(fus_lexer_got(lexer, ")"))break; \
            \
            char tile_c; \
            err = fus_lexer_get_chr(lexer, &tile_c); \
            if(err)return err; \
            \
            err = fus_lexer_get(lexer, "("); \
            if(err)return err; \
            ARRAY_PUSH_NEW(hexmap_tileset_entry_t*, \
                tileset->TYPE##_entries, entry) \
            entry->n_rgraphs = 0; \
            entry->tile_c = tile_c; \
            entry->frame_offset = 0; \
            if(fus_lexer_got(lexer, "frame_offset")){ \
                err = fus_lexer_next(lexer); \
                if(err)return err; \
                err = fus_lexer_get(lexer, "("); \
                if(err)return err; \
                err = fus_lexer_get_int(lexer, &entry->frame_offset); \
                if(err)return err; \
                err = fus_lexer_get(lexer, ")"); \
                if(err)return err; \
            } \
            while(1){ \
                if(!fus_lexer_got_str(lexer))break; \
                char *rgraph_name; \
                err = fus_lexer_get_str(lexer, &rgraph_name); \
                if(err)return err; \
                rendergraph_t *rgraph = \
                    prismelrenderer_get_rendergraph(prend, rgraph_name); \
                if(rgraph == NULL){ \
                    fus_lexer_err_info(lexer); \
                    fprintf(stderr, "Couldn't find shape: %s\n", \
                        rgraph_name); \
                    free(rgraph_name); return 2;} \
                free(rgraph_name); \
                entry->rgraphs[entry->n_rgraphs] = rgraph; \
                entry->n_rgraphs++; \
            } \
            if(entry->n_rgraphs == 0){ \
                return fus_lexer_unexpected(lexer, "str");} \
            err = fus_lexer_get(lexer, ")"); \
            if(err)return err; \
        } \
        err = fus_lexer_next(lexer); \
        if(err)return err; \
    }
    GET_RGRAPH(vert)
    GET_RGRAPH(edge)
    GET_RGRAPH(face)
    #undef GET_RGRAPH

    return 0;
}

int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename, vars_t *vars
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = hexmap_tileset_parse(tileset, prend, strdup(filename),
        &lexer);
    if(err)return err;

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}


/**********
 * HEXMAP *
 **********/

void body_cleanup(struct body *body);
void hexmap_cleanup(hexmap_t *map){
    free(map->name);

    ARRAY_FREE_PTR(body_t*, map->bodies, body_cleanup)

    ARRAY_FREE_PTR(hexmap_submap_t*, map->submaps, hexmap_submap_cleanup)
    ARRAY_FREE_PTR(hexmap_recording_t*, map->recordings,
        hexmap_recording_cleanup)
}

int hexmap_init(hexmap_t *map, hexgame_t *game, char *name,
    vec_t unit
){
    int err;

    prismelrenderer_t *prend = game->prend;
    vecspace_t *space = &hexspace;

    map->name = name;
    map->game = game;
    map->space = space;
    vec_zero(map->spawn);

    map->prend = prend;
    vec_cpy(prend->space->dims, map->unit, unit);

    ARRAY_INIT(map->bodies)
    ARRAY_INIT(map->submaps)
    ARRAY_INIT(map->recordings)
    return 0;
}

static int hexmap_parse_recording(fus_lexer_t *lexer,
    char **filename_ptr, char **palmapper_name_ptr, int *frame_offset_ptr
){
    int err;

    err = fus_lexer_get(lexer, "(");
    if(err)return err;

    err = fus_lexer_get_str(lexer, filename_ptr);
    if(err)return err;
    if(!fus_lexer_got(lexer, ")")){
        if(fus_lexer_got(lexer, "empty")){
            err = fus_lexer_next(lexer);
            if(err)return err;
        }else{
            err = fus_lexer_get_str(lexer, palmapper_name_ptr);
            if(err)return err;
        }
        if(fus_lexer_got_int(lexer)){
            err = fus_lexer_get_int(lexer, frame_offset_ptr);
            if(err)return err;
        }
    }

    err = fus_lexer_get(lexer, ")");
    if(err)return err;
    return 0;
}

static int hexmap_load_hexmap_recording(
    hexmap_t *map, hexmap_submap_t *submap, hexgame_t *game,
    hexmap_recording_t *recording
){
    /* NOTE: submap may be NULL! */
    int err;
    vecspace_t *space = map->space;

    /* Get palmapper */
    palettemapper_t *palmapper = NULL;
    if(recording->palmapper_name != NULL){
        palmapper = prismelrenderer_get_palmapper(
            game->prend, recording->palmapper_name);
        if(palmapper == NULL){
            fprintf(stderr, "Couldn't find palmapper: %s\n",
                recording->palmapper_name);
            return 2;
        }
    }

    if(recording->type == HEXMAP_RECORDING_TYPE_RECORDING){
        trf_t trf = {0};
        trf_cpy(space, &trf, &recording->trf);
        if(submap)vec_add(space->dims, trf.add, submap->pos);

        err = hexmap_load_recording(map, recording->filename,
            palmapper, true, recording->frame_offset, &trf);
        if(err)return err;
    }else if(recording->type == HEXMAP_RECORDING_TYPE_ACTOR){
        ARRAY_PUSH_NEW(body_t*, map->bodies, body)
        err = body_init(body, game, map, NULL, NULL, palmapper);
        if(err)return err;

        ARRAY_PUSH_NEW(actor_t*, game->actors, actor)
        err = actor_init(actor, map, body, recording->filename, NULL);
        if(err)return err;

        /* MAYBE TODO: add actor->pos0/rot0/turn0, so that we can
        instantiate actors at different places in various submaps,
        by setting actor->pos0/rot0/turn0 here based on submap->pos
        and recording->trf. */
        /* MAYBE TODO: actor needs some way to make use of recording->frame_offset? */
    }else{
        fprintf(stderr, "%s: Unrecognized hexmap recording type: %i\n",
            __func__, recording->type);
        return 2;
    }

    return 0;
}

int hexmap_load(hexmap_t *map, hexgame_t *game, const char *filename,
    vars_t *vars
){
    int err;
    fus_lexer_t lexer;

    fprintf(stderr, "Loading map: %s\n", filename);

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init_with_vars(&lexer, text, filename, vars);
    if(err)return err;

    err = hexmap_parse(map, game, strdup(filename), &lexer);
    if(err)return err;

    /* Load recordings & actors */
    for(int i = 0; i < map->recordings_len; i++){
        hexmap_recording_t *recording = map->recordings[i];
        err = hexmap_load_hexmap_recording(map, NULL, game, recording);
        if(err)return err;
    }
    for(int j = 0; j < map->submaps_len; j++){
        hexmap_submap_t *submap = map->submaps[j];
        for(int i = 0; i < submap->collmap.recordings_len; i++){
            hexmap_recording_t *recording = submap->collmap.recordings[i];
            err = hexmap_load_hexmap_recording(map, submap, game, recording);
            if(err)return err;
        }
    }

    fus_lexer_cleanup(&lexer);

    free(text);
    return 0;
}

int hexmap_parse(hexmap_t *map, hexgame_t *game, char *name,
    fus_lexer_t *lexer
){
    int err;

    prismelrenderer_t *prend = game->prend;

    /* parse unit */
    vec_t unit;
    err = fus_lexer_get(lexer, "unit");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    for(int i = 0; i < prend->space->dims; i++){
        err = fus_lexer_get_int(lexer, &unit[i]);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* init the map */
    err = hexmap_init(map, game, name, unit);
    if(err)return err;

    /* parse spawn point */
    char *spawn_filename = NULL;
    err = fus_lexer_get(lexer, "spawn");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    if(fus_lexer_got_str(lexer)){
        err = fus_lexer_get_str(lexer, &spawn_filename);
        if(err)return err;
    }else{
        err = fus_lexer_get_vec(lexer, map->space, map->spawn);
        if(err)return err;
    }
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* default palette */
    char *default_palette_filename;
    err = fus_lexer_get(lexer, "default_palette");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &default_palette_filename);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;

    /* default tileset */
    char *default_tileset_filename;
    err = fus_lexer_get(lexer, "default_tileset");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    err = fus_lexer_get_str(lexer, &default_tileset_filename);
    if(err)return err;
    err = fus_lexer_get(lexer, ")");
    if(err)return err;


    /* parse actors */
    if(fus_lexer_got(lexer, "actors")){
    err = fus_lexer_next(lexer);
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;

        char *filename;
        char *palmapper_name = NULL;
        int frame_offset = 0;
        err = hexmap_parse_recording(lexer,
            &filename, &palmapper_name, &frame_offset);
        if(err)return err;

        ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
            recording)
        err = hexmap_recording_init(recording,
            HEXMAP_RECORDING_TYPE_ACTOR,
            filename, palmapper_name, frame_offset);
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;
    }


    /* parse submaps */
    err = fus_lexer_get(lexer, "submaps");
    if(err)return err;
    err = fus_lexer_get(lexer, "(");
    if(err)return err;
    while(1){
        if(fus_lexer_got(lexer, ")"))break;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = hexmap_parse_submap(map, lexer, true,
            (vec_t){0}, (vec_t){0}, 0, NULL,
            default_palette_filename, default_tileset_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }
    err = fus_lexer_next(lexer);
    if(err)return err;


    /* maybe get spawn point from a submap */
    if(spawn_filename != NULL){
        hexmap_submap_t *spawn_submap = NULL;
        for(int i = 0; i < map->submaps_len; i++){
            hexmap_submap_t *submap = map->submaps[i];
            if(strcmp(submap->filename, spawn_filename) == 0){
                spawn_submap = submap; break;}
        }
        if(spawn_submap == NULL){
            fprintf(stderr, "Couldn't find submap with filename: %s\n",
                spawn_filename);
            return 2;}
        vec_cpy(map->space->dims, map->spawn, spawn_submap->pos);
        free(spawn_filename);
    }


    /* pheeew */
    return 0;
}

static int hexmap_parse_door(hexmap_t *map, hexmap_submap_t *submap,
    hexmap_door_t *door, fus_lexer_t *lexer
){
    /* We assume door's memory starts off zero'd, e.g. it was calloc'd
    by ARRAY_PUSH_NEW */
    int err;

    vecspace_t *space = map->space;

    if(fus_lexer_got(lexer, "dud")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_DUD;
    }else if(fus_lexer_got(lexer, "new_game")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_NEW_GAME;
        err = fus_lexer_get_str(lexer, &door->u.location.map_filename);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "continue")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_CONTINUE;
    }else if(fus_lexer_got(lexer, "players")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_PLAYERS;
        err = fus_lexer_get_int(lexer, &door->u.n_players);
        if(err)return err;
    }else if(fus_lexer_got(lexer, "exit")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        door->type = HEXMAP_DOOR_TYPE_EXIT;
    }else if(fus_lexer_got(lexer, "camera_mapper")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        char *s;
        {
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &s);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        door->type = HEXMAP_DOOR_TYPE_CAMERA_MAPPER;
        door->u.s = s;
    }else{
        door->type = HEXMAP_DOOR_TYPE_RESPAWN;

        if(fus_lexer_got(lexer, "map")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &door->u.location.map_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }else{
            /* Non-null door->u.location.map_filename indicates player should
            "teleport" to door->u.location */
            door->u.location.map_filename = strdup(map->name);
        }

        if(fus_lexer_got(lexer, "anim")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_str(lexer, &door->u.location.anim_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }

        err = fus_lexer_get(lexer, "pos");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_vec(lexer, space, door->u.location.pos);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        err = fus_lexer_get(lexer, "rot");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_int(lexer, &door->u.location.rot);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        err = fus_lexer_get(lexer, "turn");
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_yn(lexer, &door->u.location.turn);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    return 0;
}

static int hexmap_populate_submap_doors(hexmap_t *map,
    hexmap_submap_t *submap
){
    /* Scan submap's collmap for door tiles, and link the corresponding
    doors to them.
    (The "corresponding" door is simply the next one found, as we iterate
    through the tiles from top-left to bottom-right.) */
    int err;

    int n_doors = 0;
    hexcollmap_t *collmap = &submap->collmap;
    int w = collmap->w;
    int h = collmap->h;
    for(int y = 0; y < h; y++){
        for(int x = 0; x < w; x++){
            hexcollmap_tile_t *tile = &collmap->tiles[y * w + x];
            for(int i = 0; i < 2; i++){
                hexcollmap_elem_t *elem = &tile->face[i];
                if(elem->tile_c != 'D')continue;
                if(n_doors < submap->doors_len){
                    hexmap_door_t *door = submap->doors[n_doors];
                    door->elem = elem;
                }
                n_doors++;
            }
        }
    }

    if(n_doors != submap->doors_len){
        fprintf(stderr,
            "Map (%s) and collmap (%s) disagree on number of doors: "
            "%i != %i\n",
            map->name, submap->filename,
            submap->doors_len, n_doors);
        return 2;
    }
    return 0;
}

int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer, bool solid,
    vec_t parent_pos, vec_t parent_camera_pos, int parent_camera_type,
    prismelmapper_t *parent_mapper, char *palette_filename,
    char *tileset_filename
){
    int err;
    vecspace_t *space = map->space;
    prismelrenderer_t *prend = map->prend;

    if(fus_lexer_got(lexer, "skip")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_parse_silent(lexer);
        if(err)return err;
        return 0;
    }

    if(fus_lexer_got(lexer, "bg")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        solid = false;
    }

    char *submap_filename = NULL;
    if(fus_lexer_got(lexer, "file")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &submap_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    vec_t pos = {0};
    if(fus_lexer_got(lexer, "pos")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_vec(lexer, space, pos);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }
    vec_add(space->dims, pos, parent_pos);

    int camera_type = parent_camera_type;
    vec_t camera_pos;
    vec_cpy(space->dims, camera_pos, parent_camera_pos);
    if(fus_lexer_got(lexer, "camera")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        if(fus_lexer_got(lexer, "follow")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            camera_type = 1;
        }else{
            err = fus_lexer_get_vec(lexer, space, camera_pos);
            if(err)return err;
            vec_add(space->dims, camera_pos, pos);
        }
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    prismelmapper_t *mapper = parent_mapper;
    if(fus_lexer_got(lexer, "mapper")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_mapper(lexer, map->prend, NULL, &mapper);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "palette")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &palette_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "tileset")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &tileset_filename);
        if(err)return err;
        err = fus_lexer_get(lexer, ")");
        if(err)return err;
    }

    if(submap_filename != NULL){
        fprintf(stderr, "Loading submap: %s\n", submap_filename);

        ARRAY_PUSH_NEW(hexmap_submap_t*, map->submaps, submap)
        err = hexmap_submap_init(map, submap, strdup(submap_filename),
            solid, pos, camera_type, camera_pos, mapper,
            palette_filename, tileset_filename);
        if(err)return err;

        /* load collmap */
        err = hexcollmap_load(&submap->collmap, submap_filename,
            lexer->vars);
        if(err)return err;

        /* render submap->rgraph_map, submap->rgraph_minimap */
        err = hexmap_submap_create_rgraph_map(submap);
        if(err)return err;
        err = hexmap_submap_create_rgraph_minimap(submap);
        if(err)return err;

        /* parse doors */
        if(fus_lexer_got(lexer, "doors")){
            err = fus_lexer_next(lexer);
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;

            while(1){
                if(fus_lexer_got(lexer, ")"))break;

                err = fus_lexer_get(lexer, "(");
                if(err)return err;

                ARRAY_PUSH_NEW(hexmap_door_t*, submap->doors, door)
                err = hexmap_parse_door(map, submap, door, lexer);
                if(err)return err;

                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }
            err = fus_lexer_next(lexer);
            if(err)return err;

            err = hexmap_populate_submap_doors(map, submap);
            if(err)return err;
        }
    }

    if(fus_lexer_got(lexer, "recordings")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            char *filename;
            char *palmapper_name = NULL;
            int frame_offset = 0;
            err = hexmap_parse_recording(lexer,
                &filename, &palmapper_name, &frame_offset);
            if(err)return err;

            ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
                recording)
            err = hexmap_recording_init(recording,
                HEXMAP_RECORDING_TYPE_RECORDING,
                filename, palmapper_name, frame_offset);
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    if(fus_lexer_got(lexer, "submaps")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = hexmap_parse_submap(map, lexer, true, pos,
                camera_pos, camera_type, mapper,
                palette_filename, tileset_filename);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
        err = fus_lexer_next(lexer);
        if(err)return err;
    }

    return 0;
}

int hexmap_get_submap_index(hexmap_t *map, hexmap_submap_t *submap){
    for(int i = 0; i < map->submaps_len; i++){
        if(map->submaps[i] == submap)return i;
    }
    return -1;
}

int hexmap_load_recording(hexmap_t *map, const char *filename,
    palettemapper_t *palmapper, bool loop, int offset, trf_t *trf
){
    int err;

    hexgame_t *game = map->game;

    ARRAY_PUSH_NEW(body_t*, map->bodies, body)
    err = body_init(body, game, map, NULL, NULL, palmapper);
    if(err)return err;

    err = recording_load(&body->recording, filename, NULL, body, loop);
    if(err)return err;

    body->recording.offset += offset;

    if(trf){
        vecspace_t *space = map->space;
        if(trf->flip)body->recording.turn0 = !body->recording.turn0;
        body->recording.rot0 = rot_rot(space->rot_max,
            body->recording.rot0, trf->rot);
        vec_add(space->dims, body->recording.pos0, trf->add);
    }

    err = body_play_recording(body);
    if(err)return err;

    return 0;
}

static int hexmap_collide_elem(hexmap_t *map, int all_type,
    int x, int y, trf_t *trf,
    hexcollmap_elem_t *elems2, int rot,
    void (*normalize_elem)(trf_t *index),
    hexcollmap_elem_t *(get_elem)(
        hexcollmap_t *collmap, trf_t *index),
    hexmap_collision_t *collision
){
    /* Returns true (1) or false (0), or 2 if caller should continue
    checking for a collision. */

    vecspace_t *space = map->space;

    for(int r2 = 0; r2 < rot; r2++){
        if(!hexcollmap_elem_is_solid(&elems2[r2]))continue;

        trf_t index;
        hexspace_set(index.add, x, y);
        index.rot = r2;
        index.flip = false;

        /* And now, because we were fools and defined */
        /* the tile coords such that their Y is flipped */
        /* compared to vecspaces, we need to flip that Y */
        /* before calling trf_apply and then flip it back */
        /* again: */
        index.add[1] = -index.add[1];
        trf_apply(space, &index, trf);
        index.add[1] = -index.add[1];
        normalize_elem(&index);

        bool collide = false;
        for(int i = 0; i < map->submaps_len; i++){
            hexmap_submap_t *submap = map->submaps[i];
            if(!submap->solid)continue;

            hexcollmap_t *collmap1 = &submap->collmap;

            trf_t subindex;
            hexspace_set(subindex.add,
                index.add[0] - submap->pos[0],
                index.add[1] + submap->pos[1]);
            subindex.rot = index.rot;
            subindex.flip = index.flip;

            hexcollmap_elem_t *elem = get_elem(collmap1, &subindex);
            if(elem != NULL){
                hexmap_collision_elem_t *collision_elem = NULL;

                if(elem->tile_c == 'S')collision_elem = &collision->savepoint;
                else if(elem->tile_c == 'D')collision_elem = &collision->door;
                else if(elem->tile_c == 'w')collision_elem = &collision->water;

                if(collision_elem){
                    collision_elem->submap = submap;
                    collision_elem->elem = elem;
                }
            }
            if(hexcollmap_elem_is_solid(elem)){
                collide = true; break;}
        }
        if(all_type != 2){
            bool all = all_type;
            if((all && !collide) || (!all && collide))return collide;
        }else{
            /* Just looking for savepoints & doors... */
        }
    }
    return 2; /* Caller should keep looking for a collision */
}

static void hexmap_collision_elem_init(hexmap_collision_elem_t *elem){
    elem->submap = NULL;
    elem->elem = NULL;
}

static void hexmap_collision_init(hexmap_collision_t *collision){
    hexmap_collision_elem_init(&collision->savepoint);
    hexmap_collision_elem_init(&collision->water);
    hexmap_collision_elem_init(&collision->door);
}

static bool _hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, int all_type, hexmap_collision_t *collision
){
    hexmap_collision_init(collision);

    int ox2 = collmap2->ox;
    int oy2 = collmap2->oy;
    int w2 = collmap2->w;
    int h2 = collmap2->h;

    vecspace_t *space = map->space;

    /* NOTE: for tile coords (ox, oy, x, y, w, h),
    Y is reversed (down is positive, up is negative) */

    for(int y2 = 0; y2 < h2; y2++){
        for(int x2 = 0; x2 < w2; x2++){
            hexcollmap_tile_t *tile2 = &collmap2->tiles[y2 * w2 + x2];

            int collide;
            int x = x2 - ox2;
            int y = y2 - oy2;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->vert, 1,
                hexcollmap_normalize_vert,
                hexcollmap_get_vert,
                collision);
            if(collide != 2)return collide;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->edge, 3,
                hexcollmap_normalize_edge,
                hexcollmap_get_edge,
                collision);
            if(collide != 2)return collide;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->face, 2,
                hexcollmap_normalize_face,
                hexcollmap_get_face,
                collision);
            if(collide != 2)return collide;
        }
    }
    if(all_type == 2){
        /* Return value doesn't matter, we were just looking for
        savepoints & doors */
        return false;
    }else{
        bool all = all_type;
        if(all)return true;
        else return false;
    }
}

bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all
){
    hexmap_collision_t collision;
    return _hexmap_collide(map, collmap2, trf, all, &collision);
}

void hexmap_collide_special(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, hexmap_collision_t *collision
){
    int all_type = 2;
    _hexmap_collide(map, collmap2, trf, all_type, collision);
}


int hexmap_step(hexmap_t *map){
    int err;

    hexgame_t *game = map->game;
    vecspace_t *space = game->space;

    /* Collide bodies with each other */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];
        if(body->state == NULL)continue;
        hexcollmap_t *hitbox = body->state->hitbox;
        if(hitbox == NULL)continue;

        trf_t hitbox_trf;
        body_init_trf(body, &hitbox_trf);

        /* This body has a hitbox! So collide it against all other bodies'
        hitboxes. */
        for(int j = i + 1; j < map->bodies_len; j++){
            body_t *body_other = map->bodies[j];
            if(body_other->state == NULL)continue;
            hexcollmap_t *hitbox_other = body_other->state->hitbox;
            if(hitbox_other == NULL)continue;

            trf_t hitbox_other_trf;
            body_init_trf(body_other, &hitbox_other_trf);

            /* The other body has a hitbox! Do the collision... */
            bool collide = hexcollmap_collide(hitbox, &hitbox_trf,
                hitbox_other, &hitbox_other_trf, space, false);
            if(collide){
                /* There was a collision!
                Now we find out who was right... and who was dead. */
                err = body_collide_against_body(body, body_other);
                if(err)return err;
                err = body_collide_against_body(body_other, body);
                if(err)return err;
            }
        }
    }

    /* Do 1 gameplay step for each body */
    for(int i = 0; i < map->bodies_len; i++){
        body_t *body = map->bodies[i];
        err = body_step(body, game);
        if(err)return err;
    }

    return 0;
}



/*****************
 * HEXMAP SUBMAP *
 *****************/

const char *submap_camera_type_msg(int camera_type){
    switch(camera_type){
        case 0: return "Use camera_pos";
        case 1: return "Follow player";
        default: return "Unkown";
    }
}

void hexmap_door_cleanup(hexmap_door_t *door){
    switch(door->type){
        case HEXMAP_DOOR_TYPE_RESPAWN:
        case HEXMAP_DOOR_TYPE_NEW_GAME:
            location_cleanup(&door->u.location);
            break;
        case HEXMAP_DOOR_TYPE_CAMERA_MAPPER:
            free(door->u.s);
            break;
        default:
            break;
    }
}

void hexmap_submap_cleanup(hexmap_submap_t *submap){
    free(submap->filename);
    hexcollmap_cleanup(&submap->collmap);
    palette_cleanup(&submap->palette);
    hexmap_tileset_cleanup(&submap->tileset);
    ARRAY_FREE_PTR(hexmap_door_t*, submap->doors, hexmap_door_cleanup)
}

int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, bool solid, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename
){
    int err;

    submap->map = map;

    submap->filename = filename;
    vec_cpy(MAX_VEC_DIMS, submap->pos, pos);

    submap->solid = solid;

    submap->camera_type = camera_type;
    vec_cpy(MAX_VEC_DIMS, submap->camera_pos, camera_pos);

    hexcollmap_init(&submap->collmap, map->space, strdup(filename));

    submap->rgraph_map = NULL;
    submap->rgraph_minimap = NULL;
    submap->mapper = mapper;

    err = palette_load(&submap->palette, palette_filename, NULL);
    if(err)return err;

    err = hexmap_tileset_load(&submap->tileset, map->prend,
        tileset_filename, NULL);
    if(err)return err;

    ARRAY_INIT(submap->doors)

    return 0;
}

hexmap_door_t *hexmap_submap_get_door(hexmap_submap_t *submap,
    hexcollmap_elem_t *elem
){
    for(int i = 0; i < submap->doors_len; i++){
        hexmap_door_t *door = submap->doors[i];
        if(door->elem == elem)return door;
    }
    return NULL;
}
