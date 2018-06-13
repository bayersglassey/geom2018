#ifndef _HEXCOLLMAP_H_
#define _HEXCOLLMAP_H_

#include <stdio.h>

#include "array.h"
#include "lexer.h"


typedef struct hexcollmap_tile {
    bool vert[1];
    bool edge[3];
    bool face[2];
} hexcollmap_tile_t;

typedef struct hexcollmap {
    char *name;
    int w;
    int h;
    int ox;
    int oy;
    hexcollmap_tile_t *tiles;
} hexcollmap_t;

typedef struct hexcollmapset {
    ARRAY_DECL(struct hexcollmap, collmaps)
} hexcollmapset_t;


void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap, char *name);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces);
int hexcollmap_parse(hexcollmap_t *collmap, fus_lexer_t *lexer);
int hexcollmap_parse_lines(hexcollmap_t *collmap,
    char **lines, int lines_len);


void hexcollmapset_cleanup(hexcollmapset_t *collmapset);
int hexcollmapset_init(hexcollmapset_t *collmapset);
void hexcollmapset_dump(hexcollmapset_t *collmapset, FILE *f);
int hexcollmapset_load(hexcollmapset_t *collmapset, const char *filename);
hexcollmap_t *hexcollmapset_get_collmap(hexcollmapset_t *collmapset,
    const char *name);


#endif