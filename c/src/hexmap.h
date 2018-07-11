#ifndef _HEXMAP_H_
#define _HEXMAP_H_

#include <stdio.h>

#include "array.h"
#include "lexer.h"
#include "geom.h"
#include "prismelrenderer.h"


/******************
 * HEXMAP TILESET *
 ******************/

typedef struct hexmap_tileset_entry {
    char tile_c;
        /* see hexcollmap_elem->tile_c */
    rendergraph_t *rgraph;
} hexmap_tileset_entry_t;

typedef struct hexmap_tileset {
    char *name;
    ARRAY_DECL(hexmap_tileset_entry_t, vert_entries)
    ARRAY_DECL(hexmap_tileset_entry_t, edge_entries)
    ARRAY_DECL(hexmap_tileset_entry_t, face_entries)
} hexmap_tileset_t;

void hexmap_tileset_cleanup(hexmap_tileset_t *tileset);
int hexmap_tileset_init(hexmap_tileset_t *tileset, char *name);
int hexmap_tileset_load(hexmap_tileset_t *tileset,
    prismelrenderer_t *prend, const char *filename);
rendergraph_t *hexmap_tileset_get_rgraph_vert(hexmap_tileset_t *tileset,
    char tile_c);
rendergraph_t *hexmap_tileset_get_rgraph_edge(hexmap_tileset_t *tileset,
    char tile_c);
rendergraph_t *hexmap_tileset_get_rgraph_face(hexmap_tileset_t *tileset,
    char tile_c);



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
} hexcollmap_elem_t;

typedef struct hexcollmap_tile {
    hexcollmap_elem_t vert[1];
    hexcollmap_elem_t edge[3];
    hexcollmap_elem_t face[2];
} hexcollmap_tile_t;

typedef struct hexcollmap {
    int w;
    int h;
    int ox;
    int oy;
    vecspace_t *space;
    hexcollmap_tile_t *tiles;
} hexcollmap_t;



/**********
 * HEXMAP *
 **********/

typedef struct hexmap_submap {
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
} hexmap_submap_t;

typedef struct hexmap {
    char *name;
    vecspace_t *space;
    prismelrenderer_t *prend;
    vec_t unit;
    vec_t spawn;

    ARRAY_DECL(hexmap_submap_t, submaps)

    ARRAY_DECL(char, recording_filenames)
} hexmap_t;


void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer);
void hexcollmap_normalize_vert(trf_t *index);
void hexcollmap_normalize_edge(trf_t *index);
void hexcollmap_normalize_face(trf_t *index);
hexcollmap_tile_t *hexcollmap_get_tile(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_vert(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_edge(hexcollmap_t *collmap, trf_t *index);
hexcollmap_elem_t *hexcollmap_get_face(hexcollmap_t *collmap, trf_t *index);
bool hexcollmap_elem_is_visible(hexcollmap_elem_t *elem);
bool hexcollmap_elem_is_solid(hexcollmap_elem_t *elem);



void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, char *name, vecspace_t *space,
    prismelrenderer_t *prend,
    vec_t unit);
int hexmap_load(hexmap_t *map, prismelrenderer_t *prend,
    const char *filename);
int hexmap_parse(hexmap_t *map, prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer);
int hexmap_parse_submap(hexmap_t *map, fus_lexer_t *lexer,
    vec_t parent_pos, vec_t parent_camera_pos, int parent_camera_type,
    prismelmapper_t *parent_mapper, char *palette_filename,
    char *tileset_filename);
bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all);


void hexmap_submap_cleanup(hexmap_submap_t *submap);
int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename);
int hexmap_submap_load(hexmap_t *map, hexmap_submap_t *submap,
    const char *filename, vec_t pos, int camera_type, vec_t camera_pos,
    prismelmapper_t *mapper, char *palette_filename, char *tileset_filename);
int hexmap_submap_parse(hexmap_t *map, hexmap_submap_t *submap,
    fus_lexer_t *lexer);
int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap);


#endif