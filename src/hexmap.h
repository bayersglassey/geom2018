#ifndef _HEXMAP_H_
#define _HEXMAP_H_

#include "array.h"
#include "lexer.h"
#include "geom.h"
#include "hexgame_location.h"
#include "prismelrenderer.h"
#include "hexcollmap.h"


/******************
 * HEXMAP TILESET *
 ******************/

typedef struct hexmap_tileset_entry {
    char tile_c;
        /* see hexcollmap_elem->tile_c */
    int n_rgraphs;
        /* Between 1 and 3 (index is a rot_t value). */
    int frame_offset;
        /* How tiles' positions should affect the frame offset of
        their animation.
        For now, should basically be treated as a bool (0/1). */

    /* Weakrefs */
    rendergraph_t *rgraphs[3];
} hexmap_tileset_entry_t;

typedef struct hexmap_tileset {
    char *name;

    /* Weakrefs */
    ARRAY_DECL(hexmap_tileset_entry_t*, vert_entries)
    ARRAY_DECL(hexmap_tileset_entry_t*, edge_entries)
    ARRAY_DECL(hexmap_tileset_entry_t*, face_entries)
} hexmap_tileset_t;

void hexmap_tileset_cleanup(hexmap_tileset_t *tileset);
int hexmap_tileset_init(hexmap_tileset_t *tileset, char *name);
int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename, vars_t *vars);


/********************
 * HEXMAP COLLISION *
 ********************/

typedef struct hexmap_collision_elem {
    /* Weakrefs */
    struct hexmap_submap *submap;
    hexcollmap_elem_t *elem;
} hexmap_collision_elem_t;

typedef struct hexmap_collision {
    hexmap_collision_elem_t savepoint;
    hexmap_collision_elem_t water;
    hexmap_collision_elem_t door;
} hexmap_collision_t;


/**********
 * HEXMAP *
 **********/

enum hexmap_door_type {
    HEXMAP_DOOR_TYPE_DUD,
    HEXMAP_DOOR_TYPE_RESPAWN,
    HEXMAP_DOOR_TYPE_NEW_GAME,
    HEXMAP_DOOR_TYPE_CONTINUE,
    HEXMAP_DOOR_TYPE_PLAYERS,
    HEXMAP_DOOR_TYPE_EXIT,
    HEXMAP_DOOR_TYPE_CAMERA_MAPPER,
    HEXMAP_DOOR_TYPES
};

const char *hexmap_door_type_msg(int door_type);

typedef struct hexmap_door {
    int type; /* enum hexmap_door_type */
    union {
        hexgame_savelocation_t location; /* type == HEXMAP_DOOR_TYPE_RESPAWN or HEXMAP_DOOR_TYPE_NEW_GAME */
        int n_players; /* type == HEXMAP_DOOR_TYPE_PLAYERS */
        char *s; /* type == HEXMAP_DOOR_TYPE_CAMERA_MAPPER */
    } u;

    /* Weakrefs */
    hexcollmap_elem_t *elem;
        /* We use this to mark door's position within its submap's
        collmap, which is kind of silly but works well so long as
        hexmap_collision_t also stores hexcollmap_elem_t* instead of
        actual positions. */
} hexmap_door_t;

typedef struct hexmap_submap {
    bool solid;
    vec_t pos;
    vec_t camera_pos;
    int camera_type;
        /*
            0: use camera_pos
            1: follow player
        */
    char *filename;
    hexcollmap_t collmap;
    palette_t palette;
    hexmap_tileset_t tileset;
    ARRAY_DECL(hexmap_door_t*, doors)

    /* Weakrefs */
    struct hexmap *map;
    rendergraph_t *rgraph_map;
    rendergraph_t *rgraph_minimap;
    prismelmapper_t *mapper;
} hexmap_submap_t;

const char *submap_camera_type_msg(int camera_type);

typedef struct hexmap {
    char *name;

    vec_t spawn;
    vec_t unit;

    ARRAY_DECL(struct body*, bodies)
    ARRAY_DECL(hexmap_submap_t*, submaps)
    ARRAY_DECL(hexmap_recording_t*, recordings)

    /* Weakrefs */
    struct hexgame *game;
    vecspace_t *space; /* Always hexspace! */
    prismelrenderer_t *prend;
} hexmap_t;


void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, struct hexgame *game, char *name,
    vec_t unit);
int hexmap_load(hexmap_t *map, struct hexgame *game, const char *filename,
    vars_t *vars);
int hexmap_parse(hexmap_t *map, struct hexgame *game, char *name,
    fus_lexer_t *lexer);
int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer, bool solid,
    vec_t parent_pos, vec_t parent_camera_pos, int parent_camera_type,
    prismelmapper_t *parent_mapper, char *palette_filename,
    char *tileset_filename);
int hexmap_get_submap_index(hexmap_t *map, hexmap_submap_t *submap);
int hexmap_load_recording(hexmap_t *map, const char *filename,
    palettemapper_t *palmapper, bool loop, int offset, trf_t *trf);
bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all);
void hexmap_collide_special(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf,
    hexmap_collision_t *collision);
int hexmap_step(hexmap_t *map);

void hexmap_door_cleanup(hexmap_door_t *door);
void hexmap_submap_cleanup(hexmap_submap_t *submap);
int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, bool solid, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename);
int hexmap_submap_create_rgraph_map(hexmap_submap_t *submap);
int hexmap_submap_create_rgraph_minimap(hexmap_submap_t *submap);
hexmap_door_t *hexmap_submap_get_door(hexmap_submap_t *submap,
    hexcollmap_elem_t *elem);


#endif