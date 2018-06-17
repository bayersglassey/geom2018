#ifndef _HEXGAME_H_
#define _HEXGAME_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "anim.h"
#include "hexmap.h"
#include "prismelrenderer.h"
#include "geom.h"
#include "vec4.h"
#include "array.h"



/**********
 * PLAYER *
 **********/

#define PLAYER_KEY_U  0
#define PLAYER_KEY_D  1
#define PLAYER_KEY_L  2
#define PLAYER_KEY_R  3
#define PLAYER_KEYS   4

typedef struct player {
    rendergraph_t *rgraph;
    vec_t pos;
    rot_t rot;
    bool turn;
    SDL_Keycode key_code[PLAYER_KEYS];
    bool key_wentdown[PLAYER_KEYS];
    bool key_isdown[PLAYER_KEYS];
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player, rendergraph_t *rgraph, int keymap);



/***********
 * HEXGAME *
 ***********/

typedef struct hexgame {
    stateset_t *stateset;
    hexmap_t *map;
    ARRAY_DECL(player_t, players)
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, stateset_t *stateset, hexmap_t *map);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_step(hexgame_t *game);

struct test_app; /* TODO: Get rid of this dependency */
int hexgame_render(hexgame_t *game, struct test_app *app);


#endif