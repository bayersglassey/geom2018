#ifndef _HEXMAP_H_
#define _HEXMAP_H_

#include <stdio.h>

#include "array.h"
#include "lexer.h"
#include "geom.h"
#include "prismelrenderer.h"


/************
 * LOCATION *
 ************/

typedef struct location {
    vec_t pos;
    rot_t rot;
    bool turn;
    char *map_filename;
} location_t;

void location_init(location_t *location);
void location_cleanup(location_t *location);
void location_set(location_t *location, vecspace_t *space,
    vec_t pos, rot_t rot, bool turn, char *map_filename);


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
    rendergraph_t *rgraphs[3];
} hexmap_tileset_entry_t;

typedef struct hexmap_tileset {
    char *name;
    ARRAY_DECL(hexmap_tileset_entry_t*, vert_entries)
    ARRAY_DECL(hexmap_tileset_entry_t*, edge_entries)
    ARRAY_DECL(hexmap_tileset_entry_t*, face_entries)
} hexmap_tileset_t;

void hexmap_tileset_cleanup(hexmap_tileset_t *tileset);
int hexmap_tileset_init(hexmap_tileset_t *tileset, char *name);
int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename);



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
};

typedef struct hexcollmap_part {
    char part_c;
    char *filename;
    int type; /* enum hexcollmap_part_type */
} hexcollmap_part_t;

typedef struct hexcollmap {
    char *name;
    int w;
    int h;
    int ox;
    int oy;
    vecspace_t *space;
    hexcollmap_tile_t *tiles;
} hexcollmap_t;


int hexcollmap_part_init(hexcollmap_part_t *part,
    char part_c, char *filename, int type);
void hexcollmap_part_cleanup(hexcollmap_part_t *part);

void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space,
    char *name);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer,
    bool just_coll);
int hexcollmap_load(hexcollmap_t *collmap, const char *filename);
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
    rendergraph_t *rgraph_map;
    prismelmapper_t *mapper;
    palette_t palette;
    hexmap_tileset_t tileset;

    vec_t door_pos;
    rot_t door_rot;
    bool door_turn;
    char *door_map_filename;
    char *door_anim_filename;
} hexmap_submap_t;

enum hexmap_recording_type {
    HEXMAP_RECORDING_TYPE_RECORDING,
    HEXMAP_RECORDING_TYPE_ACTOR
};

typedef struct hexmap_recording {
    int type; /* enum hexmap_recording_type */
    char *filename;
    palettemapper_t *palmapper;
    trf_t trf;
} hexmap_recording_t;

typedef struct hexmap {
    char *name;
    struct hexgame *game;
    vecspace_t *space; /* Always hexspace! */
    vec_t spawn;

    prismelrenderer_t *prend;
    vec_t unit;

    ARRAY_DECL(struct body*, bodies)
    ARRAY_DECL(hexmap_submap_t*, submaps)
    ARRAY_DECL(hexmap_recording_t*, recordings)
} hexmap_t;


void hexmap_recording_cleanup(hexmap_recording_t *recording);
int hexmap_recording_init(hexmap_recording_t *recording, int type,
    char *filename, palettemapper_t *palmapper);

void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, struct hexgame *game, char *name,
    vec_t unit);
int hexmap_load(hexmap_t *map, struct hexgame *game, const char *filename);
int hexmap_parse(hexmap_t *map, struct hexgame *game, char *name,
    fus_lexer_t *lexer);
int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer, bool solid,
    vec_t parent_pos, vec_t parent_camera_pos, int parent_camera_type,
    prismelmapper_t *parent_mapper, char *palette_filename,
    char *tileset_filename);
int hexmap_load_recording(hexmap_t *map, const char *filename,
    palettemapper_t *palmapper, bool loop);
bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all);
void hexmap_collide_special(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf,
    hexmap_submap_t **collide_savepoint_ptr,
    hexmap_submap_t **collide_door_ptr,
    hexmap_submap_t **collide_water_ptr);
int hexmap_step(hexmap_t *map);

void hexmap_submap_cleanup(hexmap_submap_t *submap);
int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, bool solid, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename);
int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap);


#endif