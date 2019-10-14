

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
#include "prismelrenderer.h"


/************
 * LOCATION *
 ************/

void location_init(location_t *location){
    vec_zero(location->pos);
    location->rot = 0;
    location->turn = false;
    location->map_filename = NULL;
}

void location_cleanup(location_t *location){
    free(location->map_filename);
}

void location_set(location_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, char *map_filename
){
    vec_cpy(space->dims, location->pos, pos);
    location->rot = rot;
    location->turn = turn;
    if(location->map_filename != map_filename){
        free(location->map_filename);
        location->map_filename = map_filename;
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
    prismelrenderer_t *prend, const char *filename
){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
    if(err)return err;

    err = hexmap_tileset_parse(tileset, prend, strdup(filename),
        &lexer);
    if(err)return err;

    free(text);
    return 0;
}

#define HEXMAP_TILESET_GET_RGRAPH(TYPE) \
    void hexmap_tileset_get_rgraph_##TYPE(hexmap_tileset_t *tileset, \
        char tile_c, rot_t rot, \
        rendergraph_t **rgraph_ptr, bool *rot_ok_ptr, \
        int *frame_offset_ptr \
    ){ \
        /* rgraph: non-NULL if we found an entry with matching tile_c */ \
        /* rot_ok: whether the entry we found had an rgraph for the */ \
        /* requested rot; if false, rgraph is the entry's rgraph for */ \
        /* rot=0 and caller should rotate the rgraph manually */ \
        bool rot_ok = false; \
        int frame_offset = 0; \
        rendergraph_t *rgraph = NULL; \
        for(int i = 0; i < tileset->TYPE##_entries_len; i++){ \
            hexmap_tileset_entry_t *entry = tileset->TYPE##_entries[i]; \
            if(entry->tile_c != tile_c)continue; \
            if(entry->n_rgraphs > rot){ \
                rot_ok = true; \
                rgraph = entry->rgraphs[rot]; \
            }else rgraph = entry->rgraphs[0]; \
            frame_offset = entry->frame_offset; \
            break; \
        } \
        *rgraph_ptr = rgraph; \
        *rot_ok_ptr = rot_ok; \
        *frame_offset_ptr = frame_offset; \
    }
HEXMAP_TILESET_GET_RGRAPH(vert)
HEXMAP_TILESET_GET_RGRAPH(edge)
HEXMAP_TILESET_GET_RGRAPH(face)



/********************
 * HEXMAP RECORDING *
 ********************/

void hexmap_recording_cleanup(hexmap_recording_t *recording){
    free(recording->filename);
    free(recording->palmapper_name);
}

int hexmap_recording_init(hexmap_recording_t *recording, int type,
    char *filename, char *palmapper_name
){
    recording->type = type;
    recording->filename = filename;
    recording->palmapper_name = palmapper_name;
    trf_zero(&recording->trf);
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
            palmapper, true, &trf);
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
    }else{
        fprintf(stderr, "%s: Unrecognized hexmap recording type: %i\n",
            __func__, recording->type);
        return 2;
    }

    return 0;
}

