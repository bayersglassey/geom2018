#ifndef _HEXMAP_H_
#define _HEXMAP_H_

#include <stdio.h>

#include "array.h"
#include "lexer.h"
#include "geom.h"
#include "prismelrenderer.h"


typedef struct hexcollmap_tile {
    bool vert[1];
    bool edge[3];
    bool face[2];
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
    char *filename;
    hexcollmap_t collmap;
    rendergraph_t *rgraph_map;
} hexmap_submap_t;

typedef struct hexmap {
    char *name;
    vecspace_t *space;
    prismelrenderer_t *prend;
    prismelmapper_t *mapper;
    vec_t unit;
    rendergraph_t *rgraph_vert;
    rendergraph_t *rgraph_edge;
    rendergraph_t *rgraph_face;

    ARRAY_DECL(struct hexmap_submap, submaps)
} hexmap_t;


void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap, vecspace_t *space);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer);


void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, char *name, vecspace_t *space,
    prismelrenderer_t *prend,
    prismelmapper_t *mapper,
    vec_t unit,
    rendergraph_t *rgraph_vert,
    rendergraph_t *rgraph_edge,
    rendergraph_t *rgraph_face);
int hexmap_load(hexmap_t *map, prismelrenderer_t *prend,
    const char *filename);
int hexmap_parse(hexmap_t *map, prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer);
bool hexmap_collide(hexmap_t *map, hexcollmap_t *collmap2,
    trf_t *trf, bool all);


void hexmap_submap_cleanup(hexmap_submap_t *submap);
int hexmap_submap_init(hexmap_t *map, hexmap_submap_t *submap,
    char *filename, vec_t pos);
int hexmap_submap_load(hexmap_t *map, hexmap_submap_t *submap,
    const char *filename, vec_t pos);
int hexmap_submap_parse(hexmap_t *map, hexmap_submap_t *submap,
    fus_lexer_t *lexer);
int hexmap_submap_create_rgraph(hexmap_t *map, hexmap_submap_t *submap);


#endif