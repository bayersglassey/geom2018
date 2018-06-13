#ifndef _HEXCOLLMAP_H_
#define _HEXCOLLMAP_H_


typedef struct hexcolltile {
    bool vert[1];
    bool edge[3];
    bool face[2];
} hexcolltile_t;

typedef struct hexcollmap {
    char *name;
    int w;
    int h;
    hexcolltile_t *data;
} hexcollmap_t;

void hexcollmap_cleanup(hexcollmap_t *collmap);
int hexcollmap_init(hexcollmap_t *collmap);




#endif