int hexmap_load(hexmap_t *map, hexgame_t *game, const char *filename){
    int err;
    fus_lexer_t lexer;

    char *text = load_file(filename);
    if(text == NULL)return 1;

    err = fus_lexer_init(&lexer, text, filename);
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
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        err = fus_lexer_get_str(lexer, &filename);
        if(err)return err;
        if(!fus_lexer_got(lexer, ")")){
            err = fus_lexer_get_str(lexer, &palmapper_name);
            if(err)return err;
        }
        err = fus_lexer_get(lexer, ")");
        if(err)return err;

        ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
            recording)
        err = hexmap_recording_init(recording,
            HEXMAP_RECORDING_TYPE_ACTOR,
            filename, palmapper_name);
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

    vec_t door_pos;
    vec_zero(door_pos);
    rot_t door_rot = 0;
    bool door_turn = false;
    char *door_map_filename = NULL;
    char *door_anim_filename = NULL;
    if(fus_lexer_got(lexer, "door")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        {
            if(fus_lexer_got(lexer, "map")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_str(lexer, &door_map_filename);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }else{
                /* Non-null door_map_filename indicates player should
                "teleport" to door_pos, door_rot, door_turn */
                door_map_filename = strdup(map->name);
            }

            if(fus_lexer_got(lexer, "anim")){
                err = fus_lexer_next(lexer);
                if(err)return err;
                err = fus_lexer_get(lexer, "(");
                if(err)return err;
                err = fus_lexer_get_str(lexer, &door_anim_filename);
                if(err)return err;
                err = fus_lexer_get(lexer, ")");
                if(err)return err;
            }

            err = fus_lexer_get(lexer, "pos");
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_vec(lexer, space, door_pos);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;

            err = fus_lexer_get(lexer, "rot");
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_int(lexer, &door_rot);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;

            err = fus_lexer_get(lexer, "turn");
            if(err)return err;
            err = fus_lexer_get(lexer, "(");
            if(err)return err;
            err = fus_lexer_get_yn(lexer, &door_turn);
            if(err)return err;
            err = fus_lexer_get(lexer, ")");
            if(err)return err;
        }
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
        ARRAY_PUSH_NEW(hexmap_submap_t*, map->submaps, submap)
        err = hexmap_submap_init(map, submap, strdup(submap_filename),
            solid, pos, camera_type, camera_pos, mapper,
            palette_filename, tileset_filename);
        if(err)return err;

        /* load collmap */
        err = hexcollmap_load(&submap->collmap, submap_filename);
        if(err)return err;

        /* render submap->rgraph_map */
        err = hexmap_submap_create_rgraph(map, submap);
        if(err)return err;

        /* door stuff */
        vec_cpy(space->dims, submap->door_pos, door_pos);
        submap->door_rot = door_rot;
        submap->door_turn = door_turn;
        submap->door_map_filename = door_map_filename;
        submap->door_anim_filename = door_anim_filename;
    }

    if(fus_lexer_got(lexer, "recordings")){
        err = fus_lexer_next(lexer);
        if(err)return err;
        err = fus_lexer_get(lexer, "(");
        if(err)return err;
        while(1){
            if(fus_lexer_got(lexer, ")"))break;

            err = fus_lexer_get(lexer, "(");
            if(err)return err;

            char *filename;
            char *palmapper_name = NULL;
            err = fus_lexer_get_str(lexer, &filename);
            if(err)return err;
            if(!fus_lexer_got(lexer, ")")){
                err = fus_lexer_get_str(lexer, &palmapper_name);
                if(err)return err;
            }

            ARRAY_PUSH_NEW(hexmap_recording_t*, map->recordings,
                recording)
            err = hexmap_recording_init(recording,
                HEXMAP_RECORDING_TYPE_RECORDING,
                filename, palmapper_name);
            if(err)return err;

            err = fus_lexer_get(lexer, ")");
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

int hexmap_load_recording(hexmap_t *map, const char *filename,
    palettemapper_t *palmapper, bool loop, trf_t *trf
){
    int err;

    hexgame_t *game = map->game;

    ARRAY_PUSH_NEW(body_t*, map->bodies, body)
    err = body_init(body, game, map, NULL, NULL, palmapper);
    if(err)return err;

    err = recording_load(&body->recording, filename, body, loop);
    if(err)return err;

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
    hexmap_submap_t **collide_savepoint_ptr,
    hexmap_submap_t **collide_door_ptr,
    hexmap_submap_t **collide_water_ptr
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
                if(elem->tile_c == 'S')*collide_savepoint_ptr = submap;
                if(elem->tile_c == 'D')*collide_door_ptr = submap;
                if(elem->tile_c == 'w')*collide_water_ptr = submap;
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

static bool _hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, int all_type,
    hexmap_submap_t **collide_savepoint_ptr,
    hexmap_submap_t **collide_door_ptr,
    hexmap_submap_t **collide_water_ptr
){

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
                collide_savepoint_ptr, collide_door_ptr,
                collide_water_ptr);
            if(collide != 2)return collide;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->edge, 3,
                hexcollmap_normalize_edge,
                hexcollmap_get_edge,
                collide_savepoint_ptr, collide_door_ptr,
                collide_water_ptr);
            if(collide != 2)return collide;
            collide = hexmap_collide_elem(map, all_type,
                x, y, trf,
                tile2->face, 2,
                hexcollmap_normalize_face,
                hexcollmap_get_face,
                collide_savepoint_ptr, collide_door_ptr,
                collide_water_ptr);
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
    hexmap_submap_t *collide_savepoint;
    hexmap_submap_t *collide_door;
    hexmap_submap_t *collide_water;
    return _hexmap_collide(map, collmap2, trf, all,
        &collide_savepoint, &collide_door, &collide_water);
}

