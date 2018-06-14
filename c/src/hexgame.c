

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "hexgame.h"
#include "anim.h"
#include "hexcollmap.h"
#include "prismelrenderer.h"
#include "array.h"



/**********
 * PLAYER *
 **********/

void player_cleanup(player_t *player){
}

int player_init(player_t *player){
    return 0;
}


/***********
 * HEXGAME *
 ***********/


void hexgame_cleanup(hexgame_t *game){
    /* Leave cleanup of game->stateset, game->map_collmap, game->map_rgraph
    to caller */
    ARRAY_FREE(player_t, *game, players, player_cleanup)
}

int hexgame_init(hexgame_t *game, stateset_t *stateset,
    hexcollmap_t *map_collmap, rendergraph_t *map_rgraph
){
    game->stateset = stateset;
    game->map_collmap = map_collmap;
    game->map_rgraph = map_rgraph;
    ARRAY_INIT(*game, players)
    return 0;
}

bool hexgame_ready(hexgame_t *game){
    /* If we got any NULLs, game should refuse to start */
    return game->stateset && game->map_collmap && game->map_rgraph;
}

int hexgame_mainloop(hexgame_t *game){
    if(!hexgame_ready(game)){
        fprintf(stderr, "Game not ready, refusing to start\n");
        return 0;}
    return 0;
}

