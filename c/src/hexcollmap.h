#ifndef _HEXCOLLMAP_H_
#define _HEXCOLLMAP_H_

#include <stdio.h>


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

void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap, char *name);
void hexcollmap_dump(hexcollmap_t *collmap, FILE *f, int n_spaces);
int hexcollmap_parse_lines(hexcollmap_t *collmap,
    char **lines, int lines_len);


#endif