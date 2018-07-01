#ifndef _HEXMAP_H_
#define _HEXMAP_H_

#include <stdio.h>

#include "array.h"
#include "lexer.h"
#include "geom.h"
#include "prismelrenderer.h"


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

typedef struct hexmap_submap {
    vec_t pos;
    vec_t camera_pos;
    char *filename;
    hexcollmap_t collmap;
    rendergraph_t *rgraph_map;
} hexmap_submap_t;

typedef struct hexmap_rgraph_elem {
    char tile_c;
        /* see hexcollmap_elem->tile_c */
    rendergraph_t *rgraph;
} hexmap_rgraph_elem_t;

typedef struct hexmap {
    char *name;
    vecspace_t *space;
    prismelrenderer_t *prend;
    prismelmapper_t *mapper;
    vec_t unit;

    ARRAY_DECL(hexmap_rgraph_elem_t, rgraph_verts)
    ARRAY_DECL(hexmap_rgraph_elem_t, rgraph_edges)
    ARRAY_DECL(hexmap_rgraph_elem_t, rgraph_faces)

    ARRAY_DECL(hexmap_submap_t, submaps)
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
bool hexcollmap_elem_is_solid(hexcollmap_elem_t *elem);



void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, char *name, vecspace_t *space,
    prismelrenderer_t *prend,
    prismelmapper_t *mapper,
    vec_t unit);
int hexmap_load(hexmap_t *map, prismelrenderer_t *prend,
    const char *filename);
int hexmap_parse(hexmap_t *map, prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer);
int hexmap_parse_area(hexmap_t *map, fus_lexer_t *lexer);
bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all);
rendergraph_t *hexmap_get_rgraph_vert(hexmap_t *map, char tile_c);
rendergraph_t *hexmap_get_rgraph_edge(hexmap_t *map, char tile_c);
rendergraph_t *hexmap_get_rgraph_face(hexmap_t *map, char tile_c);


void hexmap_submap_cleanup(hexmap_submap_t *submap);
int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, vec_t pos, vec_t camera_pos);
int hexmap_submap_load(hexmap_t *map, hexmap_submap_t *submap,
    const char *filename, vec_t pos, vec_t camera_pos);
int hexmap_submap_parse(hexmap_t *map, hexmap_submap_t *submap,
    fus_lexer_t *lexer);
int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap);


#endif