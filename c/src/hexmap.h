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
    hexcollmap_tile_t *tiles;
} hexcollmap_t;

typedef struct hexmap {
    char *name;
    hexcollmap_t collmap;
    prismelrenderer_t *prend;
    vec_t unit;
    rendergraph_t *rgraph_vert;
    rendergraph_t *rgraph_edge;
    rendergraph_t *rgraph_face;
    rendergraph_t *rgraph_map;
} hexmap_t;


void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer);
bool hexcollmap_collide(hexcollmap_t *map1, hexcollmap_t *map2,
    trf_t *trf, bool all);


void hexmap_cleanup(hexmap_t *map);
int hexmap_init(hexmap_t *map, char *name,
    prismelrenderer_t *prend,
    vec_t unit,
    rendergraph_t *rgraph_vert,
    rendergraph_t *rgraph_edge,
    rendergraph_t *rgraph_face);
int hexmap_create_rgraph(hexmap_t *map, rendergraph_t **rgraph_ptr);
int hexmap_load(hexmap_t *map, prismelrenderer_t *prend,
    const char *filename);
int hexmap_parse(hexmap_t *map, prismelrenderer_t *prend, char *name,
    fus_lexer_t *lexer);


#endif