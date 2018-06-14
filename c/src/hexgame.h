#ifndef _HEXGAME_H_
#define _HEXGAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "hexcollmap.h"
#include "prismelrenderer.h"


typedef struct hexgame {
    stateset_t *stateset;
    hexcollmap_t *map_collmap;
    rendergraph_t *map_rgraph;
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, stateset_t *stateset,
    hexcollmap_t *map_collmap, rendergraph_t *map_rgraph);
bool hexgame_ready(hexgame_t *game);
int hexgame_mainloop(hexgame_t *game);


#endif