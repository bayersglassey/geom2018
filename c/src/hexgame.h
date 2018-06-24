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
    vec_t pos;
    rot_t rot;
    bool turn;
    SDL_Keycode key_code[PLAYER_KEYS];
    bool key_isdown[PLAYER_KEYS];
    bool key_wasdown[PLAYER_KEYS];
    bool key_wentdown[PLAYER_KEYS];

    stateset_t stateset;
    state_t *state;
    int frame_i;
    int cooldown;
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player, prismelrenderer_t *prend,
    char *stateset_filename, int keymap);
rot_t player_get_rot(player_t *player, const vecspace_t *space);

struct hexgame;
int player_step(player_t *player, struct hexgame *game);



/***********
 * HEXGAME *
 ***********/

typedef struct hexgame {
    hexmap_t *map;
    vec_t camera_pos;
    ARRAY_DECL(player_t, players)
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, hexmap_t *map);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_step(hexgame_t *game);

struct test_app; /* TODO: Get rid of this dependency */
int hexgame_render(hexgame_t *game, struct test_app *app);


#endif