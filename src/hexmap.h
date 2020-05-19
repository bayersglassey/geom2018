#ifndef _HEXMAP_H_
#define _HEXMAP_H_

#include <stdio.h>

#include "array.h"
#include "lexer.h"
#include "geom.h"
#include "location.h"
#include "prismelrenderer.h"


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
 * HEXMAP RECORDING *
 ********************/

/* ...that is, enough information to start a recording at a given
location on a hexmap... */

enum hexmap_recording_type {
    HEXMAP_RECORDING_TYPE_RECORDING,
    HEXMAP_RECORDING_TYPE_ACTOR
};

typedef struct hexmap_recording {
    int type; /* enum hexmap_recording_type */
    char *filename;
    char *palmapper_name;
    trf_t trf;
    int frame_offset;
} hexmap_recording_t;

void hexmap_recording_cleanup(hexmap_recording_t *recording);
int hexmap_recording_init(hexmap_recording_t *recording, int type,
    char *filename, char *palmapper_name, int frame_offset);


/**********************
 * HEXMAP RENDERGRAPH *
 **********************/

/* ...that is, enough information to display a rendergraph at a given
location on a hexmap... */

typedef struct hexmap_rendergraph {
    char *name;
    char *palmapper_name;
    trf_t trf;
        /* NOTE: trf is in hexspace! It's up to you to convert to
        prend->space once you actually have a prend and an rgraph! */
} hexmap_rendergraph_t;

void hexmap_rendergraph_cleanup(hexmap_rendergraph_t *rendergraph);
int hexmap_rendergraph_init(hexmap_rendergraph_t *rendergraph,
    char *name, char *palmapper_name);


/**************
 * HEXCOLLMAP *
 **************/

typedef struct hexcollmap_elem {
    char tile_c;
        /* Symbol representing the graphical tile (so, probably
        rendergraph) to use for this element.
        E.g. '0' might be the default, '1' a special version,
        'r', 'g', 'b' for red, green, blue versions...
        Should always be a printable character (in the sense of
        the standard library's isprint function), for debugging
        purposes.
        With that in mind, the default (no tile) value is ' '. */
    int z;
        /* z-depth: higher numbers overwrite lower ones */
} hexcollmap_elem_t;

typedef struct hexcollmap_tile {
    hexcollmap_elem_t vert[1];
    hexcollmap_elem_t edge[3];
    hexcollmap_elem_t face[2];
} hexcollmap_tile_t;

enum hexcollmap_part_type {
    HEXCOLLMAP_PART_TYPE_HEXCOLLMAP,
    HEXCOLLMAP_PART_TYPE_RECORDING,
    HEXCOLLMAP_PART_TYPE_RENDERGRAPH,
};

typedef struct hexcollmap_part {
    int type; /* enum hexcollmap_part_type */
    char part_c;
    char *filename;
    char *palmapper_name;
    int frame_offset;

    trf_t trf;
    int draw_z;
} hexcollmap_part_t;

typedef struct hexcollmap {
    char *name;
    int w;
    int h;
    int ox;
    int oy;
    hexcollmap_tile_t *tiles;
    ARRAY_DECL(hexmap_recording_t*, recordings)
    ARRAY_DECL(hexmap_rendergraph_t*, rendergraphs)

    /* Weakrefs */
    vecspace_t *space;
} hexcollmap_t;

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


int hexcollmap_part_init(hexcollmap_part_t *part, int type,
    char part_c, char *filename, char *palmapper_name, int frame_offset);
void hexcollmap_part_cleanup(hexcollmap_part_t *part);

void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    char *name);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer,
    bool just_coll);
int hexcollmap_load(hexcollmap_t *collmap, const char *filename,
    vars_t *vars);
void hexcollmap_normalize_vert(trf_t *index);
void hexcollmap_normalize_edge(trf_t *index);
void hexcollmap_normalize_face(trf_t *index);
hexcollmap_tile_t *hexcollmap_get_tile(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_vert(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_edge(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_face(hexcollmap_t *collmap, trf_t *index);
bool hexcollmap_elem_is_visible(hexcollmap_elem_t *elem);
bool hexcollmap_elem_is_solid(hexcollmap_elem_t *elem);
bool hexcollmap_collide(
    hexcollmap_t *collmap1, trf_t *trf1,
    hexcollmap_t *collmap2, trf_t *trf2,
    vecspace_t *space, bool all);


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

typedef struct hexmap_door {
    int type; /* enum hexmap_door_type */
    union {
        location_t location; /* type == HEXMAP_DOOR_TYPE_RESPAWN or HEXMAP_DOOR_TYPE_NEW_GAME */
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
    prismelmapper_t *mapper;
} hexmap_submap_t;

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
int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap);
hexmap_door_t *hexmap_submap_get_door(hexmap_submap_t *submap,
    hexcollmap_elem_t *elem);


#endif