void hexmap_collide_special(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf,
    hexmap_submap_t **collide_savepoint_ptr,
    hexmap_submap_t **collide_door_ptr,
    hexmap_submap_t **collide_water_ptr
){
    int all_type = 2;
    _hexmap_collide(map, collmap2, trf, all_type,
        collide_savepoint_ptr, collide_door_ptr, collide_water_ptr);
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

void hexmap_submap_cleanup(hexmap_submap_t *submap){
    free(submap->filename);
    free(submap->door_map_filename);
    free(submap->door_anim_filename);
    hexcollmap_cleanup(&submap->collmap);
    palette_cleanup(&submap->palette);
    hexmap_tileset_cleanup(&submap->tileset);
}

int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, bool solid, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename
){
    int err;

    submap->filename = filename;
    vec_cpy(MAX_VEC_DIMS, submap->pos, pos);

    submap->solid = solid;

    submap->camera_type = camera_type;
    vec_cpy(MAX_VEC_DIMS, submap->camera_pos, camera_pos);

    err = hexcollmap_init(&submap->collmap, map->space, strdup(filename));
    if(err)return err;

    submap->rgraph_map = NULL;
    submap->mapper = mapper;

    err = palette_load(&submap->palette, palette_filename);
    if(err)return err;

    err = hexmap_tileset_load(&submap->tileset, map->prend,
        tileset_filename);
    if(err)return err;

    vec_zero(submap->door_pos);
    submap->door_rot = 0;
    submap->door_turn = false;
    submap->door_map_filename = NULL;
    submap->door_anim_filename = NULL;

    return 0;
}

static int add_tile_rgraph(rendergraph_t *rgraph, rendergraph_t *rgraph2,
    vecspace_t *space, vec_t add, rot_t rot, int frame_i
){
    int err;
    rendergraph_trf_t *rendergraph_trf;
    err = rendergraph_push_rendergraph_trf(rgraph, &rendergraph_trf);
    if(err)return err;
    rendergraph_trf->rendergraph = rgraph2;
    rendergraph_trf->trf.rot = rot;
    rendergraph_trf->frame_i = frame_i;
    vec_cpy(space->dims, rendergraph_trf->trf.add, add);
    return 0;
}

int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap){
    int err;

    prismelrenderer_t *prend = map->prend;
    vecspace_t *space = prend->space;
    hexcollmap_t *collmap = &submap->collmap;

    /* BIG OL' HACK: If any "tile" rgraphs are animated, we need the
    map's rgraph to be animated also.
    The most correct way to do this is I guess to compute the LCD of
    the tile rgraphs' n_frames, and set the map's rgraph's n_frames
    to that.
    But for now we use a magic number which has "many" divisors.
    That's a lot of bitmaps to cache for the map's rgraph, though...
    if we're going to allow complicated map animations, maybe we
    should disable bitmap caching for it (somehow). */
    int n_frames = 24;

    ARRAY_PUSH_NEW(rendergraph_t*, prend->rendergraphs, rgraph)
    err = rendergraph_init(rgraph, strdup(submap->filename), prend, NULL,
        rendergraph_animation_type_default,
        n_frames);
    if(err)return err;

    hexmap_tileset_t *tileset = &submap->tileset;

    for(int y = 0; y < collmap->h; y++){
        for(int x = 0; x < collmap->w; x++){
            int px = x - collmap->ox;
            int py = y - collmap->oy;

            vec_t v;
            vec4_set(v, px + py, 0, -py, 0);
            vec_mul(space, v, map->unit);

            hexcollmap_tile_t *tile =
                &collmap->tiles[y * collmap->w + x];

            #define HEXMAP_ADD_TILE(PART, ROT) \
                for(int i = 0; i < ROT; i++){ \
                    hexcollmap_elem_t *elem = &tile->PART[i]; \
                    if(!hexcollmap_elem_is_visible(elem))continue; \
                    rendergraph_t *rgraph_tile; \
                    bool rot_ok; \
                    int frame_offset; \
                    hexmap_tileset_get_rgraph_##PART(tileset, \
                            elem->tile_c, i, \
                            &rgraph_tile, &rot_ok, &frame_offset); \
                    int frame_i = frame_offset? x: 0; \
                    if(rgraph_tile == NULL){ \
                        fprintf(stderr, "Couldn't find " #PART " tile " \
                            "for character: %c\n", elem->tile_c); \
                        return 2;} \
                    err = add_tile_rgraph(rgraph, rgraph_tile, \
                        space, v, rot_ok? 0: i * 2, frame_i); \
                    if(err)return err; \
                }
            HEXMAP_ADD_TILE(vert, 1)
            HEXMAP_ADD_TILE(edge, 3)
            HEXMAP_ADD_TILE(face, 2)
            #undef HEXMAP_ADD_TILE
        }
    }

    submap->rgraph_map = rgraph;
    return 0;
}

