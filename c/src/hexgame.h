#ifndef _HEXGAME_H_
#define _HEXGAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "hexcollmap.h"
#include "prismelrenderer.h"
#include "geom.h"
#include "vec4.h"
#include "array.h"



/**********
 * PLAYER *
 **********/

#define PLAYER_KEYS  4

typedef struct player {
    rendergraph_t *rgraph;
    vec_t pos;
    rot_t rot;
    bool turn;
    int key_state[PLAYER_KEYS];
        /* 0: key up, 1: key down, 2: key went down this frame */
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player);



/***********
 * HEXGAME *
 ***********/

typedef struct hexgame {
    stateset_t *stateset;
    hexcollmap_t *map_collmap;
    rendergraph_t *map_rgraph;
    ARRAY_DECL(player_t, players)
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, stateset_t *stateset,
    hexcollmap_t *map_collmap, rendergraph_t *map_rgraph);
bool hexgame_ready(hexgame_t *game);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_step(hexgame_t *game);
int hexgame_render(hexgame_t *game, SDL_Renderer *renderer);


#endif