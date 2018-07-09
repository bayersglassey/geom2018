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
    vec_t respawn_pos;
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

    int recording_action;
        /* 0: none, 1: play, 2: record */
    char *recording;
    int recording_i;
    int recording_size;
    int recording_wait;
    char *recording_name;
    FILE *recording_file;
} player_t;

void player_cleanup(player_t *player);
int player_init(player_t *player, prismelrenderer_t *prend,
    char *stateset_filename, const char *state_name, int keymap,
    vec_t respawn_pos);
void player_keydown(player_t *player, int key_i);
void player_keyup(player_t *player, int key_i);
int player_get_key_i(player_t *player, char c);
char player_get_key_c(player_t *player, int key_i, bool absolute);
rot_t player_get_rot(player_t *player, const vecspace_t *space);
int player_process_event(player_t *player, SDL_Event *event);
int player_step(player_t *player, struct hexmap *map);

int player_play_recording(player_t *player, char *data, char *filename);
int player_start_recording(player_t *player, char *name);
int player_stop_recording(player_t *player);
int player_record(player_t *player, const char *data);
int player_recording_step(player_t *player);



/***********
 * HEXGAME *
 ***********/

typedef struct hexgame {
    int frame_i;
    bool zoomout;
    bool follow;
    char *respawn_filename;
    hexmap_t *map;
    hexmap_submap_t *cur_submap;
    vec_t camera_pos;
    rot_t camera_rot;
    ARRAY_DECL(player_t, players)
} hexgame_t;


void hexgame_cleanup(hexgame_t *game);
int hexgame_init(hexgame_t *game, hexmap_t *map, char *respawn_filename);
int hexgame_load_player_recording(hexgame_t *game, const char *filename,
    int keymap);
int hexgame_process_event(hexgame_t *game, SDL_Event *event);
int hexgame_step(hexgame_t *game);
int hexgame_render(hexgame_t *game,
    SDL_Renderer *renderer, SDL_Surface *surface,
    SDL_Palette *pal, int x0, int y0, int zoom);


#endif